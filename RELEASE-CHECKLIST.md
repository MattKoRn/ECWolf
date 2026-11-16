# ECWolf commercial-release readiness checklist

This is an engineering checklist, **not** a statement that this fork has
passed certification or that commercial distribution rights have been obtained.

## Code and licensing
- [ ] Audit the source-port and all third-party component licenses; ship the
      correct source, copyright notices, attributions and other obligations.
- [ ] Review the rights to any ECWolf/Wolf4SDL-related names, logos, artwork,
      sounds, music, maps and other resources before marketing a product.
- [ ] Package only assets you made, properly licensed or are allowed to share.
      Do not ship Wolfenstein 3D or Spear of Destiny proprietary game data.
- [ ] Provide a straightforward path for players to import their own legally
      acquired game data; verify an incomplete set produces a useful error.

## Reproducible releases
- [ ] Windows: run `BUILD-AND-INSTALL-ECWOLF.bat` on a clean Windows machine
      with supported Visual Studio C++ tools, Git, CMake and SDL submodules.
- [ ] Confirm the executable is installed and the desktop shortcut points
      to that executable. Check incremental rebuild and `-Reconfigure`.
- [ ] Run Windows and Linux GitHub Actions workflows successfully on master.
      Capture build logs and record artifact hashes for tagged releases.
- [ ] Run `python -m unittest discover -s tests -p "test_autoplayer_harness.py" -v`.
- [ ] Run `cmake -S tests -B build/policy`, then build and run ctest.
- [ ] On Windows, run `tests/test-game-data-validation.ps1`.

## Licensed-data gameplay acceptance
- [ ] On a test system with *locally obtained licensed data*, run the real
      installed engine with Wolfenstein 3D (.WL6) and Spear (.SOD).
- [ ] Run the unattended harness for both campaigns, retaining per-campaign
      event and engine logs. A synthetic stub is **not** a full playthrough.
- [ ] Accept cheat-free completion only when the harness reports `completed`;
      reject timeouts, deaths without recovery, crashes, skipped maps, and
      runs that needed temporary noclip.
- [ ] Manually test F12 enable/disable, ammunition/pickup decisions, locked
      doors, moving pushwalls, bosses, deaths, episode transitions, and UI.
- [ ] Check that Emergency Cheats defaults to off; turn it on for a controlled
      stall, then turn it off and verify collision immediately restores.
- [ ] Verify RPG progress, offline XP, loot and save files survive upgrades
      and that no campaign test overwrites the user's ordinary RPG profile.

## Player safety and release polish
- [ ] Provide clear first-run setup, keyboard controls, accessible HUD display,
      save-data location, uninstall steps, and known limitations.
- [ ] Include crash diagnostics with privacy-conscious logs and no personal
      game-data uploads by default.
- [ ] Test Windows SmartScreen/signing, installer rollback and permissions,
      and a fresh-install workflow before public distribution.
