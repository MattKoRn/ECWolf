# ECWolf Windows x64 release

This edition adds a dark launcher for selecting user-supplied game data and mods,
widescreen resolution profiles, full-screen/windowed selection and optional
Autoplayer startup. The renderer remains ECWolf's established software raycaster:
higher resolution is not a replacement GPU renderer, shader system or new art.

## One-click source build and install

Clone the repository using Git on Windows 10/11 x64, and install CMake and
Visual Studio C++ Build Tools. Double-click BUILD-AND-INSTALL-ECWOLF.bat.
The existing build runs its regression checks and compiles/installs the engine.
The updated process then creates an ECWolf Launcher desktop shortcut and a
portable ZIP at build/windows-release/ECWolf-Windows-x64-Portable.zip.

## Portable ZIP and one-click installer

Extract the ZIP and double-click PLAY-ECWOLF.bat to play without installation.
Alternatively, double-click INSTALL-ECWOLF.bat to install under the current
user's LOCALAPPDATA/Programs/ECWolf directory and create a desktop shortcut.
No administrator rights are needed. This is a PowerShell-script installer, not
a signed MSI/EXE installer.

The packager and installer use an explicit allowlist for the engine executable,
its DLLs, ecwolf resource PK3s, license notice and launcher. Wolfenstein 3D,
Spear of Destiny, other commercial game data, private saves, configs, RPG
profiles and custom mods are excluded. Upgrades do not delete existing files.

## Playing

In the launcher, choose a folder containing complete, legally acquired
GAMEMAPS, MAPHEAD and VSWAP game data files in WL6, WL1 or SOD format.
Choose an optional folder containing mod archives, then check desired mods.
Set 1920x1080, 1280x720 or 640x480, choose fullscreen/windowed and play.
The launcher uses the engine's existing --data, --file, --res, --aspect and
--autoplay command-line options. Existing game config, saves and RPG profiles
are not changed. Launcher preferences only are kept in
%LOCALAPPDATA%/ECWolf/Launcher/settings.json.

## Testing and release limitations

Run tests/test-windows-release.ps1 to verify ZIP exclusions, installation
and preservation of existing files using synthetic fixtures. The Windows CI
workflow compiles the engine, runs regression tests and uploads the ZIP.
Commercial distribution still requires a full source/asset-license audit,
identity/branding review, code signing, a clean Windows 10/11 install test
and actual playthroughs using licensed data. Modern GPU graphics, replacement
art and a redesigned gameplay system remain future work.
