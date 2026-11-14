# One-click ECWolf RPG build + installation. Uses the shared incremental installer.
param(
    [string] $GameDataPath = "",
    [string] $InstallRoot = "",
    [switch] $Reconfigure
)

$ErrorActionPreference = "Stop"
if (-not $InstallRoot) {
    $InstallRoot = Join-Path $env:LOCALAPPDATA "Programs\ECWolf RPG"
}
& (Join-Path $PSScriptRoot "install-autoplayer.ps1") `
    -GameDataPath $GameDataPath `
    -InstallRoot $InstallRoot `
    -ShortcutName "ECWolf RPG" `
    -ShortcutDescription "ECWolf RPG - persistent progression, scaling monsters and offline XP" `
    -Reconfigure:$Reconfigure
