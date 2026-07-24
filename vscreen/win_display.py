from __future__ import annotations

import ctypes
from ctypes import wintypes
from dataclasses import dataclass

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

ENUM_CURRENT_SETTINGS = -1
CDS_UPDATEREGISTRY = 0x00000001
CDS_NORESET = 0x10000000
CDS_RESET = 0x40000000
DISP_CHANGE_SUCCESSFUL = 0
DM_PELSWIDTH = 0x00080000
DM_PELSHEIGHT = 0x00100000
DM_DISPLAYFREQUENCY = 0x00400000
DM_POSITION = 0x00000020
DISPLAY_DEVICE_ACTIVE = 0x00000001
DISPLAY_DEVICE_PRIMARY_DEVICE = 0x00000004
DISPLAY_DEVICE_ATTACHED_TO_DESKTOP = 0x00000001
SM_CXVIRTUALSCREEN = 78
SM_CYVIRTUALSCREEN = 79
SM_XVIRTUALSCREEN = 76
SM_YVIRTUALSCREEN = 77
SRCCOPY = 0x00CC0020
HALFTONE = 4
COLORONCOLOR = 3  # 预览缩小用，比 HALFTONE 轻

QDC_ONLY_ACTIVE_PATHS = 0x00000002
DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE = 1
DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME = 1
DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE = -3
DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE = -4

DPI_SCALE_VALUES = (100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500)

VDD_HINTS = ("VDD", "MTT", "IddSample", "Virtual Display", "MttVDD", "Indirect")

_dpi_ready = False


def ensure_dpi_aware() -> None:
    """进程级 Per-Monitor DPI，避免抓屏坐标与逻辑分辨率错位。"""
    global _dpi_ready
    if _dpi_ready:
        return
    try:
        # PROCESS_PER_MONITOR_DPI_AWARE = 2
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass
    _dpi_ready = True


class DISPLAY_DEVICEW(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("DeviceName", wintypes.WCHAR * 32),
        ("DeviceString", wintypes.WCHAR * 128),
        ("StateFlags", wintypes.DWORD),
        ("DeviceID", wintypes.WCHAR * 128),
        ("DeviceKey", wintypes.WCHAR * 128),
    ]


class POINTL(ctypes.Structure):
    _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


class DEVMODEW(ctypes.Structure):
    _fields_ = [
        ("dmDeviceName", wintypes.WCHAR * 32),
        ("dmSpecVersion", wintypes.WORD),
        ("dmDriverVersion", wintypes.WORD),
        ("dmSize", wintypes.WORD),
        ("dmDriverExtra", wintypes.WORD),
        ("dmFields", wintypes.DWORD),
        ("dmPosition", POINTL),
        ("dmDisplayOrientation", wintypes.DWORD),
        ("dmDisplayFixedOutput", wintypes.DWORD),
        ("dmColor", wintypes.SHORT),
        ("dmDuplex", wintypes.SHORT),
        ("dmYResolution", wintypes.SHORT),
        ("dmTTOption", wintypes.SHORT),
        ("dmCollate", wintypes.SHORT),
        ("dmFormName", wintypes.WCHAR * 32),
        ("dmLogPixels", wintypes.WORD),
        ("dmBitsPerPel", wintypes.DWORD),
        ("dmPelsWidth", wintypes.DWORD),
        ("dmPelsHeight", wintypes.DWORD),
        ("dmDisplayFlags", wintypes.DWORD),
        ("dmDisplayFrequency", wintypes.DWORD),
        ("dmICMMethod", wintypes.DWORD),
        ("dmICMIntent", wintypes.DWORD),
        ("dmMediaType", wintypes.DWORD),
        ("dmDitherType", wintypes.DWORD),
        ("dmReserved1", wintypes.DWORD),
        ("dmReserved2", wintypes.DWORD),
        ("dmPanningWidth", wintypes.DWORD),
        ("dmPanningHeight", wintypes.DWORD),
    ]


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", wintypes.LONG),
        ("top", wintypes.LONG),
        ("right", wintypes.LONG),
        ("bottom", wintypes.LONG),
    ]


class MONITORINFOEXW(ctypes.Structure):
    _fields_ = [
        ("cbSize", wintypes.DWORD),
        ("rcMonitor", RECT),
        ("rcWork", RECT),
        ("dwFlags", wintypes.DWORD),
        ("szDevice", wintypes.WCHAR * 32),
    ]


class LUID(ctypes.Structure):
    _fields_ = [("LowPart", wintypes.DWORD), ("HighPart", wintypes.LONG)]


class DISPLAYCONFIG_PATH_SOURCE_INFO(ctypes.Structure):
    _fields_ = [
        ("adapterId", LUID),
        ("id", wintypes.UINT),
        ("modeInfoIdx", wintypes.UINT),
        ("statusFlags", wintypes.UINT),
    ]


class DISPLAYCONFIG_RATIONAL(ctypes.Structure):
    _fields_ = [("Numerator", wintypes.UINT), ("Denominator", wintypes.UINT)]


class DISPLAYCONFIG_PATH_TARGET_INFO(ctypes.Structure):
    _fields_ = [
        ("adapterId", LUID),
        ("id", wintypes.UINT),
        ("modeInfoIdx", wintypes.UINT),
        ("outputTechnology", wintypes.UINT),
        ("rotation", wintypes.UINT),
        ("scaling", wintypes.UINT),
        ("refreshRate", DISPLAYCONFIG_RATIONAL),
        ("scanLineOrdering", wintypes.UINT),
        ("targetAvailable", wintypes.BOOL),
        ("statusFlags", wintypes.UINT),
    ]


class DISPLAYCONFIG_PATH_INFO(ctypes.Structure):
    _fields_ = [
        ("sourceInfo", DISPLAYCONFIG_PATH_SOURCE_INFO),
        ("targetInfo", DISPLAYCONFIG_PATH_TARGET_INFO),
        ("flags", wintypes.UINT),
    ]


class DISPLAYCONFIG_2DREGION(ctypes.Structure):
    _fields_ = [("cx", wintypes.UINT), ("cy", wintypes.UINT)]


class DISPLAYCONFIG_VIDEO_SIGNAL_INFO(ctypes.Structure):
    _fields_ = [
        ("pixelRate", ctypes.c_uint64),
        ("hSyncFreq", DISPLAYCONFIG_RATIONAL),
        ("vSyncFreq", DISPLAYCONFIG_RATIONAL),
        ("activeSize", DISPLAYCONFIG_2DREGION),
        ("totalSize", DISPLAYCONFIG_2DREGION),
        ("videoStandard", wintypes.UINT),
        ("scanLineOrdering", wintypes.UINT),
    ]


class DISPLAYCONFIG_SOURCE_MODE(ctypes.Structure):
    _fields_ = [
        ("width", wintypes.UINT),
        ("height", wintypes.UINT),
        ("pixelFormat", wintypes.UINT),
        ("position", POINTL),
    ]


class DISPLAYCONFIG_TARGET_MODE(ctypes.Structure):
    _fields_ = [("targetVideoSignalInfo", DISPLAYCONFIG_VIDEO_SIGNAL_INFO)]


class DISPLAYCONFIG_MODE_INFO(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [
            ("targetMode", DISPLAYCONFIG_TARGET_MODE),
            ("sourceMode", DISPLAYCONFIG_SOURCE_MODE),
        ]

    _fields_ = [
        ("infoType", wintypes.UINT),
        ("id", wintypes.UINT),
        ("adapterId", LUID),
        ("mode", _U),
    ]


class DISPLAYCONFIG_DEVICE_INFO_HEADER(ctypes.Structure):
    _fields_ = [
        ("type", wintypes.INT),
        ("size", wintypes.UINT),
        ("adapterId", LUID),
        ("id", wintypes.UINT),
    ]


class DISPLAYCONFIG_SOURCE_DEVICE_NAME(ctypes.Structure):
    _fields_ = [
        ("header", DISPLAYCONFIG_DEVICE_INFO_HEADER),
        ("viewGdiDeviceName", wintypes.WCHAR * 32),
    ]


class DISPLAYCONFIG_SOURCE_DPI_SCALE_GET(ctypes.Structure):
    _fields_ = [
        ("header", DISPLAYCONFIG_DEVICE_INFO_HEADER),
        ("minScaleRel", ctypes.c_int32),
        ("curScaleRel", ctypes.c_int32),
        ("maxScaleRel", ctypes.c_int32),
    ]


class DISPLAYCONFIG_SOURCE_DPI_SCALE_SET(ctypes.Structure):
    _fields_ = [
        ("header", DISPLAYCONFIG_DEVICE_INFO_HEADER),
        ("scaleRel", ctypes.c_int32),
    ]


@dataclass
class MonitorInfo:
    device_name: str  # \\.\DISPLAYn
    monitor_name: str
    adapter_name: str
    left: int
    top: int
    width: int
    height: int
    is_primary: bool
    likely_virtual: bool


def _is_vdd_text(text: str) -> bool:
    t = text.upper()
    return any(h.upper() in t for h in VDD_HINTS)


def list_monitors() -> list[MonitorInfo]:
    """列出当前接到桌面的监视器。"""
    ensure_dpi_aware()
    by_device: dict[str, tuple[str, str, bool]] = {}
    i = 0
    while True:
        adapter = DISPLAY_DEVICEW()
        adapter.cb = ctypes.sizeof(adapter)
        if not user32.EnumDisplayDevicesW(None, i, ctypes.byref(adapter), 0):
            break
        j = 0
        while True:
            mon = DISPLAY_DEVICEW()
            mon.cb = ctypes.sizeof(mon)
            if not user32.EnumDisplayDevicesW(adapter.DeviceName, j, ctypes.byref(mon), 0):
                break
            if mon.StateFlags & DISPLAY_DEVICE_ACTIVE:
                primary = bool(adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
                likely = _is_vdd_text(adapter.DeviceString) or _is_vdd_text(mon.DeviceString)
                by_device[adapter.DeviceName] = (mon.DeviceString, adapter.DeviceString, primary or likely)
            j += 1
        i += 1

    out: list[MonitorInfo] = []

    def _cb(hmon, _hdc, _lprect, _data):
        info = MONITORINFOEXW()
        info.cbSize = ctypes.sizeof(info)
        if not user32.GetMonitorInfoW(hmon, ctypes.byref(info)):
            return 1
        name = info.szDevice
        mon_name, adapter_name, _ = by_device.get(name, ("", "", False))
        is_primary = bool(info.dwFlags & 1)
        likely = _is_vdd_text(adapter_name) or _is_vdd_text(mon_name)
        # 无 VDD 字样时：非主屏也纳入「可预览副屏」
        out.append(
            MonitorInfo(
                device_name=name,
                monitor_name=mon_name or name,
                adapter_name=adapter_name,
                left=info.rcMonitor.left,
                top=info.rcMonitor.top,
                width=info.rcMonitor.right - info.rcMonitor.left,
                height=info.rcMonitor.bottom - info.rcMonitor.top,
                is_primary=is_primary,
                likely_virtual=likely,
            )
        )
        return 1

    MONITORENUMPROC = ctypes.WINFUNCTYPE(
        ctypes.c_int, wintypes.HMONITOR, wintypes.HDC, ctypes.POINTER(RECT), wintypes.LPARAM
    )
    user32.EnumDisplayMonitors(0, 0, MONITORENUMPROC(_cb), 0)
    out.sort(key=lambda m: (not m.is_primary, m.left, m.top, m.device_name))
    return out


def preview_targets(prefer_virtual: bool = True) -> list[MonitorInfo]:
    mons = list_monitors()
    virtuals = [m for m in mons if m.likely_virtual]
    if prefer_virtual and virtuals:
        return virtuals
    secondary = [m for m in mons if not m.is_primary]
    return secondary or mons


def set_mode(device_name: str, width: int, height: int, hz: int, x: int | None = None, y: int | None = None) -> None:
    dm = DEVMODEW()
    dm.dmSize = ctypes.sizeof(dm)
    if not user32.EnumDisplaySettingsW(device_name, ENUM_CURRENT_SETTINGS, ctypes.byref(dm)):
        raise RuntimeError(f"无法读取显示模式: {device_name}")
    dm.dmPelsWidth = width
    dm.dmPelsHeight = height
    dm.dmDisplayFrequency = hz
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY
    if x is not None and y is not None:
        dm.dmPosition.x = x
        dm.dmPosition.y = y
        dm.dmFields |= DM_POSITION
    flags = CDS_UPDATEREGISTRY | CDS_NORESET
    rc = user32.ChangeDisplaySettingsExW(device_name, ctypes.byref(dm), None, flags, None)
    if rc != DISP_CHANGE_SUCCESSFUL:
        raise RuntimeError(f"设置分辨率失败 {device_name}: {width}x{height}@{hz} (code={rc})")


def apply_display_changes() -> None:
    rc = user32.ChangeDisplaySettingsExW(None, None, None, 0, None)
    if rc != DISP_CHANGE_SUCCESSFUL:
        raise RuntimeError(f"提交显示更改失败 (code={rc})")


def _query_paths():
    path_count = wintypes.UINT()
    mode_count = wintypes.UINT()
    user32.GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, ctypes.byref(path_count), ctypes.byref(mode_count))
    paths = (DISPLAYCONFIG_PATH_INFO * path_count.value)()
    modes = (DISPLAYCONFIG_MODE_INFO * mode_count.value)()
    rc = user32.QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS,
        ctypes.byref(path_count),
        paths,
        ctypes.byref(mode_count),
        modes,
        None,
    )
    if rc != 0:
        raise RuntimeError(f"QueryDisplayConfig 失败: {rc}")
    return paths, path_count.value, modes, mode_count.value


def _source_name(adapter_id: LUID, source_id: int) -> str:
    name = DISPLAYCONFIG_SOURCE_DEVICE_NAME()
    name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME
    name.header.size = ctypes.sizeof(name)
    name.header.adapterId = adapter_id
    name.header.id = source_id
    rc = user32.DisplayConfigGetDeviceInfo(ctypes.byref(name))
    if rc != 0:
        return ""
    return name.viewGdiDeviceName


def set_dpi_scale(device_name: str, scale_percent: int) -> None:
    """按百分比设置每监视器 DPI。scale 取常见档位；自定义值会落到最近档。"""
    target = min(DPI_SCALE_VALUES, key=lambda v: abs(v - int(scale_percent)))
    paths, npath, modes, _ = _query_paths()
    for i in range(npath):
        src = paths[i].sourceInfo
        gdi = _source_name(src.adapterId, src.id)
        if gdi.upper() != device_name.upper():
            continue
        get = DISPLAYCONFIG_SOURCE_DPI_SCALE_GET()
        get.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE
        get.header.size = ctypes.sizeof(get)
        get.header.adapterId = src.adapterId
        get.header.id = src.id
        rc = user32.DisplayConfigGetDeviceInfo(ctypes.byref(get))
        if rc != 0:
            raise RuntimeError(f"读取 DPI 失败 {device_name}: {rc}")
        # recommended 绝对下标 = -minScaleRel（相对量以推荐值为 0）
        recommended = -get.minScaleRel
        abs_idx = None
        for idx, val in enumerate(DPI_SCALE_VALUES):
            if val == target:
                abs_idx = idx
                break
        if abs_idx is None:
            raise RuntimeError(f"不支持的缩放: {scale_percent}")
        rel = abs_idx - recommended
        if rel < get.minScaleRel or rel > get.maxScaleRel:
            raise RuntimeError(
                f"{device_name} 无法设为 {target}%（允许相对范围 "
                f"{get.minScaleRel}..{get.maxScaleRel}，推荐档索引 {recommended}）"
            )
        put = DISPLAYCONFIG_SOURCE_DPI_SCALE_SET()
        put.header.type = DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE
        put.header.size = ctypes.sizeof(put)
        put.header.adapterId = src.adapterId
        put.header.id = src.id
        put.scaleRel = rel
        rc = user32.DisplayConfigSetDeviceInfo(ctypes.byref(put))
        if rc != 0:
            raise RuntimeError(f"设置 DPI 失败 {device_name}: {rc}")
        return
    raise RuntimeError(f"未找到显示源: {device_name}")


def capture_monitor_rgb(mon: MonitorInfo, out_w: int, out_h: int) -> bytes:
    """用 StretchBlt 抓取监视器画面，返回 RGB 原始字节（out_w * out_h * 3）。"""
    ensure_dpi_aware()
    if out_w < 1 or out_h < 1:
        raise ValueError("预览尺寸无效")
    hdc_screen = user32.GetDC(0)
    if not hdc_screen:
        raise RuntimeError("GetDC 失败")
    try:
        hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
        # 24bpp + DWORD 行对齐，少拷一层 alpha，减轻预览 CPU
        stride = (out_w * 3 + 3) & ~3
        bmi = ctypes.create_string_buffer(40)
        bi = ctypes.cast(bmi, ctypes.POINTER(ctypes.c_uint32))
        bi[0] = 40
        bi[1] = out_w
        bi[2] = -out_h  # 顶向下
        ctypes.cast(ctypes.addressof(bmi) + 12, ctypes.POINTER(ctypes.c_uint16))[0] = 1
        ctypes.cast(ctypes.addressof(bmi) + 14, ctypes.POINTER(ctypes.c_uint16))[0] = 24
        bits = ctypes.c_void_p()
        hbmp = gdi32.CreateDIBSection(hdc_mem, bmi, 0, ctypes.byref(bits), None, 0)
        if not hbmp:
            gdi32.DeleteDC(hdc_mem)
            raise RuntimeError("CreateDIBSection 失败")
        old = gdi32.SelectObject(hdc_mem, hbmp)
        # COLORONCOLOR：预览缩放够用，比 HALFTONE 省 CPU
        gdi32.SetStretchBltMode(hdc_mem, COLORONCOLOR)
        ok = gdi32.StretchBlt(
            hdc_mem,
            0,
            0,
            out_w,
            out_h,
            hdc_screen,
            mon.left,
            mon.top,
            mon.width,
            mon.height,
            SRCCOPY,
        )
        gdi32.SelectObject(hdc_mem, old)
        if not ok:
            gdi32.DeleteObject(hbmp)
            gdi32.DeleteDC(hdc_mem)
            raise RuntimeError("StretchBlt 失败")
        # DIB 24bpp 为 BGR；转 RGB。按行处理，跳过对齐填充。
        src = ctypes.string_at(bits, stride * out_h)
        rgb = bytearray(out_w * out_h * 3)
        di = 0
        for y in range(out_h):
            row = y * stride
            for x in range(out_w):
                si = row + x * 3
                rgb[di] = src[si + 2]
                rgb[di + 1] = src[si + 1]
                rgb[di + 2] = src[si]
                di += 3
        gdi32.DeleteObject(hbmp)
        gdi32.DeleteDC(hdc_mem)
        return bytes(rgb)
    finally:
        user32.ReleaseDC(0, hdc_screen)


def rgb_to_ppm(rgb: bytes, w: int, h: int) -> bytes:
    return f"P6 {w} {h} 255\n".encode("ascii") + rgb


def arrange_after_primary(devices: list[str], sizes: list[tuple[int, int, int]]) -> None:
    """把给定设备依次摆到主屏右侧。sizes: (w,h,hz)。"""
    mons = list_monitors()
    primary = next((m for m in mons if m.is_primary), None)
    if primary is None:
        raise RuntimeError("找不到主显示器")
    x = primary.left + primary.width
    y = primary.top
    for dev, (w, h, hz) in zip(devices, sizes):
        set_mode(dev, w, h, hz, x=x, y=y)
        x += w
    apply_display_changes()
