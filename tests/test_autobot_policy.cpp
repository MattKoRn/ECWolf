#include "../src/wl_autobot_policy.h"
#include <cassert>

int main()
{
	using namespace AutobotPolicy;
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
}
