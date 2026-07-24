# -*- mode: python ; coding: utf-8 -*-
"""单文件 VirtualScreen.exe：Tcl/Tk 必须放到 _tcl_data / _tk_data（与 PyInstaller 运行时钩子一致）。"""

from pathlib import Path

from PyInstaller.building.api import EXE, PYZ
from PyInstaller.building.build_main import Analysis

ROOT = Path(SPECPATH).resolve()
PY_BASE = Path(r"C:\Users\49358\AppData\Local\Programs\Python\Python312")
TCL86 = PY_BASE / "tcl" / "tcl8.6"
TK86 = PY_BASE / "tcl" / "tk8.6"

if not (TCL86 / "init.tcl").is_file():
    raise SystemExit(f"找不到 Tcl: {TCL86}")
if not (TK86 / "tk.tcl").is_file():
    raise SystemExit(f"找不到 Tk: {TK86}")

# 目录名必须叫 _tcl_data / _tk_data，否则 pyi_rth__tkinter 会指到空目录
datas = [
    (str(TCL86), "_tcl_data"),
    (str(TK86), "_tk_data"),
    (str(ROOT / "config.example.json"), "."),
]

a = Analysis(
    [str(ROOT / "launch.py")],
    pathex=[str(ROOT)],
    binaries=[],
    datas=datas,
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[str(ROOT / "hooks" / "pyi_rth_tcltk_fix.py")],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="VirtualScreen",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
)
