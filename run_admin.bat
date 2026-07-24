@echo off
cd /d "%~dp0"
powershell -NoProfile -Command "Start-Process -FilePath python -ArgumentList '-m','vscreen' -WorkingDirectory '%cd%' -Verb RunAs"
