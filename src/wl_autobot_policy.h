#ifndef __WL_AUTOBOT_POLICY_H__
#define __WL_AUTOBOT_POLICY_H__

// Pure, engine-independent decisions used by the autoplayer. Kept small so
// they can be regression-tested without shipping proprietary game data.
namespace AutobotPolicy
{
	enum RecoveryStep { KeepTrying, Detour, EmergencyNoclip };

	inline RecoveryStep RecoveryFor(unsigned int stalledTics, unsigned int detours)
	{
		if(stalledTics >= 280 && detours >= 3)
			return EmergencyNoclip;
		if(stalledTics >= 140 && detours < 3)
			return Detour;
		return KeepTrying;
	}

	inline int TravelCost(bool interactive, unsigned int danger)
	{
		if(danger > 4) danger = 4;
		return 10 + (interactive ? 18 : 0) + static_cast<int>(danger) * 24;
	}

	inline int StrafeDirection(unsigned int ticks, bool positiveClear, bool negativeClear)
	{
		const int preference = ((ticks / 90) % 2) == 0 ? 1 : -1;
		if(preference > 0 && positiveClear) return 1;
		if(preference < 0 && negativeClear) return -1;
		if(positiveClear) return 1;
		if(negativeClear) return -1;
		return 0;
	}

	inline bool ShouldShoot(double distanceTiles, bool melee)
	{
		// Avoid long-range ammo waste; melee only when the enemy is in reach.
		return distanceTiles <= (melee ? 1.3 : 5.5);
	}
}

#endif
