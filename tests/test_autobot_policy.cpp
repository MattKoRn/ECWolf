#include "../src/wl_autobot_policy.h"
#include <cassert>
#include <vector>
#include <climits>

struct TestGrid
{
	std::vector<int> entry;
	int operator()(int, int to) const { return entry[to]; }
};

int main()
{
	using namespace AutobotPolicy;
	TestGrid grid;
	grid.entry.assign(9, 10);
	std::vector<int> distance, parent;
	// Blocked cells force a detour; freeing the cell restores a direct route.
	grid.entry[4] = -1;
	ComputeRoutes(3, 3, 3, grid, distance, parent);
	assert(distance[5] == 40 && parent[5] != 4);
	grid.entry[4] = 10;
	ComputeRoutes(3, 3, 3, grid, distance, parent);
	assert(distance[5] == 20 && parent[5] == 4);
	// Death hazards are avoided, but a modest door penalty is acceptable.
	grid.entry[4] = TravelCost(false, 3);
	ComputeRoutes(3, 3, 3, grid, distance, parent);
	assert(distance[5] == 40 && parent[5] != 4);
	grid.entry[4] = TravelCost(true, 0);
	ComputeRoutes(3, 3, 3, grid, distance, parent);
	assert(distance[5] == 38 && parent[5] == 4);
	grid.entry.assign(9, -1);
	ComputeRoutes(3, 3, 3, grid, distance, parent);
	assert(distance[5] == INT_MAX);
	assert(RecoveryFor(139, 0) == KeepTrying);
	assert(RecoveryFor(140, 0) == Detour);
	assert(RecoveryFor(279, 3) == KeepTrying);
	assert(RecoveryFor(280, 3) == EmergencyNoclip);
	assert(TravelCost(false, 0) == 10);
	assert(TravelCost(true, 0) > TravelCost(false, 0));
	assert(TravelCost(false, 2) > TravelCost(true, 0));
	assert(TravelCost(false, 100) == TravelCost(false, 4));
	assert(StrafeDirection(0, true, true) == 1);
	assert(StrafeDirection(90, true, true) == -1);
	assert(StrafeDirection(0, false, true) == -1);
	assert(StrafeDirection(90, true, false) == 1);
	assert(StrafeDirection(0, false, false) == 0);
	assert(ShouldShoot(5.5, false));
	assert(!ShouldShoot(7.0, false));
	assert(ShouldShoot(1.0, true));
	assert(!ShouldShoot(3.0, true));
	assert(LocalThreat(0, 0, 50) == 5);
	assert(LocalThreat(1, 1, 100) == 5);
	assert(LocalThreat(5, 0, 100) == 0);
	assert(ThreatAwareCost(false, false, false, 0, 0, false) == 10);
	assert(ThreatAwareCost(false, true, true, 0, 0, false) >
		ThreatAwareCost(false, true, false, 0, 0, false));
	assert(ThreatAwareCost(false, false, false, 0, 6, true) >
		ThreatAwareCost(false, false, false, 0, 6, false));
	assert(ObjectivePriority(KeyObjective, 4, true, true, true) == 0);
	assert(ObjectivePriority(ExitObjective, 0, true, true, true) == 2);
	assert(ObjectivePriority(ExitObjective, 2, true, false, false) == 5);
	assert(ObjectivePriority(ExitObjective, 2, false, true, false) == 5);
	assert(ObjectivePriority(ExitObjective, 2, false, false, true) == 5);
	assert(ObjectivePriority(ExitObjective, 2, false, false, false) == 2);
	assert(ObjectivePriority(SwitchObjective, 2, false, true, true) <
		ObjectivePriority(ExitObjective, 2, false, true, true));
	assert(WeaponScore(4, 40, false, 3.0, 1) >
		WeaponScore(1, 40, false, 3.0, 1));
	assert(WeaponScore(4, 2, false, 3.0, 1) <
		WeaponScore(1, 40, false, 3.0, 1));
	assert(WeaponScore(4, 0, false, 3.0, 1) < 0);
	assert(WeaponScore(0, 0, true, 1.0, 1) > 0);
	assert(WeaponScore(0, 0, true, 3.0, 1) < 0);
	assert(ThreatDodgeDirection(0, true, true, 1, 4) == 1);
	assert(ThreatDodgeDirection(0, true, true, 4, 1) == -1);
	assert(ThreatDodgeDirection(0, false, true, 1, 4) == -1);
	assert(ThreatDodgeDirection(0, false, false, 1, 4) == 0);
}
