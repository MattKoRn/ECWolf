# Per-user installer shipped inside the portable ZIP. Never removes user data.
param([string]$InstallRoot='',[switch]$NoShortcut)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not $InstallRoot) { $InstallRoot=Join-Path $env:LOCALAPPDATA 'Programs\ECWolf' }
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'ecwolf.exe') -PathType Leaf)) {
    throw 'Extract all files from the ECWolf ZIP before installing.'
}
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'launcher\ecwolf-launcher.ps1'))) {
    throw 'Missing launcher files in the extracted ZIP.'
}
if ([IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') -ieq [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')) {
    throw 'Choose an install destination distinct from the extracted ZIP directory.'
}
New-Item -ItemType Directory -Path $InstallRoot -Force | Out-Null
foreach ($item in @(Get-ChildItem -LiteralPath $PSScriptRoot -File)) {
    if ($item.Name -eq 'ecwolf.exe' -or $item.Extension.ToLowerInvariant() -eq '.dll' -or
        $item.Name -like 'ecwolf.pk3' -or $item.Name -eq 'licenses.txt') {
        Copy-Item -LiteralPath $item.FullName -Destination $InstallRoot -Force
    }
}
$launcher=Join-Path $InstallRoot 'launcher'
New-Item -ItemType Directory -Path $launcher -Force | Out-Null
foreach ($name in @('ecwolf-launcher.ps1','game-data-validation.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "launcher\$name") -Destination $launcher -Force
}
if (-not $NoShortcut) {
    $desktop=[Environment]::GetFolderPath('DesktopDirectory')
    if (-not $desktop) { throw 'Desktop folder unavailable.' }
    $shell=New-Object -ComObject WScript.Shell
    $link=$shell.CreateShortcut((Join-Path $desktop 'ECWolf Launcher.lnk'))
    $link.TargetPath=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $link.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $launcher 'ecwolf-launcher.ps1')+'"'
    $link.WorkingDirectory=$InstallRoot
    $link.IconLocation=(Join-Path $InstallRoot 'ecwolf.exe')+',0'
    $link.Description='ECWolf launcher'
    $link.Save()
}
Write-Host "ECWolf installed to $InstallRoot"
