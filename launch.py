#!/usr/bin/env python3
"""启动虚拟屏 GUI（PyInstaller 单文件绿色版）。"""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path


def _prepare_frozen() -> None:
    """onefile：把 Tcl/Tk 拷到 LocalAppData（C:\\WINDOWS\\TEMP 下 Tcl 会拒读 init.tcl）。"""
    if not getattr(sys, "frozen", False):
        root = Path(__file__).resolve().parent
        if str(root) not in sys.path:
            sys.path.insert(0, str(root))
        return

    base = Path(getattr(sys, "_MEIPASS", Path(sys.executable).resolve().parent))
    if str(base) not in sys.path:
        sys.path.insert(0, str(base))

    src_tcl = next(
        (base / rel for rel in ("_tcl_data", "tcl8.6") if (base / rel / "init.tcl").is_file()),
        None,
    )
    src_tk = next(
        (base / rel for rel in ("_tk_data", "tk8.6") if (base / rel / "tk.tcl").is_file()),
        None,
    )
    if src_tcl is None or src_tk is None:
        raise FileNotFoundError(f"打包包内缺少 Tcl/Tk 数据: {base}")

    cache = Path(os.environ.get("LOCALAPPDATA") or Path.home() / "AppData" / "Local")
    cache = cache / "VirtualScreen" / "tcltk-8.6.15"
    dst_tcl = cache / "tcl8.6"
    dst_tk = cache / "tk8.6"
    marker = cache / ".ready"

    need_copy = True
    if marker.is_file() and (dst_tcl / "init.tcl").is_file() and (dst_tk / "tk.tcl").is_file():
        need_copy = False
    if need_copy:
        if cache.exists():
            shutil.rmtree(cache, ignore_errors=True)
        cache.mkdir(parents=True, exist_ok=True)
        shutil.copytree(src_tcl, dst_tcl)
        shutil.copytree(src_tk, dst_tk)
        marker.write_text("ok\n", encoding="utf-8")

    # 覆盖 PyInstaller rthook 指向 TEMP 的设置
    os.environ["TCL_LIBRARY"] = str(dst_tcl)
    os.environ["TK_LIBRARY"] = str(dst_tk)


def _smoke() -> int:
    """打包自检：真正创建 Tk，成功则写旁路标记。"""
    import tkinter as tk

    root = tk.Tk()
    root.withdraw()
    ver = str(root.tk.call("info", "patchlevel"))
    root.destroy()
    mark = (
        Path(sys.executable).with_suffix(".smoke_ok")
        if getattr(sys, "frozen", False)
        else Path("smoke_ok.txt")
    )
    tcl = os.environ.get("TCL_LIBRARY", "")
    mark.write_text(
        f"TK_OK {ver}\nMEIPASS={getattr(sys, '_MEIPASS', '')}\nTCL_LIBRARY={tcl}\n",
        encoding="utf-8",
    )
    if "\\WINDOWS\\TEMP\\" in tcl.upper().replace("/", "\\"):
        raise SystemExit(f"TCL 仍在 WINDOWS\\TEMP: {tcl}")
    print(f"SMOKE_OK {ver}")
    return 0


_prepare_frozen()

if __name__ == "__main__":
    if "--smoke" in sys.argv:
        raise SystemExit(_smoke())
    # 尽早声明 DPI，后续抓屏坐标才与系统逻辑分辨率一致
    from vscreen.win_display import ensure_dpi_aware

    ensure_dpi_aware()
    # 启动时提权一次：之后本会话应用/清除/装驱动不再反复弹 UAC
    # （Windows 无法静默自动点同意，只能减少弹窗次数）
    from vscreen.elevate import ensure_admin_at_start

    ensure_admin_at_start()
    from vscreen.app import main

    main()
