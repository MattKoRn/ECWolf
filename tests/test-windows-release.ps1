# Windows release regression tests. Uses only synthetic, non-proprietary fixtures.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$temp=Join-Path ([IO.Path]::GetTempPath()) ('ecwolf-release-' + [Guid]::NewGuid().ToString('N'))
$fake=Join-Path $temp 'built-engine'
$out=Join-Path $temp 'output'
$installed=Join-Path $temp 'new-install'
try {
    New-Item -ItemType Directory -Path $fake -Force | Out-Null
    foreach ($name in @('ecwolf.exe','ecwolf.pk3','SDL2.dll',
                          'GAMEMAPS.WL6','MAPHEAD.WL6','VSWAP.WL6',
                          'user.cfg','profile.rpg','private-mod.pk3')) {
        Set-Content -LiteralPath (Join-Path $fake $name) -Value 'synthetic test data'
    }
    & (Join-Path $repo 'tools\package-windows.ps1') -InstallRoot $fake -OutputDirectory $out -SkipShortcut
    $stage=Join-Path $out 'ECWolf-Windows-x64'
    $zip=Join-Path $out 'ECWolf-Windows-x64-Portable.zip'
    foreach ($name in @('ecwolf.exe','ecwolf.pk3','SDL2.dll','Install-ECWolf.ps1',
                         'INSTALL-ECWOLF.bat','PLAY-ECWOLF.bat',
                         'launcher\ecwolf-launcher.ps1','launcher\game-data-validation.ps1')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $name) -PathType Leaf)) {
            throw "Missing required release file: $name"
        }
    }
    foreach ($private in @('GAMEMAPS.WL6','MAPHEAD.WL6','VSWAP.WL6',
                           'user.cfg','profile.rpg','private-mod.pk3')) {
        if (Test-Path -LiteralPath (Join-Path $stage $private)) {
            throw "Private/licensed file leaked into release: $private"
        }
    }
    if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) { throw 'ZIP missing' }
    $expanded=Join-Path $temp 'unzipped'
    Expand-Archive -LiteralPath $zip -DestinationPath $expanded
    New-Item -ItemType Directory -Path $installed -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $installed 'existing-save.sav') -Value 'preserve this save'
    Set-Content -LiteralPath (Join-Path $expanded 'GAMEMAPS.WL6') -Value 'synthetic secret'
    & (Join-Path $expanded 'Install-ECWolf.ps1') -InstallRoot $installed -NoShortcut
    foreach ($mustExist in @('ecwolf.exe','ecwolf.pk3','SDL2.dll',
                              'launcher\ecwolf-launcher.ps1','existing-save.sav')) {
        if (-not (Test-Path -LiteralPath (Join-Path $installed $mustExist))) {
            throw "Installer lost required file: $mustExist"
        }
    }
    if (Test-Path -LiteralPath (Join-Path $installed 'GAMEMAPS.WL6')) {
        throw 'Installer unexpectedly copied commercial game data'
    }
    & (Join-Path $installed 'launcher\ecwolf-launcher.ps1') -SmokeCheck
    Write-Host 'Windows release packaging and installation regression tests passed.'
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
