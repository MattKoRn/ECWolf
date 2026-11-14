#include "../src/wl_autobot_policy.h"
#include "../src/wl_rpg_progression.h"
// CI builds in Release; keep assertion-based regressions active under NDEBUG.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <vector>
#include <climits>

struct TestGrid
{
	std::vector<int> entry;
	int operator()(int, int to) const { return entry[to]; }
};

struct TestXPNumber
{
	unsigned long long value;
	explicit TestXPNumber(unsigned long long n = 0) : value(n) {}
	void Multiply(const TestXPNumber &rhs) { value *= rhs.value; }
	void MultiplySmall(unsigned int factor) { value *= factor; }
	void Add(const TestXPNumber &rhs) { value += rhs.value; }
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
	assert(RecoveryFor(280, 3) == Detour);
	assert(RecoveryFor(559, 4) == KeepTrying);
	assert(RecoveryFor(560, 4) == EmergencyNoclip);
	assert(!ShouldEmergencyAdvance(70 * 479, 10));
	assert(!ShouldEmergencyAdvance(70 * 480, 3));
	assert(ShouldEmergencyAdvance(70 * 480, 4));
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
	assert(ShouldShoot(1.05, true));
	assert(!ShouldShoot(1.06, true));
	assert(!ShouldShoot(3.0, true));
	// Wolf knife inherits Clip ammo, but primary attack consumes zero ammo.
	assert(IsMeleeWeapon(true, false, 0));
	assert(IsMeleeWeapon(false, false, 0));
	assert(!IsMeleeWeapon(true, false, 1));
	assert(!IsMeleeWeapon(true, true, 1));

	// The knife must approach a target and NEVER retreat at striking range.
	assert(MeleeApproachSpeed(5.0) == -68);
	assert(MeleeApproachSpeed(1.1) == -68);
	assert(MeleeApproachSpeed(1.0) == -35);
	assert(MeleeApproachSpeed(0.8) == -35);
	assert(MeleeApproachSpeed(0.65) == 0);
	assert(ShouldShoot(0.85, true));
	assert(!ShouldShoot(1.06, true));

	// Knife pulses start immediately upon aim/range lock, remain down for
	// three tics, then release until the next 15-tic cycle.
	for(unsigned int tic = 0; tic < 45; ++tic)
	{
		assert(MeleeAttackInput(tic, true, true) == (tic % 15 < 3));
		assert(!MeleeAttackInput(tic, false, true));
		assert(!MeleeAttackInput(tic, true, false));
	}
	// Dropped ammo takes priority over an exit only while collectible.
	assert(AmmoDropPriority(true, true, 6) == 1);
	assert(AmmoDropPriority(false, true, 6) == 6);
	assert(AmmoDropPriority(true, false, 6) == 6);
	assert(AmmoDropPriority(true, true, 2) == 1);

	// Manual camera bob is unchanged; auto-play reduces it by 90%.
	assert(AutoPlayVisualBob(400, false) == 400);
	assert(AutoPlayVisualBob(400, true) == 40);
	assert(AutoPlayVisualBob(-400, true) == -40);
	assert(AutoPlayVisualBob(0, true) == 0);

	// Quadratic thresholds strictly increase at every level up; the
	// generic algorithm is also used by the engine's BigNumber.
	assert(RPGProgression::RequiredXPForLevel(TestXPNumber(1)).value == 160);
	assert(RPGProgression::RequiredXPForLevel(TestXPNumber(2)).value == 260);
	assert(RPGProgression::RequiredXPForLevel(TestXPNumber(3)).value == 400);
	assert(RPGProgression::RequiredXPForLevel(TestXPNumber(5)).value == 800);
	assert(RPGProgression::RequiredXPForLevel(TestXPNumber(10)).value == 2500);
	unsigned long long previous = 0;
	for(unsigned int level = 1; level < 1000; ++level)
	{
		const unsigned long long required =
			RPGProgression::RequiredXPForLevel(TestXPNumber(level)).value;
		assert(required > previous);
		previous = required;
	}
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
	// Survival-first resources: replenish sooner under danger, but leave
	// nonurgent supplies behind when a campaign exit is available.
	assert(SupplyPriority(true, false, 30, 100, 50, 0, 0) == 1);
	assert(SupplyPriority(true, false, 75, 100, 50, 5, 0) == 1);
	assert(SupplyPriority(true, false, 75, 100, 50, 0, 2) == 1);
	assert(SupplyPriority(true, false, 75, 100, 50, 0, 0) == 6);
	assert(SupplyPriority(false, true, 100, 100, 7, 0, 0) == 1);
	assert(SupplyPriority(false, true, 100, 100, 12, 5, 0) == 1);
	assert(SupplyPriority(false, true, 100, 100, 12, 0, 0) == 6);
	assert(RangedMovementSpeed(1.0, true, 8) == 60);
	assert(RangedMovementSpeed(2.0, false, 0) == 25);
	assert(RangedMovementSpeed(3.5, true, 6) == 20);
	assert(RangedMovementSpeed(3.5, false, 6) == 0);
	assert(RangedMovementSpeed(6.0, false, 0) == -42);
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
