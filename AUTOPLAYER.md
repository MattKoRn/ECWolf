# ECWolf Autoplayer (experimental)

This fork integrates an **opt-in** bot into the engine's input loop. It is intended
for single-player Wolfenstein 3D, Spear of Destiny, and ECWolf-compatible maps.

## Windows one-click build and installation

1. Install [Git for Windows](https://git-scm.com/downloads/win),
   [CMake](https://cmake.org/download/), and **Visual Studio 2022 Build
   Tools** with the *Desktop development with C++* workload. Visual Studio
   2019 and 2026 with C++ tools are also supported by the launcher.
2. Obtain a recursive clone:
   `git clone --recursive https://github.com/MattKoRn/ECWolf.git`
3. Double-click **BUILD-AND-INSTALL-AUTOPLAYER.bat**. If you already cloned
   the repository, run `git pull --ff-only` first to get installer fixes.
4. The installer builds a Release engine using bundled SDL_mixer codecs (including
   OpusFile, without requiring a separate OpusFile installation), installs it to
   `%LOCALAPPDATA%\Programs\ECWolf Autoplayer`, and creates
   **ECWolf Autoplayer.lnk** on your desktop.
5. Supply your legally owned Wolfenstein 3D / Spear of Destiny game data. Game
   data is not distributed with this repository. To copy your existing data
   automatically, run PowerShell from the repository root with:
   `powershell -ExecutionPolicy Bypass -File tools\install-autoplayer.ps1 -GameDataPath "C:\path\to\game\data"`

The installer stops with an error if required build dependencies are absent
or CMake fails. The default installation does **not** overwrite an existing
ECWolf installation.

**Incremental one-click builds:** The initial run compiles the full game and its
dependencies. Later runs reuse the `build/autoplayer-win64` directory, skip the
CMake configuration step when settings have not changed, and compile only changed
source files plus any files that depend on them. The build system automatically
regenerates its configuration when CMake project files change. The installer
still refreshes the installed game and desktop shortcut. Do not delete the build
directory if you want incremental compilation.

To explicitly rerun CMake configuration without discarding compiled files, use
`powershell -ExecutionPolicy Bypass -File tools\install-autoplayer.ps1 -Reconfigure`.

## Controls

- **F12** switches the autoplayer on or off during gameplay.
- **Options → Controls → Autoplayer (F12)** provides the same toggle.
- Start a game normally, then enable the bot. The bot is disabled by default.
- Bot movement is suppressed in demos, multiplayer, pause states and when the
  player is not alive.

## How it works

- Uses the real map geometry for breadth-first routing.
- Targets keys, essential pickups, reachable exit and teleporter triggers,
  and scripted switches; visits unexplored reachable floor tiles if needed.
- Operates normal player movement, Use, and Attack inputs rather than
  teleporting in normal play.
- Attempts to fight currently visible monsters before pursuing navigation.
- On unattended death, restarts the current level using ECWolf's native respawn
  flow instead of reloading a world snapshot. The account-wide RPG profile
  still saves automatically and persists between sessions.
- After repeated failed recovery, allows temporary noclip and ultimately
  an **emergency level advance**. These bypass rules **only** after the
  bot has failed to navigate normally. Do not enable autopilot if you want
  a strictly cheat-free game.
- Attempts to continue into the next available episode after an episode
  ending. Skips certain modal end screens in unattended mode.

## Known limitations and testing

**Experimental implementation — full game completion is not verified.** In
particular, mod-specific trigger scripting, boss-dependent exits, moving
pushwalls, timed puzzles, inventory decisions, and unusual key mechanics can
require additional handling. Route selection is tile-based and does not
yet model dynamic threats or guaranteed survivability. Death restarts the
current map without consuming a finite life while the bot is enabled;
scripted level skipping may occur as a last resort.

Automated compiler checks are defined in
`.github/workflows/autoplayer-windows.yml` and
`.github/workflows/autoplayer-linux-smoke.yml`, but a successful Windows
build and full-game automated playthrough **must be verified** before
treating this as production-ready. Inspect the GitHub Actions runs for
build diagnostics. A Windows machine with the toolchain and licensed game
data is required to validate gameplay.

## Combat and death regression checks

The bot pulses Attack for weapons with `NOAUTOFIRE` (such as the Wolf3D
pistol), while holding Attack for fully automatic weapons. This avoids
a pistol firing once and then waiting forever for a button release.

The auto-player deliberately does **not** load or create automatic world
checkpoints. A previous death-recovery path replaced the map and player with
a deserialized snapshot and was associated with black-screen crashes. Now the
bot exits `PlayLoop` with `ex_died` and ECWolf's own `Died()` /
`CheckSpawnPlayer()` machinery restarts the current level. Normal manual
save/load remains available. During unattended play the bot does not spend
finite lives, so repeated deaths do not end the run. Player health, position
and map progress reset upon death; the permanent `Documents/ECWolfRPG`
profile continues to autosave independently.

Manual test scenarios to validate on a machine with licensed game data:

1. Equip the pistol; confirm repeated shots against a living visible enemy.
2. Equip an automatic weapon; confirm continuous fire while a target is visible.
3. Die early in a level; confirm engine respawn with no checkpoint loading.
4. Die after clearing part of the map; confirm it restarts the current map.
5. Die repeatedly; confirm the bot resumes and doesn't hit a game-over screen.
6. Turn off the bot with F12; confirm ordinary player death remains unchanged.
7. Confirm the permanent RPG profile is unchanged and still autosaves after death.

These are regression scenarios, not a claim of an automated full playthrough.

## Recovery / troubleshooting

- If the game reports missing data: provide the original game's data files
  (ECWolf itself cannot supply them).
- If CMake fails to find SDL or a bundled codec: run
  `git submodule sync --recursive` and `git submodule update --init --recursive`.
  The installer runs both commands automatically. If you still see the old
  `Could NOT find OpusFile` error after updating, use the `-Reconfigure`
  switch shown above to refresh CMake without deleting compiled files.
- If PowerShell reports `cmake failed during 'configure'` or `'build'`, that
  is only the wrapper reporting a nonzero exit code. Look at the diagnostic
  lines included in the message or open the corresponding
  `build/autoplayer-logs/configure.log` or `build/autoplayer-logs/build.log`.
  Copy the first actual CMake or compiler error from that log when reporting
  an issue; the wrapper line alone cannot identify the failed dependency.
- If a moving wall, locked door, or mod-specific puzzle traps the bot, disable
  it with **F12**, resolve the obstacle, and re-enable it.
- If automatic recovery is undesirable, keep autopilot off or adjust the
  emergency fallback in `src/wl_autobot.cpp`.

The implementation intentionally keeps its own state outside savegame
serialization. Saves include the ordinary game state, not the bot's visit
history.
