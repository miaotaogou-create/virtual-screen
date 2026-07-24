@echo off
cd /d "%~dp0.."
python -m pip install -q pyinstaller
python -m PyInstaller --noconfirm --clean --onefile --windowed --name VirtualScreen --paths . launch.py
if errorlevel 1 exit /b 1
copy /Y config.example.json dist\config.example.json >nul
echo.
echo 输出: %cd%\dist\VirtualScreen.exe
