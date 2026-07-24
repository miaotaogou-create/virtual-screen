@echo off
setlocal
cd /d "%~dp0.."
set "TCL_LIBRARY="
set "TK_LIBRARY="
set "PY=C:\Users\49358\AppData\Local\Programs\Python\Python312\python.exe"
if not exist "%PY%" set "PY=python"
taskkill /F /IM VirtualScreen.exe >nul 2>&1
"%PY%" -m pip install -q pyinstaller
"%PY%" -m PyInstaller --noconfirm --clean VirtualScreen.spec
if errorlevel 1 exit /b 1
echo 输出: %cd%\dist\VirtualScreen.exe
endlocal
