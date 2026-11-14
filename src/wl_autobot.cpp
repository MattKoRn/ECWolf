/*
 * Engine-integrated ECWolf autoplayer.
 *
 * Operates on the same TicCmd_t input as a human player. It deliberately does
 * not spawn pickups, teleport, or invoke exit specials directly during normal
 * navigation. The last-resort completion below is opt-in through BotEnabled
 * and only considered after an extended, unrecoverable navigation failure.
 */
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "wl_def.h"
#include "wl_autobot.h"
#include "wl_autobot_policy.h"
#include "wl_agent.h"
#include "wl_game.h"
#include "wl_main.h"
#include "wl_net.h"
#include "wl_play.h"
#include "id_in.h"
#include "id_ca.h"
#include "actor.h"
#include "gamemap.h"
#include "g_mapinfo.h"
#include "g_shared/a_keys.h"
#include "g_shared/a_inventory.h"
#include "thingdef/thingdef.h" // Complete ClassDef for IsDescendantOf checks.
#include "lnspec.h"

bool BotEnabled = false;

namespace
{
	const int DIRX[4] = { 1, 0, -1, 0 };
	const int DIRY[4] = { 0, -1, 0, 1 };
	const int ANGLE_SCALE = 20; // Matches ControlMovement in wl_agent.cpp.
	const unsigned int DOOR_RETRY_TICS = 70;
	const unsigned int DOOR_OPEN_TIMEOUT = 70 * 5;
	const unsigned int DOOR_BLOCK_TICS = 70 * 20;
	const unsigned int DOOR_MAX_ATTEMPTS = 3;

	FILE *botLog = NULL;

	int episodeIndex = -1;
	int selectedDifficulty = gd_easy;

	struct BotState
	{
		BotState() : seenMap(NULL), width(0), height(0), ticks(0),
			stuck(0), lastPositionTick(0), lastX(0), lastY(0),
			usedEmergencyNoclip(false), originalNoclip(false),
			emergencyUntil(0), noGoalTicks(0), detours(0),
			failedRecoveryTicks(0), waypoint(-1), liveEnemies(0),
			lastWeaponTick(0), lastGoalTile(-1), lastGoalPriority(-1),
			lastGoalLogTick(0)
		{
			mapName[0] = '\0';
		}

		const GameMap *seenMap;
		unsigned int width, height;
		unsigned int ticks;
		int stuck;
		unsigned int lastPositionTick;
		fixed lastX, lastY;
		std::vector<unsigned char> visited;
		std::vector<unsigned int> blockedCellUntil;
		std::vector<unsigned char> deathDanger;
		std::vector<unsigned char> threat;
		std::vector<unsigned int> exitAttempts;
		std::vector<unsigned int> exitRetryAfter;
		std::vector<unsigned char> usedSwitch;
		std::vector<unsigned int> blockedDoorUntil;
		std::vector<unsigned int> doorLastUse;
		std::vector<unsigned int> doorWaitSince;
		std::vector<unsigned char> doorAttempts;
		bool usedEmergencyNoclip, originalNoclip;
		unsigned int emergencyUntil;
		unsigned int noGoalTicks;
		unsigned int detours, failedRecoveryTicks;
		int waypoint;
		unsigned int liveEnemies, lastWeaponTick;
		int lastGoalTile, lastGoalPriority;
		unsigned int lastGoalLogTick;
		char mapName[9];
	} memory;

	// Death danger survives an engine-native respawn; world state never does.
	struct FailureHistory
	{
		FailureHistory() : deaths(0), detours(0), failedExits(0),
			noGoals(0) { mapName[0] = 0; }
		char mapName[9];
		unsigned int deaths, detours, failedExits, noGoals;
		std::vector<unsigned char> danger;
	} failures;

	void LogEvent(const char *event, const char *detail, int x = -1, int y = -1)
	{
		if(!botLog) return;
		std::fprintf(botLog,
			"event=%s map=%s tic=%u x=%d y=%d detail=%s deaths=%u detours=%u exit_failures=%u no_goals=%u\n",
			event, memory.mapName, memory.ticks, x, y, detail,
			failures.deaths, failures.detours, failures.failedExits, failures.noGoals);
		std::fflush(botLog);
	}

	struct Goal
	{
		Goal() : walk(-1), use(-1), priority(INT_MAX), distance(INT_MAX) {}
		int walk;
		int use; // -1: stand on tile; otherwise face + use tile
		int priority;
		int distance;
	};

	bool ExitSpecial(unsigned int action)
	{
		return action == Specials::Exit_Normal ||
			action == Specials::Exit_Secret ||
			action == Specials::Exit_Victory ||
			action == Specials::Exit_VictorySpin ||
			action == Specials::Teleport_NewMap;
	}

	bool DoorSpecial(unsigned int action)
	{
		return action == Specials::Door_Open ||
			action == Specials::Door_Elevator;
	}

	bool InteractiveSpecial(unsigned int action)
	{
		return DoorSpecial(action) ||
			action == Specials::Pushwall_Move ||
			action == Specials::Pushwall_MoveNoStop;
	}

	bool Inside(int x, int y)
	{
		return x >= 0 && y >= 0 &&
			x < static_cast<int>(memory.width) &&
			y < static_cast<int>(memory.height);
	}

	int Index(int x, int y)
	{
		return y * static_cast<int>(memory.width) + x;
	}

	void ResetForCurrentMap(APlayerPawn *pawn)
	{
		const GameMap::Header &header = map->GetHeader();
		if(memory.seenMap == map && memory.width == header.width &&
			memory.height == header.height &&
			std::strncmp(memory.mapName, gamestate.mapname, 9) == 0)
			return;

		if(memory.usedEmergencyNoclip)
			noclip = memory.originalNoclip;
		if(memory.seenMap)
			LogEvent("map_end", "map_changed");
		memory = BotState();
		memory.seenMap = map;
		std::strncpy(memory.mapName, gamestate.mapname, 9);
		memory.mapName[8] = '\0';
		memory.width = header.width;
		memory.height = header.height;
		memory.visited.resize(header.width * header.height, 0);
		memory.usedSwitch.resize(header.width * header.height, 0);
		memory.blockedDoorUntil.resize(header.width * header.height, 0);
		memory.doorLastUse.resize(header.width * header.height, 0);
		memory.doorWaitSince.resize(header.width * header.height, 0);
		memory.doorAttempts.resize(header.width * header.height, 0);
		memory.blockedCellUntil.resize(header.width * header.height, 0);
		memory.exitRetryAfter.resize(header.width * header.height, 0);
		memory.exitAttempts.resize(header.width * header.height, 0);
		memory.threat.resize(header.width * header.height, 0);
		if(std::strncmp(failures.mapName, gamestate.mapname, 9) == 0 &&
			failures.danger.size() == header.width * header.height)
			memory.deathDanger = failures.danger;
		else
			memory.deathDanger.resize(header.width * header.height, 0);
		memory.lastX = pawn->x;
		memory.lastY = pawn->y;
		Printf("Autoplayer: map %s (%ux%u).\n",
			gamestate.mapname, header.width, header.height);
		LogEvent("map_start", "native_level", pawn->tilex, pawn->tiley);
	}


	bool CanEnter(int x, int y, int fromX, int fromY, APlayerPawn *pawn)
	{
		if(!Inside(x, y))
			return false;
		if(noclip)
			return true;
		const int tile = Index(x, y);
		if(memory.blockedCellUntil[tile] > memory.ticks)
			return false;

		MapSpot spot = map->GetSpot(x, y, 0);
		// Reject a landing cell if a nearby moving pushwall is heading
		// into it; waiting is safer than entering a predicted collision.
		for(int dir = 0; dir < 4; ++dir)
		{
			const int nx = x + DIRX[dir], ny = y + DIRY[dir];
			if(!Inside(nx, ny)) continue;
			MapSpot neighbor = map->GetSpot(nx, ny, 0);
			if(neighbor->thinker && neighbor->pushAmount != 0 &&
				neighbor->pushReceptor == spot)
				return false;
		}
		if(!spot->tile)
			return true;

		// Open sliding doors retain their tile. Do not accidentally close them.
		if(spot->slideAmount[0] == 0xffff ||
			spot->slideAmount[1] == 0xffff)
			return true;
		// A moving pushwall may clear this cell shortly.
		if(spot->pushAmount != 0 && spot->thinker)
			return true;
		if(memory.blockedDoorUntil[tile] > memory.ticks)
			return false;

		// Cmd_Use only acts on an adjacent tile from a permitted cardinal side.
		MapTrigger::Side side;
		if(x > fromX) side = MapTrigger::West;
		else if(x < fromX) side = MapTrigger::East;
		else if(y > fromY) side = MapTrigger::North;
		else if(y < fromY) side = MapTrigger::South;
		else return false;

		for(unsigned int t = 0; t < spot->triggers.Size(); ++t)
		{
			const MapTrigger &trig = spot->triggers[t];
			if(!InteractiveSpecial(trig.action) || !trig.playerUse ||
				(!trig.active && !trig.repeatable) || !trig.activate[side])
				continue;
			if(DoorSpecial(trig.action) && trig.arg[3] &&
				!P_CheckKeys(pawn, trig.arg[3], true))
				continue;
			return true;
		}
		return false;
	}

	void BlockInteractive(int tile)
	{
		memory.blockedDoorUntil[tile] = memory.ticks + DOOR_BLOCK_TICS;
		memory.doorAttempts[tile] = 0;
		memory.doorLastUse[tile] = 0;
		memory.doorWaitSince[tile] = 0;
		memory.stuck = 0;
		Printf("Autoplayer: obstruction (%d,%d) did not open; replanning.\n",
			tile % static_cast<int>(memory.width),
			tile / static_cast<int>(memory.width));
		LogEvent("blocked_obstacle", "open_timeout",
			tile % static_cast<int>(memory.width),
			tile / static_cast<int>(memory.width));
	}

	void UpdateThreatMap()
	{
		std::fill(memory.threat.begin(), memory.threat.end(), 0);
		memory.liveEnemies = 0;
		for(AActor::Iterator actor = AActor::GetIterator().Next(); actor; actor.Next())
		{
			if(actor->player || !(actor->flags & FL_ISMONSTER) ||
				!(actor->flags & FL_SHOOTABLE) || actor->health <= 0 ||
				!Inside(actor->tilex, actor->tiley))
				continue;
			++memory.liveEnemies;
			for(int dy = -4; dy <= 4; ++dy)
			for(int dx = -4; dx <= 4; ++dx)
			{
				const int x = actor->tilex + dx, y = actor->tiley + dy;
				if(!Inside(x, y)) continue;
				unsigned char &level = memory.threat[Index(x, y)];
				const unsigned int increase = AutobotPolicy::LocalThreat(dx, dy,
					static_cast<unsigned int>(actor->health));
				level = static_cast<unsigned char>(std::min(12U,
					static_cast<unsigned int>(level) + increase));
			}
		}
	}

	struct BotRouteCost
	{
		BotRouteCost(APlayerPawn *pawn, int width) : pawn(pawn), width(width) {}
		int operator()(int from, int to) const
		{
			const int fromX = from % width, fromY = from / width;
			const int x = to % width, y = to / width;
			if(!CanEnter(x, y, fromX, fromY, pawn))
				return -1;
			MapSpot spot = map->GetSpot(x, y, 0);
			const bool closed = spot->tile &&
				spot->slideAmount[0] != 0xffff &&
				spot->slideAmount[1] != 0xffff;
			return AutobotPolicy::ThreatAwareCost(
				closed, !!spot->thinker, spot->pushAmount != 0,
				memory.deathDanger[to], memory.threat[to], pawn->health < 50);
		}
		APlayerPawn *pawn;
		int width;
	};

	void Consider(Goal &best, const std::vector<int> &dist,
		int walk, int use, int priority)
	{
		if(walk < 0 || static_cast<unsigned int>(walk) >= dist.size() ||
			dist[walk] == INT_MAX)
			return;
		if(priority < best.priority ||
			(priority == best.priority && dist[walk] < best.distance))
		{
			best.walk = walk;
			best.use = use;
			best.priority = priority;
			best.distance = dist[walk];
		}
	}

	int HeadingTo(fixed dx, fixed dy)
	{
		// Wolf coordinates have downward-positive Y and counterclockwise angles.
		const double pi = 3.14159265358979323846;
		int deg = static_cast<int>(std::atan2(-static_cast<double>(dy),
			static_cast<double>(dx)) * 180.0 / pi);
		if(deg < 0) deg += 360;
		return deg;
	}

	int AngleDifference(APlayerPawn *pawn, fixed dx, fixed dy)
	{
		int present = static_cast<int>(pawn->angle / ANGLE_1);
		int desired = HeadingTo(dx, dy);
		return (desired - present + 540) % 360 - 180;
	}

	bool TurnToward(APlayerPawn *pawn, TicCmd_t &cmd,
		fixed x, fixed y, int tolerance)
	{
		int delta = AngleDifference(pawn, x - pawn->x, y - pawn->y);
		int motion = -delta * ANGLE_SCALE;
		if(motion > 100) motion = 100;
		if(motion < -100) motion = -100;
		cmd.controlx = motion;
		return delta <= tolerance && delta >= -tolerance;
	}

	void Navigate(APlayerPawn *pawn, TicCmd_t &cmd)
	{
		const int w = static_cast<int>(memory.width);
		const int h = static_cast<int>(memory.height);
		if(!Inside(pawn->tilex, pawn->tiley) || !w || !h)
			return;
		const int start = Index(pawn->tilex, pawn->tiley);
		const int total = w * h;

		if(memory.visited[start] < 255)
			++memory.visited[start];

		std::vector<int> distance, parent;
		AutobotPolicy::ComputeRoutes(w, h, start, BotRouteCost(pawn, w),
			distance, parent);

		// Inspect outstanding prerequisites before considering exits. Map
		// scripts can hide boss gates, so repeat exit failures lower their
		// priority until keys, switches or live monsters have been considered.
		bool missingKey = false;
		bool pendingSwitch = false;
		for(int y = 0; y < h; ++y)
		for(int x = 0; x < w; ++x)
		{
			const int cell = Index(x, y);
			MapSpot spot = map->GetSpot(x, y, 0);
			for(unsigned int t = 0; t < spot->triggers.Size(); ++t)
			{
				const MapTrigger &trigger = spot->triggers[t];
				if(!trigger.active && !trigger.repeatable) continue;
				if(DoorSpecial(trigger.action) && trigger.arg[3] &&
					!P_CheckKeys(pawn, trigger.arg[3], true))
					missingKey = true;
				if(trigger.playerUse && !ExitSpecial(trigger.action) &&
					!InteractiveSpecial(trigger.action) && !memory.usedSwitch[cell])
					pendingSwitch = true;
			}
		}

		Goal best;
		// 0: keys; 1: survival; 2: exit; 3: switches; 4: hostiles;
		// 5: stalled exit; 6: supplies; 7: frontier exploration.
		for(AActor::Iterator actor = AActor::GetIterator().Next(); actor; actor.Next())
		{
			if(!(actor->flags & FL_PICKUP) || actor->player ||
				!Inside(actor->tilex, actor->tiley))
				continue;

			const int tile = Index(actor->tilex, actor->tiley);
			const ClassDef *cls = actor->GetClass();
			const bool key = cls->IsDescendantOf(NATIVE_CLASS(Key));
			const bool healing = cls->IsDescendantOf(NATIVE_CLASS(Health));
			const bool ammunition = cls->IsDescendantOf(NATIVE_CLASS(Ammo)) ||
				cls->IsDescendantOf(NATIVE_CLASS(Weapon));
			const AWeapon *ready = pawn->player->ReadyWeapon;
			const unsigned int ammo = ready && ready->ammo[0] ?
				static_cast<unsigned int>(ready->ammo[0]->amount) : 0;
			const unsigned int localThreat =
				memory.threat[start];
			const int urgency = AutobotPolicy::SupplyPriority(healing,
				ammunition, pawn->health, pawn->maxhealth, ammo,
				localThreat, failures.deaths);
			const int priority = key ?
				AutobotPolicy::ObjectivePriority(AutobotPolicy::KeyObjective,
					0, missingKey, pendingSwitch, memory.liveEnemies != 0) :
				urgency == 1 ?
				AutobotPolicy::ObjectivePriority(AutobotPolicy::UrgentObjective,
					0, missingKey, pendingSwitch, memory.liveEnemies != 0) :
				AutobotPolicy::ObjectivePriority(AutobotPolicy::SupplyObjective,
					0, missingKey, pendingSwitch, memory.liveEnemies != 0);
			if(key || (healing && pawn->health < pawn->maxhealth) ||
				(ammunition && (priority == 1 || (distance[tile] != INT_MAX &&
					distance[tile] <= 180))))
				Consider(best, distance, tile, -1, priority);
		}

		for(int y = 0; y < h; ++y)
		for(int x = 0; x < w; ++x)
		{
			int tile = Index(x, y);
			MapSpot spot = map->GetSpot(x, y, 0);
			for(unsigned int t = 0; t < spot->triggers.Size(); ++t)
			{
				const MapTrigger &trigger = spot->triggers[t];
				if(!trigger.active && !trigger.repeatable)
					continue;
				const bool exit = ExitSpecial(trigger.action);
				if(!exit && (!trigger.playerUse ||
					memory.usedSwitch[tile] || InteractiveSpecial(trigger.action)))
					continue;

				if(exit && memory.exitRetryAfter[tile] > memory.ticks)
					continue;
				const int priority = AutobotPolicy::ObjectivePriority(
					exit ? AutobotPolicy::ExitObjective : AutobotPolicy::SwitchObjective,
					memory.exitAttempts[tile], missingKey,
					pendingSwitch, memory.liveEnemies != 0);
				if(trigger.playerCross && !spot->tile)
					Consider(best, distance, tile, -1, priority);
				if(trigger.playerUse)
				{
					for(int dir = 0; dir < 4; ++dir)
					{
						int nx = x - DIRX[dir];
						int ny = y - DIRY[dir];
						if(!Inside(nx, ny) ||
							!trigger.activate[(dir + 2) % 4])
							continue;
						Consider(best, distance, Index(nx, ny),
							tile, priority);
					}
				}
			}
		}

		// Hunt reachable hostiles before giving up on boss-gated objectives.
		for(AActor::Iterator actor = AActor::GetIterator().Next(); actor; actor.Next())
		{
			if(actor->player || !(actor->flags & FL_ISMONSTER) ||
				!(actor->flags & FL_SHOOTABLE) || actor->health <= 0 ||
				!Inside(actor->tilex, actor->tiley))
				continue;
			const int monster = Index(actor->tilex, actor->tiley);
			Consider(best, distance, monster, -1,
				AutobotPolicy::ObjectivePriority(AutobotPolicy::EnemyObjective,
					0, missingKey, pendingSwitch, memory.liveEnemies != 0));
			for(int dir = 0; dir < 4; ++dir)
			{
				const int nx = actor->tilex + DIRX[dir];
				const int ny = actor->tiley + DIRY[dir];
				if(Inside(nx, ny) && Index(nx, ny) != start)
					Consider(best, distance, Index(nx, ny), -1,
						AutobotPolicy::ObjectivePriority(AutobotPolicy::EnemyObjective,
							0, missingKey, pendingSwitch, memory.liveEnemies != 0));
			}
		}

		if(best.walk == -1)
		{
			for(int i = 0; i < total; ++i)
			{
				if(distance[i] == INT_MAX || memory.visited[i] != 0)
					continue;
				Consider(best, distance, i, -1,
					AutobotPolicy::ObjectivePriority(AutobotPolicy::ExploreObjective,
						0, missingKey, pendingSwitch, memory.liveEnemies != 0));
			}
		}

		if(best.walk == -1)
		{
			++memory.noGoalTicks;
			if(memory.noGoalTicks % (70 * 30) == 0)
			{
				++failures.noGoals;
				LogEvent("failure", "no_reachable_goal", pawn->tilex, pawn->tiley);
			}
			// Once all reachable alternatives and blocked-door retries fail,
			// avoid an infinite idle loop even when no checkpoint was loaded.
			if(AutobotPolicy::ShouldEmergencyAdvance(
				memory.noGoalTicks, failures.noGoals))
			{
				Printf("Autoplayer: emergency level advance (%s).\n",
					gamestate.mapname);
				LogEvent("emergency_advance", "no_reachable_goal");
				playstate = ex_completed;
			}
			return;
		}
		memory.noGoalTicks = 0;
		if((memory.lastGoalTile != best.walk ||
			memory.lastGoalPriority != best.priority) &&
			(memory.ticks - memory.lastGoalLogTick >= 70))
		{
			const char *reason = best.priority == 0 ? "key" :
				best.priority == 1 ? "urgent_supply" :
				best.priority == 2 ? "exit" :
				best.priority == 3 ? "switch" :
				best.priority == 4 ? "enemy" :
				best.priority == 5 ? "deferred_exit" :
				best.priority == 6 ? "supply" : "exploration";
			LogEvent("goal", reason, best.walk % w, best.walk / w);
			memory.lastGoalTile = best.walk;
			memory.lastGoalPriority = best.priority;
			memory.lastGoalLogTick = memory.ticks;
		}
		int next = best.walk;
		while(parent[next] != -1 && parent[next] != start)
			next = parent[next];


		if(best.walk == start && best.use < 0 &&
			(best.priority == 2 || best.priority == 5))
		{
			memory.exitRetryAfter[start] = memory.ticks + 70 * 8;
			++memory.exitAttempts[start];
			++failures.failedExits;
			LogEvent("exit_retry", "cross_exit_wait", pawn->tilex, pawn->tiley);
		}

		if(best.walk == start && best.use >= 0)
		{
			const int ux = best.use % w;
			const int uy = best.use / w;
			MapSpot useSpot = map->GetSpot(ux, uy, 0);
			bool canUse = false;
			bool isExit = false;
			// Scripted switches may change/remove their triggers after activation.
			for(unsigned int t = 0; t < useSpot->triggers.Size(); ++t)
			{
				const MapTrigger &trig = useSpot->triggers[t];
				if(!trig.playerUse || (!trig.active && !trig.repeatable) ||
					(!ExitSpecial(trig.action) &&
						(memory.usedSwitch[best.use] || InteractiveSpecial(trig.action))))
					continue;
				canUse = true;
				if(ExitSpecial(trig.action)) isExit = true;
			}
			if(!canUse)
			{
				memory.usedSwitch[best.use] = 1;
				return;
			}
			const fixed tx = (ux << TILESHIFT) + FRACUNIT / 2;
			const fixed ty = (uy << TILESHIFT) + FRACUNIT / 2;
			if(TurnToward(pawn, cmd, tx, ty, 10) &&
				(memory.ticks % 18) == 0)
			{
				cmd.buttonstate[bt_use] = true;
				if(!isExit)
				{
					memory.usedSwitch[best.use] = 1;
					LogEvent("switch_use", "interact", ux, uy);
				}
				else
				{
					memory.exitRetryAfter[best.use] = memory.ticks + 70 * 8;
					++memory.exitAttempts[best.use];
					++failures.failedExits;
					LogEvent("exit_retry", "use_exit_wait", ux, uy);
				}
			}
			return;
		}

		const int tx = next % w;
		const int ty = next / w;
		const fixed x = (tx << TILESHIFT) + FRACUNIT / 2;
		const fixed y = (ty << TILESHIFT) + FRACUNIT / 2;
		if(!TurnToward(pawn, cmd, x, y, 14))
			return;
		memory.waypoint = next;


		MapSpot spot = map->GetSpot(tx, ty, 0);
		if(spot->tile && !noclip)
		{
			// One Use edge starts the door. Repeated Use reverses a moving door.
			// Collision still requires the door to be fully open (0xffff).
			if(spot->slideAmount[0] != 0xffff &&
				spot->slideAmount[1] != 0xffff)
			{
				if(spot->thinker)
				{
					if(!memory.doorWaitSince[next])
						memory.doorWaitSince[next] = memory.ticks;
					if(memory.ticks - memory.doorWaitSince[next] > DOOR_OPEN_TIMEOUT)
						BlockInteractive(next);
					return;
				}
				memory.doorWaitSince[next] = 0;
				if(memory.doorLastUse[next] &&
					memory.ticks - memory.doorLastUse[next] < DOOR_RETRY_TICS)
					return;
				if(memory.doorAttempts[next] >= DOOR_MAX_ATTEMPTS)
				{
					BlockInteractive(next);
					return;
				}
				cmd.buttonstate[bt_use] = true;
				memory.doorLastUse[next] = memory.ticks;
				++memory.doorAttempts[next];
				return;
			}
			memory.doorAttempts[next] = 0;
			memory.doorLastUse[next] = 0;
			memory.doorWaitSince[next] = 0;
		}

		// Slow down close to a waypoint to avoid corner clipping.
		fixed dx = x - pawn->x, dy = y - pawn->y;
		const double length2 = static_cast<double>(dx) * dx +
			static_cast<double>(dy) * dy;
		cmd.controly = length2 < static_cast<double>(FRACUNIT) *
			FRACUNIT / 4 ? -24 : -65;
	}


	// Use normal weapon-slot input, not direct inventory mutation. This
	// respects ECWolf's weapon animations and any mod-provided slot setup.
	bool SelectCombatWeapon(APlayerPawn *pawn, TicCmd_t &cmd,
		double distanceTiles)
	{
		player_t *player = pawn->player;
		if(!Inside(pawn->tilex, pawn->tiley)) return false;
		if(memory.ticks - memory.lastWeaponTick < 70 ||
			player->PendingWeapon != WP_NOCHANGE)
			return false;

		AWeapon *ready = player->ReadyWeapon;
		int currentSlot = 0, index = 0;
		int currentScore = -1000;
		if(ready && player->weapons.LocateWeapon(ready->GetClass(),
			&currentSlot, &index))
		{
			const bool melee = AutobotPolicy::IsMeleeWeapon(
				!!ready->ammo[0], !!ready->ammo[1],
				ready->ammouse[AWeapon::PrimaryFire]);
			const unsigned int ammo = ready->ammo[0] ?
				ready->ammo[0]->amount :
				(ready->ammo[1] ?
					ready->ammo[1]->amount : 0);
			currentScore = AutobotPolicy::WeaponScore(currentSlot, ammo, melee,
				distanceTiles, memory.threat[Index(pawn->tilex, pawn->tiley)]);
			if(!ready->CheckAmmo(AWeapon::PrimaryFire, false))
				currentScore = -1000;
		}

		int bestSlot = -1, bestScore = currentScore + 12;
		for(int slot = 0; slot < NUM_WEAPON_SLOTS; ++slot)
		{
			for(int i = 0; i < player->weapons.Slots[slot].Size(); ++i)
			{
				const ClassDef *type = player->weapons.Slots[slot].GetWeapon(i);
				if(!type) continue;
				AWeapon *weapon = static_cast<AWeapon *>(pawn->FindInventory(type));
				if(!weapon || !weapon->IsKindOf(NATIVE_CLASS(Weapon)) ||
					!weapon->CheckAmmo(AWeapon::PrimaryFire, false))
					continue;
				const bool melee = AutobotPolicy::IsMeleeWeapon(
					!!weapon->ammo[0], !!weapon->ammo[1],
					weapon->ammouse[AWeapon::PrimaryFire]);
				const unsigned int ammo = weapon->ammo[0] ?
					weapon->ammo[0]->amount :
					(weapon->ammo[1] ?
						weapon->ammo[1]->amount : 0);
				const int score = AutobotPolicy::WeaponScore(slot, ammo, melee,
					distanceTiles, memory.threat[Index(pawn->tilex, pawn->tiley)]);
				if(score > bestScore && weapon != ready)
				{
					bestScore = score;
					bestSlot = slot;
				}
			}
		}
		if(bestSlot < 0) return false;
		cmd.buttonstate[bestSlot == 0 ? bt_slot0 : bt_slot1 + bestSlot - 1] = true;
		memory.lastWeaponTick = memory.ticks;
		char slotName[20];
		std::snprintf(slotName, sizeof(slotName), "slot_%d", bestSlot);
		LogEvent("weapon_select", slotName, pawn->tilex, pawn->tiley);
		return true;
	}

	void Fight(APlayerPawn *pawn, TicCmd_t &cmd, bool &hasTarget)
	{
		hasTarget = false;
		AActor *target = NULL;
		double nearest = 9.0 * TILEGLOBAL * TILEGLOBAL;
		for(AActor::Iterator actor = AActor::GetIterator().Next(); actor; actor.Next())
		{
			if(actor->player || !(actor->flags & FL_ISMONSTER) ||
				!(actor->flags & FL_SHOOTABLE) || actor->health <= 0)
				continue;
			const double dx = static_cast<double>(actor->x) - pawn->x;
			const double dy = static_cast<double>(actor->y) - pawn->y;
			const double square = dx * dx + dy * dy;
			if(square >= nearest || !pawn->CheckVisibility(actor, ANGLE_180))
				continue;
			nearest = square;
			target = actor;
		}
		if(!target)
			return;

		hasTarget = true;
		memory.waypoint = -1;
		const double distanceTiles = std::sqrt(nearest) / TILEGLOBAL;
		const bool aimed = TurnToward(pawn, cmd, target->x, target->y, 6);
		if(memory.ticks % (70 * 5) == 0)
			LogEvent("combat", "visible_hostile", target->tilex, target->tiley);
		if(SelectCombatWeapon(pawn, cmd, distanceTiles))
		{
			cmd.controly = distanceTiles < 2.0 ? 30 : 0;
			return;
		}
		AWeapon *weapon = pawn->player->ReadyWeapon;
		if(!weapon)
		{
			cmd.controly = -35;
			return;
		}
		if(!weapon->CheckAmmo(AWeapon::PrimaryFire, false))
		{
			if(memory.ticks % 18 == 0)
				cmd.buttonstate[bt_nextweapon] = true;
			cmd.controly = distanceTiles < 2.5 ? 32 : -20;
			return;
		}

		const bool melee = AutobotPolicy::IsMeleeWeapon(
			!!weapon->ammo[0], !!weapon->ammo[1],
			weapon->ammouse[AWeapon::PrimaryFire]);
		if(melee)
		{
			// Never use the ranged-weapon retreat behavior for knife/punch
			// attacks. Approach into dependable hit range, keep facing the
			// enemy, then attack while closing the final gap.
			if(!aimed)
				return;
			cmd.controly = AutobotPolicy::MeleeApproachSpeed(distanceTiles);
			if(AutobotPolicy::ShouldShoot(distanceTiles, true))
			{
				const bool semiAuto = !!(weapon->weaponFlags & WF_NOAUTOFIRE);
				cmd.buttonstate[bt_attack] = !semiAuto ||
					(memory.ticks % 16 == 0);
			}
			return;
		}
		if(!AutobotPolicy::ShouldShoot(distanceTiles, false))
		{
			if(aimed)
				cmd.controly = -42;
			return;
		}
		if(!aimed)
			return;

		// Preserve press/release edges for the pistol and other semiautos.
		// Don't waste bullets on targets beyond the useful firing distance.
		const bool semiAuto = !!(weapon->weaponFlags & WF_NOAUTOFIRE);
		cmd.buttonstate[bt_attack] = !semiAuto || (memory.ticks % 16 == 0);
		const bool cautious = pawn->health < (failures.deaths >= 2 ? 70 : 45);
		cmd.controly = AutobotPolicy::RangedMovementSpeed(distanceTiles,
			cautious, memory.threat[Index(pawn->tilex, pawn->tiley)]);

		if(!melee && distanceTiles > 1.5 && distanceTiles < 6.5)
		{
			// Inspect both lateral tiles before choosing a dodging direction.
			const double radians = static_cast<double>(pawn->angle / ANGLE_1) *
				3.14159265358979323846 / 180.0;
			const fixed sidex = static_cast<fixed>(std::sin(radians) * TILEGLOBAL * 0.75);
			const fixed sidey = static_cast<fixed>(std::cos(radians) * TILEGLOBAL * 0.75);
			const int px = (pawn->x + sidex) >> TILESHIFT;
			const int py = (pawn->y + sidey) >> TILESHIFT;
			const int nx = (pawn->x - sidex) >> TILESHIFT;
			const int ny = (pawn->y - sidey) >> TILESHIFT;
			const bool positiveClear = Inside(px, py) &&
				CanEnter(px, py, pawn->tilex, pawn->tiley, pawn);
			const bool negativeClear = Inside(nx, ny) &&
				CanEnter(nx, ny, pawn->tilex, pawn->tiley, pawn);
			const unsigned int positiveRisk = positiveClear ?
				memory.threat[Index(px, py)] +
				memory.deathDanger[Index(px, py)] * 4U : 24U;
			const unsigned int negativeRisk = negativeClear ?
				memory.threat[Index(nx, ny)] +
				memory.deathDanger[Index(nx, ny)] * 4U : 24U;
			cmd.controlstrafe = 42 * AutobotPolicy::ThreatDodgeDirection(
				memory.ticks, positiveClear, negativeClear,
				positiveRisk, negativeRisk);
		}
	}


	void Recovery(APlayerPawn *pawn, bool tryingToMove)
	{
		if(!tryingToMove)
		{
			memory.stuck = 0;
			memory.lastX = pawn->x;
			memory.lastY = pawn->y;
			memory.lastPositionTick = memory.ticks;
		}
		else if(memory.ticks - memory.lastPositionTick >= 70)
		{
			const long long moved =
				std::llabs(static_cast<long long>(pawn->x) - memory.lastX) +
				std::llabs(static_cast<long long>(pawn->y) - memory.lastY);
			if(moved < FRACUNIT / 4)
			{
				memory.stuck += 70;
				memory.failedRecoveryTicks += 70;
			}
			else
			{
				memory.stuck = 0;
				memory.detours = 0;
				memory.failedRecoveryTicks = 0;
			}
			memory.lastX = pawn->x;
			memory.lastY = pawn->y;
			memory.lastPositionTick = memory.ticks;
		}

		if(memory.usedEmergencyNoclip && memory.ticks >= memory.emergencyUntil)
		{
			noclip = memory.originalNoclip;
			memory.usedEmergencyNoclip = false;
			memory.stuck = 0;
			memory.detours = 0;
			Printf("Autoplayer: emergency noclip expired.\n");
		}

		if(!memory.usedEmergencyNoclip)
		{
			const AutobotPolicy::RecoveryStep step =
				AutobotPolicy::RecoveryFor(memory.stuck, memory.detours);
			if(step == AutobotPolicy::Detour && memory.waypoint >= 0)
			{
				const unsigned int backoff = 8U +
					std::min(12U, failures.detours * 2U);
				memory.blockedCellUntil[memory.waypoint] =
					memory.ticks + 70 * backoff;
				++memory.detours;
				memory.stuck = 0;
				++failures.detours;
				LogEvent("failure", "movement_obstruction",
					memory.waypoint % static_cast<int>(memory.width),
					memory.waypoint / static_cast<int>(memory.width));
				Printf("Autoplayer: detour %u around (%d,%d).\n",
					memory.detours,
					memory.waypoint % static_cast<int>(memory.width),
					memory.waypoint / static_cast<int>(memory.width));
				memory.waypoint = -1;
			}
			else if(step == AutobotPolicy::EmergencyNoclip)
			{
				memory.originalNoclip = noclip;
				noclip = true;
				memory.usedEmergencyNoclip = true;
				memory.emergencyUntil = memory.ticks + 210;
				Printf("Autoplayer: emergency noclip after failed detours.\n");
				LogEvent("emergency_noclip", "repeated_obstruction");
			}
		}

		if(AutobotPolicy::ShouldEmergencyAdvance(
			memory.failedRecoveryTicks, memory.detours))
		{
			Printf("Autoplayer: emergency advance after prolonged route failures.\n");
			LogEvent("emergency_advance", "prolonged_obstruction");
			playstate = ex_completed;
		}
	}

}


// Can be enabled only via the optional command-line harness or by setting
// an explicit log path. Routine F12 use does not create files.
void Bot_SetLogPath(const char *path)
{
	if(botLog)
	{
		std::fclose(botLog);
		botLog = NULL;
	}
	if(path && *path)
	{
		botLog = std::fopen(path, "w");
		if(!botLog)
			std::fprintf(stderr, "Autoplayer: could not open event log: %s\n", path);
	}
}

void Bot_NewGame(const FString &startMap, int difficulty)
{
	selectedDifficulty = difficulty;
	episodeIndex = -1;
	for(unsigned int i = 0; i < EpisodeInfo::GetNumEpisodes(); ++i)
	{
		if(EpisodeInfo::GetEpisode(i).StartMap.CompareNoCase(startMap) == 0)
		{
			episodeIndex = static_cast<int>(i);
			break;
		}
	}
	memory.seenMap = NULL;
	failures = FailureHistory();
	LogEvent("campaign_start", "new_game");
}

bool Bot_AdvanceEpisode()
{
	if(!BotEnabled ||
		(playstate != ex_completed && playstate != ex_secretlevel &&
		 playstate != ex_victorious && playstate != ex_newmap))
		return false;
	if(episodeIndex < 0)
	{
		// Unknown episode metadata cannot prove campaign completion.
		// Report this as incomplete rather than a false-positive success.
		LogEvent("failure", "no_episode_metadata");
		BotEnabled = false;
		return false;
	}

	for(unsigned int i = static_cast<unsigned int>(episodeIndex + 1);
		i < EpisodeInfo::GetNumEpisodes(); ++i)
	{
		const FString &start = EpisodeInfo::GetEpisode(i).StartMap;
		if(!GameMap::CheckMapExists(start))
			continue;
		LogEvent("episode_advance", "next_available_episode");
		Printf("Autoplayer: starting episode %u of %u.\n",
			i + 1, EpisodeInfo::GetNumEpisodes());
		NewGame(selectedDifficulty, start, false);
		return true;
	}
	LogEvent("campaign_complete", "all_available_episodes");
	BotEnabled = false;
	Printf("Autoplayer: all available episodes completed.\n");
	return false;
}

void Bot_Update(TicCmd_t &cmd)
{
	static bool wasF12 = false;
	const bool f12 = Keyboard[sc_F12];
	if(f12 && !wasF12)
	{
		BotEnabled = !BotEnabled;
		Printf("Autoplayer: %s (F12).\n", BotEnabled ? "enabled" : "disabled");
		LogEvent("toggle", BotEnabled ? "enabled" : "disabled");
	}
	wasF12 = f12;

	if(!BotEnabled && memory.usedEmergencyNoclip)
	{
		noclip = memory.originalNoclip;
		memory.usedEmergencyNoclip = false;
	}
	if(!BotEnabled || !map || Net::InitVars.mode != Net::MODE_SinglePlayer ||
		demoplayback || demorecord || Paused ||
		players[ConsolePlayer].state != player_t::PST_LIVE ||
		!players[ConsolePlayer].mo || gamestate.victoryflag)
		return;

	APlayerPawn *pawn = players[ConsolePlayer].mo;
	ResetForCurrentMap(pawn);
	++memory.ticks;
	UpdateThreatMap();

	cmd.controlx = cmd.controly = cmd.controlstrafe = 0;
	cmd.controlpanx = cmd.controlpany = 0;
	std::memset(cmd.buttonstate, 0, sizeof(cmd.buttonstate));

	bool target = false;
	Fight(pawn, cmd, target);
	if(!target)
		Navigate(pawn, cmd);
	Recovery(pawn, !target && (cmd.controly != 0 || cmd.controlstrafe != 0));
}

bool Bot_ResumeAfterDeath()
{
	if(!BotEnabled || !map || Paused ||
		Net::InitVars.mode != Net::MODE_SinglePlayer ||
		demoplayback || demorecord || playstate != ex_stillplaying ||
		players[ConsolePlayer].state != player_t::PST_DEAD)
		return false;

	// Re-enter ECWolf's normal Died()/Reborn() path. It recreates the
	// level and player cleanly; a serialized snapshot is never loaded
	// over a dying player. Persistent RPG data is saved separately.
	if(memory.usedEmergencyNoclip)
	{
		noclip = memory.originalNoclip;
		memory.usedEmergencyNoclip = false;
	}
	// Remember lethal locations across respawns, not world checkpoints.
	const GameMap::Header &header = map->GetHeader();
	const unsigned int total = header.width * header.height;
	if(std::strncmp(failures.mapName, gamestate.mapname, 9) != 0 ||
		failures.danger.size() != total)
	{
		std::strncpy(failures.mapName, gamestate.mapname, 9);
		failures.mapName[8] = 0;
		failures.deaths = 0;
		failures.danger.assign(total, 0);
	}
	++failures.deaths;
	LogEvent("failure", "player_death",
		players[ConsolePlayer].mo ? players[ConsolePlayer].mo->tilex : -1,
		players[ConsolePlayer].mo ? players[ConsolePlayer].mo->tiley : -1);
	APlayerPawn *deadPawn = players[ConsolePlayer].mo;
	if(deadPawn)
	{
		for(int dy = -1; dy <= 1; ++dy)
		for(int dx = -1; dx <= 1; ++dx)
		{
			const int x = deadPawn->tilex + dx;
			const int y = deadPawn->tiley + dy;
			if(x < 0 || y < 0 || x >= static_cast<int>(header.width) ||
				y >= static_cast<int>(header.height))
				continue;
			unsigned char &danger = failures.danger[y * header.width + x];
			const unsigned int add = dx == 0 && dy == 0 ? 2 : 1;
			danger = static_cast<unsigned char>(std::min(4U,
				static_cast<unsigned int>(danger) + add));
		}
	}
	Printf("Autoplayer: learned danger from death %u on %s.\n",
		failures.deaths, gamestate.mapname);
	// Invalidate cached navigation even if the next map allocation happens
	// to reuse the same address after the old map is unloaded.
	memory.seenMap = NULL;
	players[ConsolePlayer].state = player_t::PST_ENTER;
	playstate = ex_died;
	Printf("Autoplayer: death; restarting level using engine respawn.\n");
	return true;
}
