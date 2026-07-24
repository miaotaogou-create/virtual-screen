# PyInstaller 运行时：强制指向打包进 _MEIPASS 的 Tcl/Tk，避免找不到 init.tcl
import os
import sys


def _pick(*cands: str) -> str | None:
    for c in cands:
        if c and os.path.isfile(os.path.join(c, "init.tcl")):
            return c
        # tk 目录是 tk.tcl 而不是 init.tcl
        if c and os.path.isfile(os.path.join(c, "tk.tcl")):
            return c
    return None


if getattr(sys, "frozen", False):
    base = getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    tcl = _pick(
        os.path.join(base, "_tcl_data"),
        os.path.join(base, "tcl8.6"),
        os.path.join(base, "tcl", "tcl8.6"),
    )
    tk = _pick(
        os.path.join(base, "_tk_data"),
        os.path.join(base, "tk8.6"),
        os.path.join(base, "tcl", "tk8.6"),
    )
    if tcl:
        os.environ["TCL_LIBRARY"] = tcl
    if tk:
        os.environ["TK_LIBRARY"] = tk
