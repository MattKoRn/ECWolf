#ifndef __WL_AUTOBOT_POLICY_H__
#define __WL_AUTOBOT_POLICY_H__

#include <climits>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

// Pure, engine-independent decisions used by the autoplayer. Kept small so
// they can be regression-tested without shipping proprietary game data.
namespace AutobotPolicy
{
	enum RecoveryStep { KeepTrying, Detour, EmergencyNoclip };

	// Caller supplies an edge cost for each direction; -1 rejects an edge.
	// This permits door/key checks and moving obstacle checks at every tick.
	template<class EdgeCost>
	inline void ComputeRoutes(int width, int height, int start,
		const EdgeCost &cost, std::vector<int> &distance,
		std::vector<int> &parent)
	{
		if(width <= 0 || height <= 0 || start < 0 || start >= width * height)
		{
			distance.clear();
			parent.clear();
			return;
		}
		const int dx[4] = {1, 0, -1, 0};
		const int dy[4] = {0, -1, 0, 1};
		const int total = width * height;
		distance.assign(total, INT_MAX);
		parent.assign(total, -1);
		typedef std::pair<int, int> Node;
		std::priority_queue<Node, std::vector<Node>, std::greater<Node> > frontier;
		distance[start] = 0;
		frontier.push(Node(0, start));
		while(!frontier.empty())
		{
			const Node node = frontier.top();
			frontier.pop();
			const int at = node.second;
			if(node.first != distance[at]) continue;
			const int x = at % width, y = at / width;
			for(int dir = 0; dir < 4; ++dir)
			{
				const int nx = x + dx[dir], ny = y + dy[dir];
				if(nx < 0 || ny < 0 || nx >= width || ny >= height)
					continue;
				const int next = ny * width + nx;
				const int step = cost(at, next);
				if(step <= 0 || distance[at] > INT_MAX - step)
					continue;
				const int candidate = distance[at] + step;
				if(candidate >= distance[next]) continue;
				distance[next] = candidate;
				parent[next] = at;
				frontier.push(Node(candidate, next));
			}
		}
	}


	// Allow four genuine pathfinding retries before resorting to noclip.
	// The stalled timer resets after each detour, so the last-resort delay
	// is measured AFTER the fourth failed route.
	inline RecoveryStep RecoveryFor(unsigned int stalledTics, unsigned int detours)
	{
		if(stalledTics >= 70 * 8 && detours >= 4)
			return EmergencyNoclip;
		if(stalledTics >= 140 && detours < 4)
			return Detour;
		return KeepTrying;
	}

	// Completion must never depend on a brief obstruction or a temporary
	// scripted objective outage. These timeouts are measured in engine tics.
	inline bool ShouldEmergencyAdvance(unsigned int failedTics,
		unsigned int failedAttempts)
	{
		return failedTics >= 70 * 480 && failedAttempts >= 4;
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

	// A living monster applies a local, decaying risk value. Risk is
	// intentionally a cost, never a wall: the bot can still engage enemies.
	inline unsigned int LocalThreat(int dx, int dy, unsigned int toughness)
	{
		if(dx < 0) dx = -dx;
		if(dy < 0) dy = -dy;
		const int distance = dx + dy;
		if(distance > 4) return 0;
		return static_cast<unsigned int>(5 - distance) +
			(toughness >= 80 ? 2U : 0U);
	}

	inline int ThreatAwareCost(bool closed, bool moving, bool pushwall,
		unsigned int deathDanger, unsigned int nearbyThreat,
		bool lowHealth)
	{
		const int base = TravelCost(closed, deathDanger);
		if(nearbyThreat > 12) nearbyThreat = 12;
		return base + static_cast<int>(nearbyThreat) * (lowHealth ? 14 : 6) +
			(moving ? (pushwall ? 65 : 32) : 0);
	}

	enum ObjectiveKind { KeyObjective, UrgentObjective, ExitObjective,
		SwitchObjective, EnemyObjective, SupplyObjective, ExploreObjective };

	// After repeat failures, switches and surviving enemies are treated as
	// potential prerequisites of the exit. Keys stay highest priority.
	inline int ObjectivePriority(ObjectiveKind kind, unsigned int failedExits,
		bool missingKey, bool activeSwitches, bool livingEnemies)
	{
		switch(kind)
		{
			case KeyObjective: return 0;
			case UrgentObjective: return 1;
			case ExitObjective:
				return (failedExits >= 2 &&
					(missingKey || activeSwitches || livingEnemies)) ? 5 : 2;
			case SwitchObjective: return 3;
			case EnemyObjective: return 4;
			case SupplyObjective: return 6;
			case ExploreObjective: return 7;
		}
		return 7;
	}

	// Survival pickups become urgent as health/ammunition falls, and sooner
	// when nearby hostiles or repeat deaths make the situation dangerous.
	inline int SupplyPriority(bool healing, bool ammunition, int health,
		int maxHealth, unsigned int ammo, unsigned int localThreat,
		unsigned int deaths)
	{
		if(healing && maxHealth > 0 && health < maxHealth)
		{
			const int percent = health * 100 / maxHealth;
			if(percent < 50 || (percent < 65 && localThreat >= 3) ||
				(percent < 80 && (localThreat >= 5 || deaths >= 2)))
				return 1;
		}
		if(ammunition && (ammo <= 8 ||
			(localThreat >= 5 && ammo <= 16) ||
			(deaths >= 2 && ammo <= 18)))
			return 1;
		return 6;
	}

	inline int WeaponScore(int slot, unsigned int ammo, bool melee,
		double distanceTiles, unsigned int threat)
	{
		if(melee)
			return distanceTiles < 1.4 && threat < 5 ? 30 : -1000;
		if(ammo == 0) return -1000;
		int score = 30 + slot * 8;
		if(ammo < 5) score -= 70;
		else if(ammo < 12) score -= 25;
		if(distanceTiles > 4.0 && slot <= 1) score -= 15;
		if(threat >= 5 && slot >= 2) score += 12; // Stronger attacks under pressure.
		if(distanceTiles < 1.5 && slot >= 5) score -= 25;
		return score;
	}

	// Back away from close ranged threats, especially at low health. Attack
	// while retreating, rather than running into enemies or emptying the
	// magazine at ranges where shots do not land.
	inline int RangedMovementSpeed(double distanceTiles, bool lowHealth,
		unsigned int localThreat)
	{
		if(distanceTiles < 1.5) return lowHealth ? 60 : 38;
		if(distanceTiles < 2.5) return lowHealth ? 45 : 25;
		if(lowHealth && localThreat >= 5 && distanceTiles < 4.5)
			return 20;
		if(distanceTiles > 5.5) return -42;
		return 0;
	}

	inline int ThreatDodgeDirection(unsigned int ticks,
		bool positiveClear, bool negativeClear,
		unsigned int positiveDanger, unsigned int negativeDanger)
	{
		if(!positiveClear && !negativeClear) return 0;
		if(!positiveClear) return -1;
		if(!negativeClear) return 1;
		if(positiveDanger < negativeDanger) return 1;
		if(negativeDanger < positiveDanger) return -1;
		return StrafeDirection(ticks, positiveClear, negativeClear);
	}

	// Further reduce camera and weapon movement bob during unattended play:
	// one tenth of manual-play amplitude (previously one quarter).
	// Real movement, aiming and manual-play visuals remain unchanged.
	template<class T>
	inline T AutoPlayVisualBob(T bob, bool autoPlaying)
	{
		return autoPlaying ? bob / 10 : bob;
	}

	// Collect every reachable enemy ammo drop while there is room for it.
	// Priority 1 outranks exits; full inventory is not a collectible drop.
	inline int AmmoDropPriority(bool dropped, bool collectible,
		int normalPriority)
	{
		return dropped && collectible ? 1 : normalPriority;
	}

	// NOAUTOFIRE knives need a real release between presses. Start a short
	// attack burst as soon as the bot has an aligned close-range target, not
	// on a global clock which can repeatedly miss that opportunity.
	inline bool MeleeAttackInput(unsigned int attackTics, bool aligned,
		bool inRange)
	{
		return aligned && inRange && (attackTics % 15) < 3;
	}

	// Wolf3D Knife inherits an ammo type from WolfWeapon but overrides
	// primary ammouse to zero. Checking only null ammo slots mislabels it
	// as a firearm and triggers retreat instead of a close-range strike.
	inline bool IsMeleeWeapon(bool hasPrimaryAmmo, bool hasAltAmmo,
		unsigned int primaryAmmoUse)
	{
		return (!hasPrimaryAmmo && !hasAltAmmo) || primaryAmmoUse == 0;
	}

	// Knife/punch attacks should enter and STAY in actual striking range.
	// A_CustomPunch typically reaches about one tile; close in further
	// rather than stepping backwards as the previous ranged logic did.
	inline int MeleeApproachSpeed(double distanceTiles)
	{
		if(distanceTiles > 1.0) return -68;
		if(distanceTiles > 0.70) return -35;
		return 0;
	}

	inline bool ShouldShoot(double distanceTiles, bool melee)
	{
		// A little margin below engine melee range avoids whiffing.
		return distanceTiles <= (melee ? 1.05 : 5.5);
	}
}

#endif
