# Build and install ECWolf Autoplayer. Double-click the .bat file at the repo root.
param(
    [string] $GameDataPath = "",
    [string] $InstallRoot = "",
    [string] $ShortcutName = "ECWolf Autoplayer",
    [string] $ShortcutDescription = "ECWolf with F12 Autoplayer",
    [switch] $Reconfigure
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Run-Checked {
    param(
        [string] $Exe,
        [string[]] $Arguments,
        [string] $Stage = "command"
    )

    # Keep output visible while retaining the actual CMake/compiler errors.
    # Native programs can send normal diagnostics to stderr; do not treat
    # their stderr stream as a PowerShell terminating error.
    $ErrorActionPreference = "Continue"
    $logPath = Join-Path $script:logDirectory "$Stage.log"
    & $Exe @Arguments 2>&1 | Tee-Object -FilePath $logPath
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        $highlights = @(
            Select-String -LiteralPath $logPath -Pattern "CMake Error", "Could NOT find", "fatal error", "error C[0-9]+:", "error MSB[0-9]+:", "error LNK[0-9]+:", "NOTFOUND" |
                Select-Object -First 5 |
                ForEach-Object { $_.Line.Trim() }
        )
        $detail = if ($highlights.Count -gt 0) {
            "Key diagnostic lines:`n" + ($highlights -join "`n") + "`n"
        }
        else {
            "See the output immediately above for the underlying failure.`n"
        }
        $summary = "$Exe failed during '$Stage' (exit code $exitCode).`n$detail" + "Full log: $logPath"
        throw $summary
    }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$build = Join-Path $repo "build\autoplayer-win64"
$script:logDirectory = Join-Path $repo "build\autoplayer-logs"
New-Item -ItemType Directory -Path $script:logDirectory -Force | Out-Null
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
Run-Checked "git" @("-C", $repo, "submodule", "sync", "--recursive") -Stage "submodule-sync"
Run-Checked "git" @("-C", $repo, "submodule", "update", "--init", "--recursive") -Stage "submodule-update"

# CMake/Visual Studio incremental builds reuse compiled object files. Skip the
# configure pass for matching settings; the generated ZERO_CHECK target still
# regenerates when the project's CMake files change.
$cachePath = Join-Path $build "CMakeCache.txt"
$solutionPath = Join-Path $build "ECWolf.sln"
$needsConfigure = $true
if (-not $Reconfigure -and (Test-Path -LiteralPath $cachePath) -and (Test-Path -LiteralPath $solutionPath)) {
    $cacheValues = @{}
    foreach ($line in Get-Content -LiteralPath $cachePath) {
        if ($line -match '^([^/#][^:]*):[^=]+=(.*)$') {
            $cacheValues[$Matches[1]] = $Matches[2]
        }
    }

    $expectedValues = @{
        "CMAKE_GENERATOR"           = $generator
        "CMAKE_GENERATOR_PLATFORM"  = "x64"
        "CMAKE_HOME_DIRECTORY"      = $repo
        "CMAKE_INSTALL_PREFIX"      = $InstallRoot
        "INTERNAL_ZLIB"             = "ON"
        "INTERNAL_BZIP2"            = "ON"
        "INTERNAL_JPEG"             = "ON"
        "INTERNAL_SDL"              = "ON"
        "INTERNAL_SDL_MIXER"        = "ON"
        "INTERNAL_SDL_NET"          = "ON"
        "INTERNAL_SDL_MIXER_CODECS" = "ON"
    }

    $needsConfigure = $false
    foreach ($name in $expectedValues.Keys) {
        if (-not $cacheValues.ContainsKey($name)) {
            $needsConfigure = $true
            break
        }
        $actual = $cacheValues[$name]
        $expected = $expectedValues[$name]
        if ($name -eq "CMAKE_HOME_DIRECTORY" -or $name -eq "CMAKE_INSTALL_PREFIX") {
            $actual = $actual.Replace('\', '/').TrimEnd('/')
            $expected = $expected.Replace('\', '/').TrimEnd('/')
        }
        if ($actual -ine $expected) {
            $needsConfigure = $true
            break
        }
    }
    if ($cacheValues.ContainsKey("CMAKE_SUPPRESS_REGENERATION") -and $cacheValues["CMAKE_SUPPRESS_REGENERATION"] -eq "ON") {
        $needsConfigure = $true
    }
}

if ($needsConfigure) {
    Write-Host "=== Configuring with $generator ==="
    Run-Checked "cmake" @(
        "-S", $repo, "-B", $build, "-G", $generator, "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$InstallRoot",
        "-DINTERNAL_ZLIB=ON", "-DINTERNAL_BZIP2=ON", "-DINTERNAL_JPEG=ON",
        "-DINTERNAL_SDL=ON", "-DINTERNAL_SDL_MIXER=ON", "-DINTERNAL_SDL_NET=ON",
        # Use bundled codecs instead of requiring a system OpusFile installation.
        "-DINTERNAL_SDL_MIXER_CODECS=ON"
    ) -Stage "configure"
}
else {
    Write-Host "=== Reusing existing CMake configuration ==="
}

Write-Host "=== Incremental compilation (only changed sources and dependencies) ==="
# No --clean-first: the build tool recompiles changed sources and affected dependencies.
Run-Checked "cmake" @("--build", $build, "--config", "Release", "--parallel") -Stage "build"
Write-Host "=== Installing to $InstallRoot ==="
Run-Checked "cmake" @("--install", $build, "--config", "Release", "--prefix", $InstallRoot) -Stage "install"

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
$shortcut = $shell.CreateShortcut((Join-Path $desktop "$ShortcutName.lnk"))
$shortcut.TargetPath = $exe
$shortcut.WorkingDirectory = Split-Path $exe -Parent
$shortcut.IconLocation = "$exe,0"
$shortcut.Description = $ShortcutDescription
$shortcut.Save()

Write-Host ""
Write-Host "Installation complete: $exe" -ForegroundColor Green
Write-Host "Desktop shortcut: $ShortcutName.lnk"
Write-Host "Launch ECWolf with your legally owned Wolf3D/Spear game data."
Write-Host "In a running single-player level, press F12 or enable Autoplayer in Controls."
