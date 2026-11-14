#ifndef __WL_AUTOBOT_H__
#define __WL_AUTOBOT_H__

class FString;
struct TicCmd_t;

// Shared by the Controls menu and the F12 hotkey. Off by default.
extern bool BotEnabled;

// Called once after normal input polling; never simulates OS key presses.
void Bot_Update(TicCmd_t &cmd);

// Episode bookkeeping is updated for both manual and automatic new games.
void Bot_NewGame(const FString &startMap, int difficulty);
bool Bot_AdvanceEpisode();

// Save at the PlayLoop frame boundary; request a reload there, but only
// load after PlayLoop exits (from GameLoop). Never replace actors mid-frame.
void Bot_SaveCheckpoint();
bool Bot_WantsReload();
bool Bot_ResumeAfterDeath();
bool Bot_ReloadCheckpoint();

#endif
