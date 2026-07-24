# -*- mode: python ; coding: utf-8 -*-
"""用 Python 3.12 打包单文件 VirtualScreen.exe，显式打入 tcl8.6 / tk8.6。"""

from pathlib import Path

from PyInstaller.building.api import EXE, PYZ
from PyInstaller.building.build_main import Analysis
from PyInstaller.utils.hooks import collect_dynamic_libs

ROOT = Path(SPECPATH).resolve()
PY_BASE = Path(r"C:\Users\49358\AppData\Local\Programs\Python\Python312")
TCL86 = PY_BASE / "tcl" / "tcl8.6"
TK86 = PY_BASE / "tcl" / "tk8.6"

if not (TCL86 / "init.tcl").is_file():
    raise SystemExit(f"找不到 Tcl: {TCL86}")
if not (TK86 / "tk.tcl").is_file():
    raise SystemExit(f"找不到 Tk: {TK86}")

datas = [
    (str(TCL86), "tcl8.6"),
    (str(TK86), "tk8.6"),
    (str(ROOT / "config.example.json"), "."),
]

a = Analysis(
    [str(ROOT / "launch.py")],
    pathex=[str(ROOT)],
    binaries=collect_dynamic_libs("tkinter"),
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
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
