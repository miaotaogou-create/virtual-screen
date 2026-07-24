@echo off
setlocal
cd /d "%~dp0.."

REM 用 3.12 打包更稳；找不到则回退到 py
set "PY=C:\Users\49358\AppData\Local\Programs\Python\Python312\python.exe"
if not exist "%PY%" set "PY=python"

"%PY%" -m pip install -q pyinstaller
if errorlevel 1 exit /b 1

for /f "delims=" %%i in ('"%PY%" -c "import sys; from pathlib import Path; print(Path(sys.base_prefix)/'tcl'/'tcl8.6')"') do set "TCL86=%%i"
for /f "delims=" %%i in ('"%PY%" -c "import sys; from pathlib import Path; print(Path(sys.base_prefix)/'tcl'/'tk8.6')"') do set "TK86=%%i"

echo TCL=%TCL86%
echo TK=%TK86%

"%PY%" -m PyInstaller --noconfirm --clean --onefile --windowed --name VirtualScreen ^
  --paths . ^
  --runtime-hook hooks\pyi_rth_tcltk_fix.py ^
  --add-data "%TCL86%;_tcl_data" ^
  --add-data "%TK86%;_tk_data" ^
  launch.py
if errorlevel 1 exit /b 1

copy /Y config.example.json dist\config.example.json >nul
echo.
echo 输出: %cd%\dist\VirtualScreen.exe
endlocal
