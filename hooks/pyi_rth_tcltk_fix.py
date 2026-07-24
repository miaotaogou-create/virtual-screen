# 必须在 pyi_rth__tkinter 之后仍能纠正：递归找到 init.tcl / tk.tcl
import os
import sys


def _find(root: str, filename: str) -> str | None:
    for dirpath, _, files in os.walk(root):
        if filename in files:
            return dirpath
    return None


if getattr(sys, "frozen", False):
    base = getattr(sys, "_MEIPASS", "")
    if base:
        tcl = _find(base, "init.tcl")
        tk = _find(base, "tk.tcl")
        if tcl:
            os.environ["TCL_LIBRARY"] = tcl
        if tk:
            os.environ["TK_LIBRARY"] = tk
