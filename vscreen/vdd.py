from __future__ import annotations

import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path
from xml.etree.ElementTree import Element, SubElement, tostring

from . import win_display

VDD_DEVICE_MATCH = (
    "Virtual Display Driver",
    "IddSampleDriver",
    "MttVDD",
    "VDD by MTT",
    "Indirect Display",
)

NEFCON_URL = "https://github.com/nefarius/nefcon/releases/download/v1.14.0/nefcon_v1.14.0.zip"
DRIVER_URL = (
    "https://github.com/VirtualDrivers/Virtual-Display-Driver/releases/download/"
    "25.7.23/VirtualDisplayDriver-x86.Driver.Only.zip"
)


def write_vdd_settings(path: Path, displays: list[dict]) -> None:
    """写入 VDD 的 vdd_settings.xml（count + 分辨率表）。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    root = Element("vdd_settings")
    monitors = SubElement(root, "monitors")
    SubElement(monitors, "count").text = str(len(displays))

    global_el = SubElement(root, "global")
    hz_set = sorted({int(d.get("hz", 60)) for d in displays})
    for hz in hz_set:
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

    xml = tostring(root, encoding="utf-8", xml_declaration=True)
    path.write_bytes(xml)


def find_vdd_devices() -> list[dict]:
    """用 PowerShell 枚举疑似 VDD 的 PnP 设备。"""
    ps = r"""
$names = @('Virtual Display Driver','IddSampleDriver','MttVDD','VDD','Indirect Display Driver')
Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
  $n = $_.FriendlyName
  foreach ($k in $names) { if ($n -like ("*" + $k + "*")) { return $true } }
  $false
} | Select-Object Status, Class, FriendlyName, InstanceId | ConvertTo-Json -Compress
"""
    r = subprocess.run(
        ["powershell", "-NoProfile", "-Command", ps],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    out = (r.stdout or "").strip()
    if not out:
        return []
    import json

    data = json.loads(out)
    if isinstance(data, dict):
        return [data]
    return list(data)


def vdd_installed() -> bool:
    if find_vdd_devices():
        return True
    return Path(r"C:\VirtualDisplayDriver").is_dir()


def _pnp_set_enabled(instance_id: str, enabled: bool) -> None:
    cmd = "Enable-PnpDevice" if enabled else "Disable-PnpDevice"
    ps = f'{cmd} -InstanceId "{instance_id}" -Confirm:$false'
    r = subprocess.run(
        ["powershell", "-NoProfile", "-Command", ps],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if r.returncode != 0:
        raise RuntimeError((r.stderr or r.stdout or f"{cmd} 失败").strip())


def restart_vdd_devices() -> int:
    devices = find_vdd_devices()
    if not devices:
        raise RuntimeError(
            "未检测到 Virtual Display Driver。请先点「安装驱动」，再点「应用」。"
        )
    n = 0
    for d in devices:
        iid = d.get("InstanceId")
        if not iid:
            continue
        try:
            _pnp_set_enabled(iid, False)
        except RuntimeError:
            pass
        time.sleep(0.4)
        _pnp_set_enabled(iid, True)
        n += 1
    time.sleep(1.2)
    return n


def disable_vdd_devices() -> int:
    devices = find_vdd_devices()
    n = 0
    for d in devices:
        iid = d.get("InstanceId")
        if not iid:
            continue
        _pnp_set_enabled(iid, False)
        n += 1
    return n


def wait_for_virtual_monitors(expected: int, timeout_s: float = 12.0) -> list[win_display.MonitorInfo]:
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
        time.sleep(0.5)
    return last


def apply_config(cfg: dict) -> str:
    displays = cfg["displays"]
    path = Path(cfg.get("vdd_settings_path") or r"C:\VirtualDisplayDriver\vdd_settings.xml")
    write_vdd_settings(path, displays)
    n_dev = restart_vdd_devices()
    virtuals = wait_for_virtual_monitors(len(displays))
    if len(virtuals) < len(displays):
        return (
            f"已写入 {path} 并重启 {n_dev} 个驱动设备，但只看到 {len(virtuals)} 块副屏"
            f"（期望 {len(displays)}）。可在「设置 → 系统 → 显示」里确认，或稍后看预览。"
        )

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

    time.sleep(0.8)
    virtuals2 = wait_for_virtual_monitors(len(displays), timeout_s=6.0)
    dpi_notes: list[str] = []
    for mon, spec in zip(virtuals2, displays):
        try:
            win_display.set_dpi_scale(mon.device_name, int(spec["scale"]))
        except Exception as e:  # ponytail: DPI API 因系统/驱动而异，失败不阻断分辨率
            dpi_notes.append(f"{mon.device_name}: {e}")

    msg = f"已应用 {len(displays)} 块虚拟屏（驱动设备 {n_dev}）。"
    if dpi_notes:
        msg += " 部分缩放未生效（可在系统显示设置里手动设）：" + "；".join(dpi_notes)
    return msg


def clear_virtual_displays() -> str:
    n = disable_vdd_devices()
    if n == 0:
        return "未找到可禁用的虚拟显示设备（可能尚未安装驱动，或名称不匹配）。"
    return f"已禁用 {n} 个虚拟显示设备。"


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, dest)


def install_driver() -> str:
    """下载上游签名驱动并用 nefcon 静默安装。需管理员。"""
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

        # 导入 cat 证书到 TrustedPublisher
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
            subprocess.run(
                ["powershell", "-NoProfile", "-Command", ps],
                check=False,
                capture_output=True,
            )

        r = subprocess.run(
            [str(nefcon), "install", str(infs[0]), r"Root\MttVDD"],
            cwd=str(driver_dir),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        time.sleep(8)
        Path(r"C:\VirtualDisplayDriver").mkdir(parents=True, exist_ok=True)
        if not vdd_installed():
            detail = (r.stderr or r.stdout or "").strip()
            raise RuntimeError(f"安装后仍未检测到设备。nefcon={r.returncode} {detail}")
        return "Virtual Display Driver 安装完成。"
    finally:
        # ponytail: 临时目录清理失败可忽略，下次重启 TEMP 会扫
        try:
            import shutil

            shutil.rmtree(tmp, ignore_errors=True)
        except Exception:
            pass
