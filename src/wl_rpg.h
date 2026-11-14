#ifndef ECWOLF_WL_RPG_H
#define ECWOLF_WL_RPG_H

class AActor;

// Account-wide, single-player RPG progression. Kept independent of map saves.
namespace RPG {
    enum Stat { Damage = 0, Vitality, Defense, Recovery, Fortune, Agility, StatCount };
    void Initialize();
    void Shutdown();
    void Tick();
    void ShowOfflinePopup();
    void ShowProfile();
    void AddStat(Stat stat);
    void AutoAssign();
    void SmartAutoAssign();
    bool AutoSpendEnabled();
    void ToggleAutoSpend();
    bool HudEnabled();
    void ToggleHud();
    const char *HudLine();
    void OnEnemySpawn(AActor *actor);
    void OnEnemyDeath(AActor *actor);
    unsigned OutgoingDamage(unsigned damage, const AActor *attacker);
    int IncomingDamage(int damage, const AActor *attacker);
    int MovementSpeed(int baseSpeed);
}
#endif
