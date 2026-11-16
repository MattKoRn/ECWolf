# Licensed game files remain user-supplied. No bundled game data.
# Use a small independently testable preflight before building or copying.
function Get-ECWolfGameDataFiles {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string] $Directory)

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        throw "Game data directory does not exist: $Directory"
    }
    $extensions = @(".WL1", ".WL6", ".SOD", ".SD1", ".SD2", ".SD3", ".SD4", ".N3D")
    $files = @(Get-ChildItem -LiteralPath $Directory -File |
        Where-Object { $extensions -contains $_.Extension.ToUpperInvariant() })
    $names = @{}
    foreach ($file in $files) {
        $names[$file.Name.ToUpperInvariant()] = $true
    }
    $hasCompleteSet = $false
    foreach ($extension in @("WL1", "WL6", "SOD")) {
        if ($names.ContainsKey("GAMEMAPS.$extension") -and
            $names.ContainsKey("MAPHEAD.$extension") -and
            $names.ContainsKey("VSWAP.$extension")) {
            $hasCompleteSet = $true
        }
    }
    if (-not $hasCompleteSet) {
        throw "No complete Wolf3D/Spear game data set found in $Directory. Supply GAMEMAPS, MAPHEAD and VSWAP files with matching .WL1, .WL6 or .SOD extensions from a game you own."
    }
    return $files
}
