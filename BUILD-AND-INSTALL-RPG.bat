@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\install-rpg.ps1"
if errorlevel 1 (
  echo.
  echo ECWolf RPG build/install failed. Check the output and logs above.
  pause
  exit /b 1
)
echo.
echo ECWolf RPG installed. Desktop shortcut: ECWolf RPG.
pause
