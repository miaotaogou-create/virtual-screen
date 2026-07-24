from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, tostring

from . import win_display

# Windows：隐藏子进程控制台，避免双击后闪黑框 / PowerShell 蓝框
_CREATE_NO_WINDOW = 0x08000000
_SW_HIDE = 0

NEFCON_URL = "https://github.com/nefarius/nefcon/releases/download/v1.14.0/nefcon_v1.14.0.zip"
DRIVER_URL = (
    "https://github.com/VirtualDrivers/Virtual-Display-Driver/releases/download/"
    "25.7.23/VirtualDisplayDriver-x86.Driver.Only.zip"
)

_device_cache: list[dict] | None = None
_device_cache_at = 0.0


def _run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    kwargs.setdefault("capture_output", True)
    kwargs.setdefault("text", True)
    kwargs.setdefault("encoding", "utf-8")
    kwargs.setdefault("errors", "replace")
    kwargs.setdefault("check", False)
    kwargs["creationflags"] = int(kwargs.get("creationflags", 0)) | _CREATE_NO_WINDOW
    # CREATE_NO_WINDOW  alone 对 powershell 有时仍闪一下；再加 STARTUPINFO
    si = subprocess.STARTUPINFO()
    si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    si.wShowWindow = _SW_HIDE
    kwargs["startupinfo"] = si
    return subprocess.run(cmd, **kwargs)


def write_vdd_settings(path: Path, displays: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    root = Element("vdd_settings")
    monitors = SubElement(root, "monitors")
    SubElement(monitors, "count").text = str(len(displays))

    global_el = SubElement(root, "global")
    for hz in sorted({int(d.get("hz", 60)) for d in displays}):
        SubElement(global_el, "g_refresh_rate").text = str(hz)

    resolutions = SubElement(root, "resolutions")
    seen: set[tuple[int, int, int]] = set()
    for d in displays:
        key = (int(d["width"]), int(d["height"]), int(d.get("hz", 60)))
        if key in seen:
            continue
        seen.add(key)
        res = SubElement(resolutions, "resolution")
        SubElement(res, "width").text = str(key[0])
        SubElement(res, "height").text = str(key[1])
        SubElement(res, "refresh_rate").text = str(key[2])

    options = SubElement(root, "options")
    SubElement(options, "CustomEdid").text = "false"
    SubElement(options, "PreventSpoof").text = "true"
    SubElement(options, "HardwareCursor").text = "true"
    SubElement(options, "logging").text = "false"
    path.write_bytes(tostring(root, encoding="utf-8", xml_declaration=True))


def _parse_pnputil_devices(text: str) -> list[dict]:
    """从 pnputil /enum-devices 文本里筛 Virtual Display 相关设备。"""
    hints = ("virtual display", "mttvdd", "iddsample", "indirect display", "vdd by")
    items: list[dict] = []
    cur: dict[str, str] = {}

    def flush() -> None:
        nonlocal cur
        if not cur:
            return
        desc = (cur.get("desc") or "") + " " + (cur.get("id") or "")
        low = desc.lower()
        if any(h in low for h in hints):
            items.append(
                {
                    "Status": cur.get("status", ""),
                    "Class": cur.get("class", ""),
                    "FriendlyName": cur.get("desc", ""),
                    "InstanceId": cur.get("id", ""),
                }
            )
        cur = {}

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            flush()
            continue
        low = line.lower()
        if low.startswith("instance id:") or low.startswith("实例 id:") or "instance id:" in low:
            flush()
            cur["id"] = line.split(":", 1)[-1].strip()
        elif low.startswith("device description:") or low.startswith("设备描述:"):
            cur["desc"] = line.split(":", 1)[-1].strip()
        elif low.startswith("class name:") or low.startswith("类名:"):
            cur["class"] = line.split(":", 1)[-1].strip()
        elif low.startswith("status:") or low.startswith("状态:"):
            cur["status"] = line.split(":", 1)[-1].strip()
    flush()
    return items


def find_vdd_devices(*, force: bool = False) -> list[dict]:
    global _device_cache, _device_cache_at
    now = time.time()
    if not force and _device_cache is not None and now - _device_cache_at < 8:
        return _device_cache

    # 优先 pnputil（无 PowerShell 蓝框）；失败再回退隐藏 PowerShell
    r = _run(["pnputil", "/enum-devices", "/connected"])
    data = _parse_pnputil_devices(r.stdout or "")
    if not data:
        ps = r"""
$names = @('Virtual Display Driver','IddSampleDriver','MttVDD','VDD','Indirect Display Driver')
Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
  $n = $_.FriendlyName
  foreach ($k in $names) { if ($n -like ("*" + $k + "*")) { return $true } }
  $false
} | Select-Object Status, Class, FriendlyName, InstanceId | ConvertTo-Json -Compress
"""
        r2 = _run(
            [
                "powershell",
                "-NoProfile",
                "-NonInteractive",
                "-WindowStyle",
                "Hidden",
                "-Command",
                ps,
            ]
        )
        out = (r2.stdout or "").strip()
        if out:
            parsed = json.loads(out)
            data = [parsed] if isinstance(parsed, dict) else list(parsed)

    _device_cache, _device_cache_at = data, now
    return _device_cache


def vdd_installed() -> bool:
    # 启动态优先走目录/监视器，避免无谓拉起 pnputil/powershell
    if Path(r"C:\VirtualDisplayDriver").is_dir():
        return True
    if any(m.likely_virtual for m in win_display.list_monitors()):
        return True
    return bool(find_vdd_devices())


def _pnp_set_enabled(instance_id: str, enabled: bool) -> None:
    global _device_cache_at
    # pnputil 启停，避免 PowerShell 窗
    action = "/enable-device" if enabled else "/disable-device"
    r = _run(["pnputil", action, instance_id])
    if r.returncode != 0:
        cmd = "Enable-PnpDevice" if enabled else "Disable-PnpDevice"
        ps = f'{cmd} -InstanceId "{instance_id}" -Confirm:$false'
        r2 = _run(
            [
                "powershell",
                "-NoProfile",
                "-NonInteractive",
                "-WindowStyle",
                "Hidden",
                "-Command",
                ps,
            ]
        )
        if r2.returncode != 0:
            raise RuntimeError((r2.stderr or r2.stdout or r.stderr or r.stdout or f"{cmd} 失败").strip())
    _device_cache_at = 0


def restart_vdd_devices() -> int:
    devices = find_vdd_devices(force=True)
    if not devices:
        raise RuntimeError("未检测到 Virtual Display Driver。请先点「安装驱动」，再点「应用」。")
    n = 0
    for d in devices:
        iid = d.get("InstanceId")
        if not iid:
            continue
        try:
            _pnp_set_enabled(iid, False)
        except RuntimeError:
            pass
        time.sleep(0.25)
        _pnp_set_enabled(iid, True)
        n += 1
    time.sleep(0.6)
    return n


def disable_vdd_devices() -> int:
    devices = find_vdd_devices(force=True)
    n = 0
    for d in devices:
        iid = d.get("InstanceId")
        if not iid:
            continue
        _pnp_set_enabled(iid, False)
        n += 1
    return n


def wait_for_virtual_monitors(expected: int, timeout_s: float = 8.0) -> list[win_display.MonitorInfo]:
    deadline = time.time() + timeout_s
    last: list[win_display.MonitorInfo] = []
    while time.time() < deadline:
        mons = win_display.list_monitors()
        virtuals = [m for m in mons if m.likely_virtual]
        last = virtuals
        if len(virtuals) >= expected:
            return virtuals
        secondary = [m for m in mons if not m.is_primary]
        if len(secondary) >= expected:
            return secondary[:expected]
        time.sleep(0.2)
    return last


def apply_config(cfg: dict, progress=None) -> str:
    """progress: 可选回调 progress(str)，用于界面提示。"""

    def note(msg: str) -> None:
        if progress:
            progress(msg)

    displays = cfg["displays"]
    path = Path(cfg.get("vdd_settings_path") or r"C:\VirtualDisplayDriver\vdd_settings.xml")
    note("写入驱动配置…")
    write_vdd_settings(path, displays)

    # 已有足够虚拟屏时跳过整驱动重启（最耗时），只改分辨率/位置/缩放
    existing = [m for m in win_display.list_monitors() if m.likely_virtual]
    n_dev = 0
    if len(existing) >= len(displays):
        note("虚拟屏已在线，跳过驱动重启…")
        virtuals = existing[: len(displays)]
    else:
        note("重启虚拟显示驱动…")
        n_dev = restart_vdd_devices()
        note("等待虚拟屏出现…")
        virtuals = wait_for_virtual_monitors(len(displays))

    if len(virtuals) < len(displays):
        return (
            f"已写入 {path}"
            + (f" 并重启 {n_dev} 个驱动设备" if n_dev else "")
            + f"，但只看到 {len(virtuals)} 块副屏（期望 {len(displays)}）。"
            "可在系统显示设置里确认。"
        )

    note("设置分辨率与排列…")
    primary = next((m for m in win_display.list_monitors() if m.is_primary), None)
    if primary is None:
        raise RuntimeError("找不到主显示器")
    x = primary.left + primary.width
    y = primary.top
    for mon, spec in zip(virtuals, displays):
        w, h, hz = int(spec["width"]), int(spec["height"]), int(spec.get("hz", 60))
        win_display.set_mode(mon.device_name, w, h, hz, x=x, y=y)
        x += w
    win_display.apply_display_changes()

    time.sleep(0.25)
    note("应用 DPI 缩放…")
    virtuals2 = wait_for_virtual_monitors(len(displays), timeout_s=4.0) or virtuals
    dpi_notes: list[str] = []
    for mon, spec in zip(virtuals2, displays):
        try:
            win_display.set_dpi_scale(mon.device_name, int(spec["scale"]))
        except Exception as e:  # ponytail: DPI 因系统而异
            dpi_notes.append(f"{mon.device_name}: {e}")

    if n_dev:
        msg = f"已应用 {len(displays)} 块虚拟屏（驱动设备 {n_dev}）。"
    else:
        msg = f"已更新 {len(displays)} 块虚拟屏分辨率/缩放。"
    if dpi_notes:
        msg += " 部分缩放未生效，可在系统显示设置里手动设。"
    return msg


def clear_virtual_displays() -> str:
    n = disable_vdd_devices()
    if n == 0:
        return "未找到可禁用的虚拟显示设备。"
    return f"已禁用 {n} 个虚拟显示设备。"


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, dest)


def install_driver() -> str:
    from .elevate import is_admin

    if not is_admin():
        raise RuntimeError("安装驱动需要管理员权限")

    tmp = Path(tempfile.mkdtemp(prefix="VDDInstall_"))
    try:
        nef_zip = tmp / "nefcon.zip"
        drv_zip = tmp / "driver.zip"
        _download(NEFCON_URL, nef_zip)
        _download(DRIVER_URL, drv_zip)
        with zipfile.ZipFile(nef_zip) as z:
            z.extractall(tmp)
        with zipfile.ZipFile(drv_zip) as z:
            z.extractall(tmp)

        nefcon = tmp / "x64" / "nefconw.exe"
        if not nefcon.is_file():
            raise RuntimeError("nefconw.exe 缺失")
        infs = list(tmp.rglob("MttVDD.inf"))
        if not infs:
            raise RuntimeError("压缩包内找不到 MttVDD.inf")
        driver_dir = infs[0].parent

        cats = list(driver_dir.glob("*.cat"))
        if cats:
            ps = (
                f"$b=[IO.File]::ReadAllBytes('{cats[0]}');"
                "$c=New-Object Security.Cryptography.X509Certificates.X509Certificate2Collection;"
                "$c.Import($b);"
                "$d=Join-Path $env:TEMP ('vdcert_'+[guid]::NewGuid().ToString('N'));"
                "New-Item -ItemType Directory -Path $d | Out-Null;"
                "foreach($x in $c){"
                "$p=Join-Path $d ($x.Thumbprint+'.cer');"
                "[IO.File]::WriteAllBytes($p,$x.Export('Cert'));"
                "Import-Certificate -FilePath $p -CertStoreLocation Cert:\\LocalMachine\\TrustedPublisher | Out-Null"
                "}"
            )
            _run(["powershell", "-NoProfile", "-WindowStyle", "Hidden", "-Command", ps])

        r = _run([str(nefcon), "install", str(infs[0]), r"Root\MttVDD"], cwd=str(driver_dir))
        time.sleep(8)
        Path(r"C:\VirtualDisplayDriver").mkdir(parents=True, exist_ok=True)
        global _device_cache_at
        _device_cache_at = 0
        if not vdd_installed():
            detail = (r.stderr or r.stdout or "").strip()
            raise RuntimeError(f"安装后仍未检测到设备。nefcon={r.returncode} {detail}")
        return "Virtual Display Driver 安装完成。"
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
