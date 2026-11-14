@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\install-autoplayer.ps1"
if errorlevel 1 (
  echo.
  echo The build/install did not complete. See the error above.
  pause
  exit /b 1
)
echo.
echo ECWolf Autoplayer installed and desktop shortcut created.
pause
