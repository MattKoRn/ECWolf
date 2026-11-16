@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\install-unified.ps1"
if errorlevel 1 (
  echo.
  echo ECWolf unified build/install failed. See build\autoplayer-logs for diagnostics.
  pause
  exit /b 1
)
echo.
echo ECWolf installed. Launcher shortcut and portable ZIP created.
pause
