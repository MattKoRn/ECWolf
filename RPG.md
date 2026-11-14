# ECWolf RPG — persistent progression

An engine-integrated, UT2004RPG-inspired single-player progression layer. This
lives alongside the existing F12 autoplayer and ordinary ECWolf save system.

## Play

1. Build and install on Windows by double-clicking
   `BUILD-AND-INSTALL-RPG.bat` from a Git clone with submodules.
2. Launch the **ECWolf RPG** desktop shortcut with your legally owned Wolfenstein
   3D / Spear of Destiny game data.
3. Open **Options → RPG Progression**. Choose **View RPG Profile**, manually
   spend points, or select **Auto Assign All Points** for a balanced allocation.
4. Kill enemies to earn permanent RPG XP. Levels grant five permanent stat points.
   Each successive level requires more XP: **100 + 40 × current level + 20 × current level²**
   (160 XP at level 1, 260 at level 2, 400 at level 3, and 800 at level 5).
   The quadratic curve uses arbitrary-precision values; existing saved levels,
   accumulated XP and stat points are preserved.
5. Relaunch the game to see an in-game summary of XP earned while offline.

## Permanent stats

| Stat | Actual gameplay effect |
| --- | --- |
| Damage | Higher outgoing weapon damage |
| Vitality | Increased player maximum health |
| Defense | Reduced incoming damage; at least one damage always gets through |
| Recovery | Regenerates health once per in-game second |
| Fortune | Increases earned XP and critical-hit chance |
| Agility | Increases movement speed, with a safety limit of 1.5x |

Enemies spawn with more health and moderate speed variation based on accumulated
player progression, with random ± difficulty variation; enemy attacks also
scale with player power, while XP awards use enemy baseline and player level with
a randomized bonus.

## Persistence and limits

- RPG data is saved silently to `Documents/ECWolfRPG/profile.rpg`. The
  Documents folder follows the operating system's folder location on Windows.
- The RPG profile is **account-wide** and persists across new games, ordinary
  saves, deaths, maps, and sessions. It is intentionally not reset or rolled
  back by loading an older map save.
- XP, level, unspent points, and all six stat values use arbitrary-precision
  *decimal integers* persisted as strings; they are not limited by a 32-bit or
  64-bit integer maximum. Display abbreviations continue after K, M, B and T
  using an extendable alphabetic scheme (aa, ab, …).
- The offline reward uses actual elapsed time between saves and launches.
  There is no configured maximum offline duration. It pays 15 base XP per
  minute, then applies the Fortune multiplier. Clock rollback is ignored.
- Autosaves occur after stat spending, on startup/shutdown, and periodically
  during play, without taking gameplay screenshots or opening dialogs.
  Data is written through a temporary file then replaced.
- Gameplay-facing hit points, damage, and movement convert arbitrary-precision
  values into safe engine numeric ranges. Movement buffs and progression
  resolution per frame are throttled to prevent simulation breakage. **These are
  safety limits, not level or stat purchase caps.**
- The account-wide progression layer is currently restricted to single-player
  to avoid desynchronizing multiplayer simulation. Multiplayer RPG synchronization,
  equipment affixes, and exotic powers are future extension points, not yet
  implemented.

## Files / integration

The standalone progression engine is `src/wl_rpg.cpp` and
`src/wl_rpg.h`. Its hooks live in:

- `src/wl_main.cpp`: boot and shutdown persistence;
- `src/wl_play.cpp`: periodic autosave / healing, offline popup;
- `src/wl_menu.cpp`: RPG menu, profile and allocation controls;
- `src/actor.cpp`: monster spawn strength and kill XP;
- `src/wl_state.cpp`: weapon damage scaling;
- `src/wl_agent.cpp`: enemy damage, defense, speed.

The one-click Windows installer delegates to the existing incremental CMake
builder, installs into `%LOCALAPPDATA%\Programs\ECWolf RPG`, and makes
an **ECWolf RPG** desktop shortcut. It requires CMake, Git, Visual Studio
C++ Build Tools, and the checkout's SDL submodules. Commercial game data
is not included; use the script's `-GameDataPath` option if needed.

Existing ECWolf save files remain separate and are not modified by the RPG
profile writer.
