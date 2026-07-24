@echo off
setlocal
cd /d "%~dp0.."

set "PY=C:\Users\49358\AppData\Local\Programs\Python\Python312\python.exe"
if not exist "%PY%" (
  echo 需要 Python 3.12：%PY%
  exit /b 1
)

"%PY%" -m pip install -q pyinstaller
if errorlevel 1 exit /b 1

REM 先清掉可能被占用的旧 exe
taskkill /F /IM VirtualScreen.exe >nul 2>&1

"%PY%" -m PyInstaller --noconfirm --clean VirtualScreen.spec
if errorlevel 1 exit /b 1

copy /Y config.example.json dist\config.example.json >nul
echo.
echo 输出: %cd%\dist\VirtualScreen.exe
endlocal
