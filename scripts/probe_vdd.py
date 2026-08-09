# -*- coding: utf-8 -*-
import ctypes
import time
from ctypes import wintypes

kernel32 = ctypes.windll.kernel32
user32 = ctypes.windll.user32

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
PIPE_READMODE_BYTE = 0
INVALID = 0xFFFFFFFF


class DISPLAY_DEVICE(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("DeviceName", wintypes.WCHAR * 32),
        ("DeviceString", wintypes.WCHAR * 128),
        ("StateFlags", wintypes.DWORD),
        ("DeviceID", wintypes.WCHAR * 128),
        ("DeviceKey", wintypes.WCHAR * 128),
    ]


def send_pipe(cmd: str, wait: float = 2.0) -> str:
    name = r"\\.\pipe\MTTVirtualDisplayPipe"
    h = kernel32.CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
    if h == INVALID or h == -1:
        return f"OPEN_FAIL err={ctypes.GetLastError()}"
    data = cmd.encode("utf-16le")
    written = wintypes.DWORD()
    if not kernel32.WriteFile(h, data, len(data), ctypes.byref(written), None):
        err = ctypes.GetLastError()
        kernel32.CloseHandle(h)
        return f"WRITE_FAIL err={err}"
    mode = wintypes.DWORD(PIPE_READMODE_BYTE)
    kernel32.SetNamedPipeHandleState(h, ctypes.byref(mode), None, None)
    buf = (ctypes.c_char * 8192)()
    read = wintypes.DWORD()
    avail = wintypes.DWORD()
    out = bytearray()
    t0 = time.time()
    while time.time() - t0 < wait:
        if not kernel32.PeekNamedPipe(h, None, 0, None, ctypes.byref(avail), None):
            break
        if avail.value:
            if not kernel32.ReadFile(h, buf, min(avail.value, 8192), ctypes.byref(read), None):
                break
            if read.value == 0:
                break
            out += bytes(buf[: read.value])
        else:
            if out and time.time() - t0 > 0.6:
                break
            time.sleep(0.05)
    kernel32.CloseHandle(h)
    return out.decode("utf-8", "replace")


def list_adapters():
    i = 0
    while True:
        a = DISPLAY_DEVICE()
        a.cb = ctypes.sizeof(a)
        if not user32.EnumDisplayDevicesW(None, i, ctypes.byref(a), 0):
            break
        print(f"ADAPTER {a.DeviceName} | {a.DeviceString} | flags={a.StateFlags}")
        j = 0
        while True:
            m = DISPLAY_DEVICE()
            m.cb = ctypes.sizeof(m)
            if not user32.EnumDisplayDevicesW(a.DeviceName, j, ctypes.byref(m), 0):
                break
            print(f"  MON {m.DeviceString} | flags={m.StateFlags} | {m.DeviceID}")
            j += 1
        i += 1


if __name__ == "__main__":
    print("PING:", repr(send_pipe("PING", 1.0)))
    time.sleep(0.3)
    print("SETDISPLAYCOUNT 1:", repr(send_pipe("SETDISPLAYCOUNT 1", 2.0)[:300]))
    time.sleep(5)
    list_adapters()
