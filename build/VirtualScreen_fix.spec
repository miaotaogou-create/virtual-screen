# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['..\\launch.py'],
    pathex=['.'],
    binaries=[],
    datas=[('C:\\Users\\49358\\AppData\\Local\\Programs\\Python\\Python312\\tcl\\tcl8.6', '_tcl_data'), ('C:\\Users\\49358\\AppData\\Local\\Programs\\Python\\Python312\\tcl\\tk8.6', '_tk_data')],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
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
    name='VirtualScreen_fix',
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
