@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-ECWolf.ps1"
if errorlevel 1 (
  echo Installation failed.
  pause
  exit /b 1
)
echo ECWolf installed. Open the ECWolf Launcher desktop shortcut.
pause
