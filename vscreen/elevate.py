from __future__ import annotations

import ctypes
import sys


def is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def _quote(part: str) -> str:
    if not part:
        return '""'
    if any(c in part for c in ' \t"'):
        return '"' + part.replace('"', '\\"') + '"'
    return part


def relaunch_as_admin(extra_args: list[str] | None = None) -> bool:
    """弹 UAC 以管理员重新启动。成功发起返回 True。"""
    extra = extra_args or []
    if getattr(sys, "frozen", False):
        exe = sys.executable
        params = " ".join(_quote(a) for a in extra)
    else:
        exe = sys.executable
        params = " ".join(_quote(a) for a in ["-m", "vscreen", *extra])
    rc = ctypes.windll.shell32.ShellExecuteW(None, "runas", exe, params or None, None, 1)
    return int(rc) > 32


def ensure_admin_at_start() -> None:
    """启动时提权一次。成功则当前进程退出，由管理员实例接续；用户取消则继续普通权限。

    Windows 不允许程序静默自动同意 UAC，只能把多次弹窗收成启动时一次。
    """
    if is_admin():
        return
    args = list(sys.argv[1:])
    if relaunch_as_admin(args):
        raise SystemExit(0)
