# Unified one-click ECWolf RPG + Autoplayer build and desktop shortcut.
param(
    [string] $GameDataPath = "",
    [string] $InstallRoot = "",
    [switch] $Reconfigure
)
$ErrorActionPreference = "Stop"
if (-not $InstallRoot) {
    $InstallRoot = Join-Path $env:LOCALAPPDATA "Programs\ECWolf RPG Autoplayer"
}
& (Join-Path $PSScriptRoot "install-autoplayer.ps1") `
    -GameDataPath $GameDataPath `
    -InstallRoot $InstallRoot `
    -ShortcutName "ECWolf RPG + Autoplayer" `
    -ShortcutDescription "ECWolf: F12 Autoplayer, persistent RPG progression and equipment" `
    -Reconfigure:$Reconfigure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Add the modern launcher shortcut and generate a user-data-free ZIP.
$repo = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'package-windows.ps1') -InstallRoot $InstallRoot -OutputDirectory (Join-Path $repo 'build\windows-release')
Write-Host 'ECWolf Launcher and Windows x64 portable ZIP are ready.'
