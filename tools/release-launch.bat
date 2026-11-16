@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher\ecwolf-launcher.ps1"
if errorlevel 1 (
  echo Launcher failed.
  pause
  exit /b 1
)
