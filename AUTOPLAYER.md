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
3. Double-click **BUILD-AND-INSTALL-AUTOPLAYER.bat**.
4. The installer builds a Release engine, installs it to
   `%LOCALAPPDATA%\Programs\ECWolf Autoplayer`, and creates
   **ECWolf Autoplayer.lnk** on your desktop.
5. Supply your legally owned Wolfenstein 3D / Spear of Destiny game data. Game
   data is not distributed with this repository. To copy your existing data
   automatically, run PowerShell from the repository root with:
   `powershell -ExecutionPolicy Bypass -File tools\install-autoplayer.ps1 -GameDataPath "C:\path\to\game\data"`

The installer stops with an error if required build dependencies are absent
or CMake fails. The default installation does **not** overwrite an existing
ECWolf installation.

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
- Saves a rolling checkpoint and can reload it after obstruction or death.
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
yet model dynamic threats or guaranteed survivability. Checkpoint retries
are limited; scripted level skipping may occur as a last resort.

Automated compiler checks are defined in
`.github/workflows/autoplayer-windows.yml` and
`.github/workflows/autoplayer-linux-smoke.yml`, but a successful Windows
build and full-game automated playthrough **must be verified** before
treating this as production-ready. Inspect the GitHub Actions runs for
build diagnostics. A Windows machine with the toolchain and licensed game
data is required to validate gameplay.

## Recovery / troubleshooting

- If the game reports missing data: provide the original game's data files
  (ECWolf itself cannot supply them).
- If CMake fails to find SDL: run `git submodule sync --recursive` and
  `git submodule update --init --recursive`.
- If a moving wall, locked door, or mod-specific puzzle traps the bot, disable
  it with **F12**, resolve the obstacle, and re-enable it.
- If automatic recovery is undesirable, keep autopilot off or adjust the
  emergency fallback in `src/wl_autobot.cpp`.

The implementation intentionally keeps its own state outside savegame
serialization. Saves include the ordinary game state, not the bot's visit
history.
