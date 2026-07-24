# -*- mode: python ; coding: utf-8 -*-
"""无 Tcl/Tk 的单文件绿色 exe（本地网页 UI）。"""

from pathlib import Path

from PyInstaller.building.api import EXE, PYZ
from PyInstaller.building.build_main import Analysis

ROOT = Path(SPECPATH).resolve()

a = Analysis(
    [str(ROOT / "launch.py")],
    pathex=[str(ROOT)],
    binaries=[],
    datas=[(str(ROOT / "config.example.json"), ".")],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=["tkinter", "_tkinter", "tcl", "tk"],
    noarchive=False,
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
