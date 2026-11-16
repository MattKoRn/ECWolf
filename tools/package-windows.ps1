# Safe Windows x64 release packaging, excluding licensed content and user files.
param([Parameter(Mandatory=$true)][string]$InstallRoot,
      [string]$OutputDirectory='',[switch]$SkipShortcut)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $repo 'build\windows-release' }
$exe=Join-Path $InstallRoot 'ecwolf.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Missing $exe" }
if (@(Get-ChildItem -LiteralPath $InstallRoot -File -Filter 'ecwolf*.pk3').Count -eq 0) {
    throw 'Built ECWolf engine resource PK3 was not found.'
}
$installedLauncher=Join-Path $InstallRoot 'launcher'
New-Item -ItemType Directory -Path $installedLauncher -Force | Out-Null
foreach ($name in @('ecwolf-launcher.ps1','game-data-validation.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $installedLauncher -Force
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$stage=Join-Path $OutputDirectory 'ECWolf-Windows-x64'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($item in @(Get-ChildItem -LiteralPath $InstallRoot -File)) {
    if ($item.Name -eq 'ecwolf.exe' -or $item.Extension.ToLowerInvariant() -eq '.dll' -or
        $item.Name -like 'ecwolf*.pk3' -or $item.Name -eq 'licenses.txt') {
        Copy-Item -LiteralPath $item.FullName -Destination $stage
    }
}
Copy-Item -LiteralPath $installedLauncher -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'install-release.ps1') -Destination (Join-Path $stage 'Install-ECWolf.ps1')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release-install.bat') -Destination (Join-Path $stage 'INSTALL-ECWOLF.bat')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release-launch.bat') -Destination (Join-Path $stage 'PLAY-ECWOLF.bat')
Copy-Item -LiteralPath (Join-Path $repo 'WINDOWS-RELEASE.md') -Destination (Join-Path $stage 'README-WINDOWS.md')
$archive=Join-Path $OutputDirectory 'ECWolf-Windows-x64-Portable.zip'
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
if (-not $SkipShortcut) {
    $desktop=[Environment]::GetFolderPath('DesktopDirectory')
    if (-not $desktop) { throw 'Could not find the desktop folder.' }
    $shell=New-Object -ComObject WScript.Shell
    $link=$shell.CreateShortcut((Join-Path $desktop 'ECWolf Launcher.lnk'))
    $link.TargetPath=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $link.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $installedLauncher 'ecwolf-launcher.ps1')+'"'
    $link.WorkingDirectory=$InstallRoot
    $link.IconLocation="$exe,0"
    $link.Description='ECWolf modern launcher and mod selector'
    $link.Save()
}
Write-Host "Portable ZIP created: $archive"
