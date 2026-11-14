#ifndef __WL_AUTOBOT_H__
#define __WL_AUTOBOT_H__

class FString;
struct TicCmd_t;

// Shared by the Controls menu and the F12 hotkey. Off by default.
extern bool BotEnabled;
// False by default: normal unattended play never enables noclip or skips maps.
extern bool BotAllowEmergencyCheats;
const char *Bot_StatusLine();

// Called once after normal input polling; never simulates OS key presses.
void Bot_Update(TicCmd_t &cmd);

// Episode bookkeeping is updated for both manual and automatic new games.
void Bot_NewGame(const FString &startMap, int difficulty);
bool Bot_AdvanceEpisode();

// Optional file-only structured telemetry for unattended campaign tests.
void Bot_SetLogPath(const char *path);

// Recover from bot death via ECWolf's existing level respawn path.
// No snapshot deserialization is performed during unattended gameplay.
bool Bot_ResumeAfterDeath();

#endif
