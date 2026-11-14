#ifndef ECWOLF_WL_RPG_LOOT_POLICY_H
#define ECWOLF_WL_RPG_LOOT_POLICY_H
#include <algorithm>

// Pure helpers: RPG equipment is account-wide, auto-collected from monster
// loot rolls, and never replaces the engine's weapon or inventory objects.
namespace RPGLootPolicy {
    enum Rarity { None, Common, Rare, Epic, Legendary };
    enum Slot { Weapon, Armor, SlotCount };
    enum Affix { Brutal, Keen, Swift };
    inline unsigned RollRarity(unsigned roll, unsigned fortune) {
        roll %= 10000;
        const unsigned luck = std::min(fortune, 100U);
        if(roll < 8 + luck / 20) return Legendary;
        if(roll < 40 + luck / 5) return Epic;
        if(roll < 200 + luck) return Rare;
        if(roll < 950 + luck * 2) return Common;
        return None;
    }
    inline unsigned Power(unsigned rarity, unsigned level) {
        if(!rarity) return 0;
        return std::min(30U, rarity * 3 + std::min(level / 12, 18U));
    }
    inline bool Better(unsigned currentRarity, unsigned currentPower,
                       unsigned rarity, unsigned power) {
        return rarity * 100 + power > currentRarity * 100 + currentPower;
    }
    inline void CombatWeights(unsigned healthPercent, bool bot, unsigned weights[6]) {
        // Damage, Vitality, Defense, Recovery, Fortune, Agility.
        weights[0] = 4; weights[1] = 3; weights[2] = 3;
        weights[3] = 2; weights[4] = 2; weights[5] = 2;
        if(healthPercent < 65) {
            weights[1] += 3; weights[2] += 3; weights[3] += 2;
        }
        if(bot) { weights[2] += 1; weights[3] += 1; weights[5] += 2; }
    }
}
#endif
