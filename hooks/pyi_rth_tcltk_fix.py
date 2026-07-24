# PyInstaller 运行时：在 _MEIPASS 内递归查找 Tcl/Tk，再设置环境变量
import os
import sys


def _find_file(root: str, name: str) -> str | None:
    for dirpath, _dirs, files in os.walk(root):
        if name in files:
            return dirpath
    return None


if getattr(sys, "frozen", False):
    base = getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    tcl = _find_file(base, "init.tcl")
    tk = _find_file(base, "tk.tcl")
    if tcl:
        os.environ["TCL_LIBRARY"] = tcl
    if tk:
        os.environ["TK_LIBRARY"] = tk
