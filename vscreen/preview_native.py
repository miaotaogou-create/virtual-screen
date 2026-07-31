"""独立 Win32 预览子窗口：避免往 Tk 控件上 GDI 画导致不停闪烁。"""

from __future__ import annotations

import ctypes
from ctypes import wintypes
from typing import Optional

from . import win_display

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
kernel32 = ctypes.windll.kernel32

WM_PAINT = 0x000F
WM_ERASEBKGND = 0x0014
WM_DESTROY = 0x0002
WS_CHILD = 0x40000000
WS_VISIBLE = 0x10000000
WS_CLIPSIBLINGS = 0x04000000
SWP_NOZORDER = 0x0004
SWP_NOACTIVATE = 0x0010
RDW_INVALIDATE = 0x0001
RDW_UPDATENOW = 0x0100

# 64 位下 WndProc 返回 LRESULT
WNDPROC = ctypes.WINFUNCTYPE(
    ctypes.c_ssize_t, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM
)


class PAINTSTRUCT(ctypes.Structure):
    _fields_ = [
        ("hdc", wintypes.HDC),
        ("fErase", wintypes.BOOL),
        ("rcPaint", win_display.RECT),
        ("fRestore", wintypes.BOOL),
        ("fIncUpdate", wintypes.BOOL),
        ("rgbReserved", wintypes.BYTE * 32),
    ]


class WNDCLASSW(ctypes.Structure):
    _fields_ = [
        ("style", wintypes.UINT),
        ("lpfnWndProc", WNDPROC),
        ("cbClsExtra", ctypes.c_int),
        ("cbWndExtra", ctypes.c_int),
        ("hInstance", wintypes.HINSTANCE),
        ("hIcon", wintypes.HICON),
        ("hCursor", wintypes.HCURSOR),
        ("hbrBackground", wintypes.HBRUSH),
        ("lpszMenuName", wintypes.LPCWSTR),
        ("lpszClassName", wintypes.LPCWSTR),
    ]


_CLASS = "VirtualScreenPreviewHost"
_registered = False
_wndproc_hold = None  # 防 GC
# hwnd -> MonitorInfo | None
_mon_by_hwnd: dict[int, Optional[win_display.MonitorInfo]] = {}
_bg = 0x0020120B  # 与主题 preview_bg 接近，COLORREF=0x00bbggrr


def _paint(hwnd: int, hdc: int) -> None:
    rc = win_display.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rc))
    cw, ch = rc.right - rc.left, rc.bottom - rc.top
    brush = gdi32.CreateSolidBrush(_bg)
    user32.FillRect(hdc, ctypes.byref(rc), brush)
    gdi32.DeleteObject(brush)
    mon = _mon_by_hwnd.get(int(hwnd))
    if not mon or cw < 2 or ch < 2:
        return
    hdc_src = user32.GetDC(0)
    if not hdc_src:
        return
    try:
        fit = min(cw / max(1, mon.width), ch / max(1, mon.height))
        dw = max(1, int(mon.width * fit))
        dh = max(1, int(mon.height * fit))
        dx = (cw - dw) // 2
        dy = (ch - dh) // 2
        gdi32.SetStretchBltMode(hdc, win_display.COLORONCOLOR)
        gdi32.StretchBlt(
            hdc, dx, dy, dw, dh, hdc_src, mon.left, mon.top, mon.width, mon.height, win_display.SRCCOPY
        )
    finally:
        user32.ReleaseDC(0, hdc_src)


def _wndproc(hwnd, msg, wparam, lparam):
    if msg == WM_ERASEBKGND:
        return 1
    if msg == WM_PAINT:
        ps = PAINTSTRUCT()
        hdc = user32.BeginPaint(hwnd, ctypes.byref(ps))
        try:
            _paint(int(hwnd), int(hdc))
        finally:
            user32.EndPaint(hwnd, ctypes.byref(ps))
        return 0
    if msg == WM_DESTROY:
        _mon_by_hwnd.pop(int(hwnd), None)
        return 0
    return user32.DefWindowProcW(hwnd, msg, wparam, lparam)


def _ensure_class() -> None:
    global _registered, _wndproc_hold
    if _registered:
        return
    _wndproc_hold = WNDPROC(_wndproc)
    wc = WNDCLASSW()
    wc.style = 0
    wc.lpfnWndProc = _wndproc_hold
    wc.cbClsExtra = 0
    wc.cbWndExtra = 0
    wc.hInstance = kernel32.GetModuleHandleW(None)
    wc.hIcon = None
    wc.hCursor = user32.LoadCursorW(None, 32512)  # IDC_ARROW
    wc.hbrBackground = gdi32.CreateSolidBrush(_bg)
    wc.lpszMenuName = None
    wc.lpszClassName = _CLASS
    if not user32.RegisterClassW(ctypes.byref(wc)):
        err = ctypes.get_last_error()
        # 已注册过
        if err != 1410:  # ERROR_CLASS_ALREADY_EXISTS
            raise ctypes.WinError(err)
    _registered = True


class NativePreview:
    """嵌在 Tk Frame 里的预览子窗口。"""

    def __init__(self) -> None:
        self.hwnd = 0
        self._parent = 0

    def attach(self, parent_hwnd: int, width: int, height: int) -> None:
        _ensure_class()
        self._parent = int(parent_hwnd)
        if self.hwnd:
            self.resize(width, height)
            return
        hwnd = user32.CreateWindowExW(
            0,
            _CLASS,
            "",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0,
            0,
            max(1, width),
            max(1, height),
            self._parent,
            None,
            kernel32.GetModuleHandleW(None),
            None,
        )
        if not hwnd:
            raise ctypes.WinError(ctypes.get_last_error())
        self.hwnd = int(hwnd)
        _mon_by_hwnd[self.hwnd] = None

    def resize(self, width: int, height: int) -> None:
        if not self.hwnd:
            return
        user32.SetWindowPos(
            self.hwnd,
            None,
            0,
            0,
            max(1, width),
            max(1, height),
            SWP_NOZORDER | SWP_NOACTIVATE,
        )

    def set_monitor(self, mon: Optional[win_display.MonitorInfo]) -> None:
        if not self.hwnd:
            return
        _mon_by_hwnd[self.hwnd] = mon
        user32.RedrawWindow(self.hwnd, None, None, RDW_INVALIDATE | RDW_UPDATENOW)

    def show(self, visible: bool) -> None:
        if not self.hwnd:
            return
        user32.ShowWindow(self.hwnd, 5 if visible else 0)  # SW_SHOW / SW_HIDE

    def destroy(self) -> None:
        if self.hwnd:
            user32.DestroyWindow(self.hwnd)
            self.hwnd = 0
