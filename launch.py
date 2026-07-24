"""入口：在导入 tkinter 之前强制指定 Tcl/Tk 目录（防止运行时钩子顺序问题）。"""

from __future__ import annotations

import os
import sys


def _fix_tcl_tk() -> None:
    if not getattr(sys, "frozen", False):
        return
    base = getattr(sys, "_MEIPASS", "")
    if not base:
        return

    def find(name: str) -> str | None:
        for dirpath, _, files in os.walk(base):
            if name in files:
                return dirpath
        return None

    tcl = find("init.tcl")
    tk = find("tk.tcl")
    if tcl:
        os.environ["TCL_LIBRARY"] = tcl
    if tk:
        os.environ["TK_LIBRARY"] = tk
    # 若仍找不到，直接给出可读错误，避免 Tcl 抛难懂堆栈
    if not tcl or not os.path.isfile(os.path.join(tcl, "init.tcl")):
        raise SystemExit(
            "便携包缺少 Tcl(init.tcl)。请使用 scripts/build_exe.bat 用 Python 3.12 重新打包。"
        )


_fix_tcl_tk()

from vscreen.app import main  # noqa: E402

if __name__ == "__main__":
    main()
