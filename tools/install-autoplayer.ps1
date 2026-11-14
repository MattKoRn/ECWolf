# Build and install ECWolf Autoplayer. Double-click the .bat file at the repo root.
param(
    [string] $GameDataPath = "",
    [string] $InstallRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Run-Checked {
    param([string] $Exe, [string[]] $Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Exe failed with exit code $LASTEXITCODE."
    }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$build = Join-Path $repo "build\autoplayer-win64"
if (-not $InstallRoot) {
    $InstallRoot = Join-Path $env:LOCALAPPDATA "Programs\ECWolf Autoplayer"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "Install CMake and add it to PATH (https://cmake.org/download/), then try again."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Install Git for Windows (https://git-scm.com/downloads/win), then try again."
}

$programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
$vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Install Visual Studio 2022 Build Tools with the Desktop development with C++ workload."
}
$version = (& $vswhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion | Select-Object -First 1)
if (-not $version) {
    throw "C++ build tools are missing. Install the Desktop development with C++ workload."
}

$generator = switch (($version -split "\.")[0]) {
    "18" { "Visual Studio 18 2026" }
    "17" { "Visual Studio 17 2022" }
    "16" { "Visual Studio 16 2019" }
    default { throw "Unsupported Visual Studio version: $version" }
}

Write-Host "=== ECWolf Autoplayer: preparing source ==="
if (-not (Test-Path (Join-Path $repo ".git"))) {
    throw "Use a Git clone of this repository with submodules. ZIP archives omit required SDL submodules."
}
Run-Checked "git" @("-C", $repo, "submodule", "sync", "--recursive")
Run-Checked "git" @("-C", $repo, "submodule", "update", "--init", "--recursive")

Write-Host "=== Configuring with $generator ==="
Run-Checked "cmake" @(
    "-S", $repo, "-B", $build, "-G", $generator, "-A", "x64",
    "-DCMAKE_INSTALL_PREFIX=$InstallRoot",
    "-DINTERNAL_ZLIB=ON", "-DINTERNAL_BZIP2=ON", "-DINTERNAL_JPEG=ON",
    "-DINTERNAL_SDL=ON", "-DINTERNAL_SDL_MIXER=ON", "-DINTERNAL_SDL_NET=ON"
)
Write-Host "=== Compiling ==="
Run-Checked "cmake" @("--build", $build, "--config", "Release", "--parallel")
Write-Host "=== Installing to $InstallRoot ==="
Run-Checked "cmake" @("--install", $build, "--config", "Release", "--prefix", $InstallRoot)

# Commercial game data is never bundled; optionally copy the user's own data.
if ($GameDataPath) {
    if (-not (Test-Path $GameDataPath -PathType Container)) {
        throw "Game data directory does not exist: $GameDataPath"
    }
    Get-ChildItem -LiteralPath $GameDataPath -File |
        Where-Object { $_.Extension -match '^\.(WL1|WL6|SOD|SD1|SD2|SD3|SD4|N3D)$' } |
        Copy-Item -Destination $InstallRoot -Force
}

$exe = Join-Path $InstallRoot "ecwolf.exe"
if (-not (Test-Path $exe)) {
    $found = Get-ChildItem -Path $InstallRoot -Filter "ecwolf.exe" -File -Recurse | Select-Object -First 1
    if (-not $found) {
        throw "Build succeeded, but ecwolf.exe was not found under $InstallRoot."
    }
    $exe = $found.FullName
}

$desktop = [Environment]::GetFolderPath("DesktopDirectory")
if (-not $desktop) {
    throw "Unable to locate the Windows desktop folder."
}
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $desktop "ECWolf Autoplayer.lnk"))
$shortcut.TargetPath = $exe
$shortcut.WorkingDirectory = Split-Path $exe -Parent
$shortcut.IconLocation = "$exe,0"
$shortcut.Description = "ECWolf with F12 Autoplayer"
$shortcut.Save()

Write-Host ""
Write-Host "Installation complete: $exe" -ForegroundColor Green
Write-Host "Desktop shortcut: ECWolf Autoplayer.lnk"
Write-Host "Launch ECWolf with your legally owned Wolf3D/Spear game data."
Write-Host "In a running single-player level, press F12 or enable Autoplayer in Controls."
