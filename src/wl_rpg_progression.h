#ifndef ECWOLF_WL_RPG_PROGRESSION_H
#define ECWOLF_WL_RPG_PROGRESSION_H

// Shared by the RPG engine and standalone regression tests. A Number supports
// Add, Multiply and MultiplySmall (the RPG's arbitrary-precision BigNumber
// implements all three). The requirement grows at *every* level:
//
// XP required for level L -> L+1 = 100 + 40*L + 20*L*L.
//
// This is deliberately not computed with fixed-width integer multiplication:
// high-level profiles must retain arbitrary-precision XP thresholds.
namespace RPGProgression
{
    template<class Number>
    Number RequiredXPForLevel(const Number &level)
    {
        Number squared = level;
        squared.Multiply(level);
        squared.MultiplySmall(20);
        Number linear = level;
        linear.MultiplySmall(40);
        squared.Add(linear);
        squared.Add(Number(100));
        return squared;
    }
}

#endif
