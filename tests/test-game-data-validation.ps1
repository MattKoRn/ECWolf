# Tests the same preflight used by the one-click Windows installer.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "..\tools\game-data-validation.ps1")
$root = Join-Path ([IO.Path]::GetTempPath()) ("ecwolf-data-test-" + [Guid]::NewGuid().ToString("N"))
try {
    $wolf = Join-Path $root "Wolf"
    $spear = Join-Path $root "Spear"
    New-Item -ItemType Directory -Path $wolf, $spear -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $wolf "GAMEMAPS.WL6") | Out-Null
    New-Item -ItemType File -Path (Join-Path $wolf "MAPHEAD.WL6") | Out-Null
    $rejected = $false
    try { Get-ECWolfGameDataFiles -Directory $wolf | Out-Null }
    catch { $rejected = $true }
    if (-not $rejected) { throw "Incomplete Wolf data was wrongly accepted" }
    New-Item -ItemType File -Path (Join-Path $wolf "VSWAP.WL6") | Out-Null
    New-Item -ItemType File -Path (Join-Path $wolf "notes.txt") | Out-Null
    $chosen = @(Get-ECWolfGameDataFiles -Directory $wolf)
    if ($chosen.Count -ne 3) { throw "Unexpected files selected for installation" }
    foreach ($name in @("GAMEMAPS.SOD", "MAPHEAD.SOD", "VSWAP.SOD")) {
        New-Item -ItemType File -Path (Join-Path $spear $name) | Out-Null
    }
    if (@(Get-ECWolfGameDataFiles -Directory $spear).Count -ne 3) {
        throw "Complete Spear data was rejected"
    }
    Write-Host "Licensed-game-data preflight regressions passed."
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
