from __future__ import annotations

import subprocess
import time
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
    # 仅有配置目录也算「可能已装过」
    return Path(r"C:\VirtualDisplayDriver").is_dir()


def _pnp_set_enabled(instance_id: str, enabled: bool) -> None:
    cmd = "Enable-PnpDevice" if enabled else "Disable-PnpDevice"
    ps = f'{cmd} -InstanceId "{instance_id}" -Confirm:$false'
    # 需要管理员；失败时抛出 stderr
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
    """禁用再启用，迫使驱动重读 XML。返回操作的设备数。"""
    devices = find_vdd_devices()
    if not devices:
        raise RuntimeError(
            "未检测到 Virtual Display Driver。请先按 README 安装驱动，再点「应用」。"
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
        # 驱动名未进 EDID 时，用「新增的非主屏」兜底：数量对齐即可
        secondary = [m for m in mons if not m.is_primary]
        if len(secondary) >= expected:
            return secondary[:expected]
        time.sleep(0.5)
    return last


def apply_config(cfg: dict) -> str:
    """写 XML → 重启驱动 → 设分辨率/位置/缩放。返回状态摘要。"""
    displays = cfg["displays"]
    path = Path(cfg.get("vdd_settings_path") or r"C:\VirtualDisplayDriver\vdd_settings.xml")
    write_vdd_settings(path, displays)
    n_dev = restart_vdd_devices()
    virtuals = wait_for_virtual_monitors(len(displays))
    if len(virtuals) < len(displays):
        return (
            f"已写入 {path} 并重启 {n_dev} 个驱动设备，但只看到 {len(virtuals)} 块副屏"
            f"（期望 {len(displays)}）。可在「设置 → 系统 → 显示」里确认，或稍后点预览。"
        )

    # 主屏右侧依次摆放，并设分辨率
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

    # 分辨率变更后句柄可能变，再枚举一次设 DPI
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
    return f"已禁用 {n} 个虚拟显示设备。可在设备管理器中重新启用。"
