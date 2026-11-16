/*
 * ECWolf RPG: persistent account-wide progression inspired by classic arena RPG mods.
 * All progress values use decimal arbitrary-precision integers; gameplay-facing
 * integer conversions are bounded only where ECWolf's fixed-size engine requires it.
 */
#include "wl_rpg.h"
#include "wl_rpg_progression.h"
#include "wl_rpg_loot_policy.h"
#include "wl_autobot.h"

#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
// ECWolf and the Windows SDK both define DWORD unless explicitly coordinated.
#define USE_WINDOWS_DWORD
#define USE_WINDOWS_BOOLEAN
#define WIN32_LEAN_AND_MEAN
// Prevent the Windows min/max macros from breaking std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "wl_def.h"
#include "wl_agent.h"
#include "wl_game.h"
#include "wl_play.h"
#include "wl_menu.h"
#include "wl_net.h"
#include "actor.h"
#include "g_shared/a_playerpawn.h"
#include "id_in.h"
#include "m_random.h"

namespace {
class BigNumber {
public:
    BigNumber(unsigned long long value = 0) {
        std::ostringstream out;
        out << value;
        digits = out.str();
    }
    explicit BigNumber(const std::string &text) : digits(text) {
        if(digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
            digits = "0";
        Normalize();
    }
    const std::string &Text() const { return digits; }
    bool IsZero() const { return digits == "0"; }
    bool Less(const BigNumber &other) const {
        return digits.size() != other.digits.size()
            ? digits.size() < other.digits.size() : digits < other.digits;
    }
    void Add(const BigNumber &other) {
        std::string result;
        int carry = 0;
        for(size_t i = 0; i < digits.size() || i < other.digits.size() || carry; ++i) {
            int v = carry;
            if(i < digits.size()) v += digits[digits.size() - 1 - i] - '0';
            if(i < other.digits.size()) v += other.digits[other.digits.size() - 1 - i] - '0';
            result.push_back(static_cast<char>('0' + v % 10));
            carry = v / 10;
        }
        std::reverse(result.begin(), result.end());
        digits.swap(result);
    }
    bool Subtract(const BigNumber &other) {
        if(Less(other)) return false;
        int borrow = 0;
        for(size_t i = 0; i < digits.size(); ++i) {
            int value = digits[digits.size() - 1 - i] - '0' - borrow;
            if(i < other.digits.size())
                value -= other.digits[other.digits.size() - 1 - i] - '0';
            borrow = value < 0 ? 1 : 0;
            if(borrow) value += 10;
            digits[digits.size() - 1 - i] = static_cast<char>('0' + value);
        }
        Normalize();
        return true;
    }
    void MultiplySmall(unsigned int factor) {
        if(factor == 0 || IsZero()) { digits = "0"; return; }
        unsigned long long carry = 0;
        for(size_t i = digits.size(); i-- > 0;) {
            carry += static_cast<unsigned long long>(digits[i] - '0') * factor;
            digits[i] = static_cast<char>('0' + carry % 10);
            carry /= 10;
        }
        while(carry) {
            digits.insert(digits.begin(), static_cast<char>('0' + carry % 10));
            carry /= 10;
        }
    }
    void Multiply(const BigNumber &other) {
        if(IsZero() || other.IsZero()) { digits = "0"; return; }
        const std::string lhs = digits;
        std::vector<int> answer(lhs.size() + other.digits.size(), 0);
        for(size_t i = lhs.size(); i-- > 0;) {
            for(size_t j = other.digits.size(); j-- > 0;) {
                const size_t position = i + j + 1;
                int amount = (lhs[i] - '0') * (other.digits[j] - '0') + answer[position];
                answer[position] = amount % 10;
                answer[position - 1] += amount / 10;
            }
        }
        digits.clear();
        for(size_t i = 0; i < answer.size(); ++i)
            digits.push_back(static_cast<char>('0' + answer[i]));
        Normalize();
    }
    unsigned int DivideSmall(unsigned int divisor) {
        if(!divisor) return 0;
        unsigned long long remainder = 0;
        for(size_t i = 0; i < digits.size(); ++i) {
            remainder = remainder * 10 + (digits[i] - '0');
            digits[i] = static_cast<char>('0' + remainder / divisor);
            remainder %= divisor;
        }
        Normalize();
        return static_cast<unsigned int>(remainder);
    }
    unsigned long long Capped(unsigned long long limit) const {
        unsigned long long n = 0;
        for(size_t i = 0; i < digits.size(); ++i) {
            unsigned int digit = digits[i] - '0';
            if(n > (limit - std::min<unsigned long long>(digit, limit)) / 10)
                return limit;
            n = n * 10 + digit;
            if(n >= limit) return limit;
        }
        return n;
    }
private:
    void Normalize() {
        const size_t first = digits.find_first_not_of('0');
        if(first == std::string::npos) digits = "0";
        else if(first) digits.erase(0, first);
    }
    std::string digits;
};

std::string Suffix(size_t group) {
    static const char *first[] = {"", "K", "M", "B", "T"};
    if(group < 5) return first[group];
    // Excel-like base-26 after T: aa, ab ... zz, aaa ...
    size_t index = group - 5 + 26;
    std::string result;
    do {
        result.insert(result.begin(), static_cast<char>('a' + index % 26));
        if(index < 26) break;
        index = index / 26 - 1;
    } while(true);
    return result;
}

std::string Compact(const BigNumber &number) {
    const std::string &text = number.Text();
    if(text.size() <= 3) return text;
    size_t group = (text.size() - 1) / 3;
    size_t lead = text.size() - group * 3;
    std::string out = text.substr(0, lead);
    std::string fraction = text.substr(lead, 2);
    while(!fraction.empty() && fraction[fraction.size() - 1] == '0')
        fraction.erase(fraction.size() - 1);
    if(!fraction.empty()) out += "." + fraction;
    return out + Suffix(group);
}


struct Equipment {
    unsigned rarity, affix, power;
    Equipment() : rarity(0), affix(0), power(0) {}
};
struct Profile {
    BigNumber level, xp, points;
    Equipment equipment[RPGLootPolicy::SlotCount];
    bool autoSpend, showHud;
    BigNumber stat[RPG::StatCount];
    unsigned long long savedAt;
    Profile() : level(1), xp(0), points(0), autoSpend(true), showHud(true), savedAt(0) {}
};
Profile profile;
bool ready = false;
bool offlinePopup = false;
unsigned long long offlineSeconds = 0;
BigNumber offlineXp;
std::string saveDirectory, saveFile;
unsigned long long lastDiskSave = 0;
int lastGameTick = -1;
FRandom pr_rpg("PersistentRPG");

unsigned long long Now() {
    const std::time_t t = std::time(NULL);
    return t > 0 ? static_cast<unsigned long long>(t) : 0;
}
bool CreateDirectory(const std::string &path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0700) == 0 || errno == EEXIST;
#endif
}
std::string Documents() {
#ifdef _WIN32
    char path[MAX_PATH] = {0};
    HMODULE shell = LoadLibraryA("shell32.dll");
    if(shell) {
        typedef HRESULT (WINAPI *FolderFn)(HWND, int, HANDLE, DWORD, LPSTR);
        FolderFn folder = reinterpret_cast<FolderFn>(GetProcAddress(shell, "SHGetFolderPathA"));
        if(folder) folder(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, path);
        FreeLibrary(shell);
    }
    if(path[0]) return path;
    const char *home = std::getenv("USERPROFILE");
    return std::string(home ? home : ".") + "\\Documents";
#else
    const char *home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/Documents";
#endif
}

BigNumber RequiredXP() {
    return RPGProgression::RequiredXPForLevel(profile.level);
}
void FinishLevels(unsigned int workBudget) {
    unsigned int gainedLevels = 0;
    for(unsigned int i = 0; i < workBudget; ++i) {
        BigNumber requirement = RequiredXP();
        if(profile.xp.Less(requirement)) break;
        profile.xp.Subtract(requirement);
        profile.level.Add(BigNumber(1));
        profile.points.Add(BigNumber(5));
        ++gainedLevels;
    }
    if(ready && gainedLevels > 0) {
        Printf("RPG: reached level %s (+%u stat points).\n",
            Compact(profile.level).c_str(), gainedLevels * 5);
    }
}
void GrantXP(BigNumber gained) {
    if(gained.IsZero()) return;
    // Fortune increases both real-time and offline XP without a stat cap.
    BigNumber multiplier = profile.stat[RPG::Fortune];
    multiplier.MultiplySmall(3);
    multiplier.Add(BigNumber(100));
    gained.Multiply(multiplier);
    gained.DivideSmall(100);
    profile.xp.Add(gained);
    FinishLevels(2500); // Deferred level resolution is a speed guard, not a cap.
}
bool ReadProfile() {
    std::ifstream input(saveFile.c_str());
    if(!input) return false;
    Profile loaded;
    std::string line;
    if(!std::getline(input, line) || line != "ECWolfRPG=1") return false;
    while(std::getline(input, line)) {
        size_t split = line.find('=');
        if(split == std::string::npos) continue;
        std::string key = line.substr(0, split);
        std::string value = line.substr(split + 1);
        if(key == "level") loaded.level = BigNumber(value);
        else if(key == "xp") loaded.xp = BigNumber(value);
        else if(key == "points") loaded.points = BigNumber(value);
        else if(key == "savedAt") loaded.savedAt = std::strtoull(value.c_str(), NULL, 10);
        else if(key == "autoSpend") loaded.autoSpend = value != "0";
        else if(key == "showHud") loaded.showHud = value != "0";
        else if(key == "weaponRarity") loaded.equipment[0].rarity = std::min(4, std::max(0, std::atoi(value.c_str())));
        else if(key == "weaponAffix") loaded.equipment[0].affix = std::min(2, std::max(0, std::atoi(value.c_str())));
        else if(key == "weaponPower") loaded.equipment[0].power = std::min(30, std::max(0, std::atoi(value.c_str())));
        else if(key == "armorRarity") loaded.equipment[1].rarity = std::min(4, std::max(0, std::atoi(value.c_str())));
        else if(key == "armorAffix") loaded.equipment[1].affix = std::min(2, std::max(0, std::atoi(value.c_str())));
        else if(key == "armorPower") loaded.equipment[1].power = std::min(30, std::max(0, std::atoi(value.c_str())));
        else for(int s = 0; s < RPG::StatCount; ++s) {
            std::ostringstream field; field << "stat" << s;
            if(key == field.str()) loaded.stat[s] = BigNumber(value);
        }
    }
    if(loaded.level.IsZero()) loaded.level = BigNumber(1);
    profile = loaded;
    return true;
}
void SaveProfile() {
    if(!ready) return;
    const std::string tempFile = saveFile + ".tmp";
    std::ofstream output(tempFile.c_str(), std::ios::out | std::ios::trunc);
    if(!output) return; // Silent autosave must never interrupt play.
    output << "ECWolfRPG=1\n";
    output << "level=" << profile.level.Text() << "\n";
    output << "xp=" << profile.xp.Text() << "\n";
    output << "points=" << profile.points.Text() << "\n";
    output << "autoSpend=" << (profile.autoSpend ? 1 : 0) << "\n";
    output << "showHud=" << (profile.showHud ? 1 : 0) << "\n";
    output << "weaponRarity=" << profile.equipment[0].rarity << "\n";
    output << "weaponAffix=" << profile.equipment[0].affix << "\n";
    output << "weaponPower=" << profile.equipment[0].power << "\n";
    output << "armorRarity=" << profile.equipment[1].rarity << "\n";
    output << "armorAffix=" << profile.equipment[1].affix << "\n";
    output << "armorPower=" << profile.equipment[1].power << "\n";
    for(int i = 0; i < RPG::StatCount; ++i)
        output << "stat" << i << "=" << profile.stat[i].Text() << "\n";
    output << "savedAt=" << Now() << "\n";
    output.flush();
    if(!output.good()) return;
    output.close();
#ifdef _WIN32
    MoveFileExA(tempFile.c_str(), saveFile.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    std::rename(tempFile.c_str(), saveFile.c_str());
#endif
    lastDiskSave = Now();
}
double Power() {
    const double level = static_cast<double>(profile.level.Capped(100000000));
    unsigned long long total = 0;
    for(int i = 0; i < RPG::StatCount; ++i)
        total += profile.stat[i].Capped(10000000);
    return std::min(3.5, 1.0 + 0.06 * std::sqrt(std::max(0.0, level - 1.0)) +
        0.025 * std::sqrt(static_cast<double>(total)));
}
unsigned int RandomPercent(unsigned int min, unsigned int max) {
    // FRandom::operator() without a modulus returns only 0..255.
    // Use the full 32-bit generator so 10,000-way rarity rolls are possible.
    return min + static_cast<unsigned int>(pr_rpg(static_cast<int>(max - min + 1)));
}
unsigned int NumericStat(RPG::Stat s, unsigned int cap = 1000000) {
    return static_cast<unsigned int>(profile.stat[s].Capped(cap));
}
unsigned int BoundDamage(double value) {
    if(!(value > 0)) return 0;
    return static_cast<unsigned int>(std::min(value, 100000000.0));
}
}

namespace RPG {
// Tick() uses regeneration before its helper is defined below.
static int RegenPerSecond(int maxHealth);

void Initialize() {
    if(ready) return;
    // The unattended dual-campaign harness sets a distinct profile folder
    // per engine process. Normal gameplay keeps the existing Documents path.
    const char *isolatedProfile = std::getenv("ECWOLF_RPG_PROFILE_DIR");
    if(isolatedProfile && *isolatedProfile) {
        saveDirectory = isolatedProfile;
    } else {
        saveDirectory = Documents() +
#ifdef _WIN32
            "\\ECWolfRPG";
#else
            "/ECWolfRPG";
#endif
        CreateDirectory(Documents());
    }
    CreateDirectory(saveDirectory);
    saveFile = saveDirectory +
#ifdef _WIN32
        "\\profile.rpg";
#else
        "/profile.rpg";
#endif
    ReadProfile();
    ready = true;
    unsigned long long now = Now();
    if(profile.savedAt > 0 && now > profile.savedAt) {
        offlineSeconds = now - profile.savedAt; // No artificial duration limit.
        BigNumber minutes(offlineSeconds);
        minutes.MultiplySmall(15);
        minutes.DivideSmall(60);
        if(!minutes.IsZero()) {
            offlineXp = minutes;
            GrantXP(minutes);
            offlinePopup = true;
        }
    }
    SaveProfile(); // Account for the elapsed period once, even after a crash.
}

void Shutdown() { SaveProfile(); }

void Tick() {
    if(!ready || Net::InitVars.mode != Net::MODE_SinglePlayer) return;
    if(lastGameTick == gamestate.TimeCount) return;
    lastGameTick = gamestate.TimeCount;
    if(gamestate.TimeCount % TICRATE) return;
    FinishLevels(2500);
    if(profile.autoSpend && !profile.points.IsZero()) SmartAutoAssign();
    APlayerPawn *pawn = players[ConsolePlayer].mo;
    if(pawn && players[ConsolePlayer].state == player_t::PST_LIVE) {
        const APlayerPawn *base = static_cast<const APlayerPawn *>(pawn->GetDefault());
        unsigned int vitality = NumericStat(Vitality);
        int baseMax = std::max(1, base->maxhealth);
        int newMax = static_cast<int>(std::min(100000000.0,
            static_cast<double>(baseMax) + static_cast<double>(vitality) * 12.0));
        pawn->maxhealth = newMax;
        if(players[ConsolePlayer].health > newMax)
            players[ConsolePlayer].health = newMax;
        if(players[ConsolePlayer].health > 0) {
            const int amount = RegenPerSecond(newMax);
            if(amount > 0) {
                players[ConsolePlayer].health = std::min(newMax,
                    players[ConsolePlayer].health + amount);
            }
            pawn->health = players[ConsolePlayer].health;
        }
    }
    if(Now() >= lastDiskSave + 10) SaveProfile();
}

void ShowOfflinePopup() {
    if(!offlinePopup) return;
    offlinePopup = false;
    std::ostringstream msg;
    msg << "ECWolf RPG: OFFLINE PROGRESS\n"
        << "Away: " << (offlineSeconds / 3600) << "h "
        << ((offlineSeconds % 3600) / 60) << "m\n"
        << "Base XP earned: " << Compact(offlineXp) << "\n"
        << "Fortune bonus applied!\n"
        << "Level: " << Compact(profile.level) << "\n"
        << "Unspent points: " << Compact(profile.points) << "\n"
        << "Press a key to continue";
    Message(msg.str().c_str());
    IN_Ack(ACK_Local);
}

static double ArmorMultiplier() {
    const Equipment &relic = profile.equipment[RPGLootPolicy::Armor];
    return 1.0 + NumericStat(Defense) * 0.06 +
        (relic.rarity && relic.affix == RPGLootPolicy::Brutal ? relic.power * 0.01 : 0.0);
}
static int RegenPerSecond(int maxHealth) {
    const unsigned int regen = NumericStat(Recovery);
    const Equipment &armor = profile.equipment[RPGLootPolicy::Armor];
    if(!regen && !(armor.rarity && armor.affix == RPGLootPolicy::Keen))
        return 0;
    const double cap = maxHealth > 0 ? static_cast<double>(maxHealth) / 8.0 : 1000000.0;
    return std::max(1, static_cast<int>(std::min(cap,
        1.0 + regen * 0.75 +
        (armor.rarity && armor.affix == RPGLootPolicy::Keen ? armor.power * 0.2 : 0.0))));
}
static double DamageMultiplier() {
    const Equipment &weapon = profile.equipment[RPGLootPolicy::Weapon];
    return 1.0 + NumericStat(Damage) * 0.075 +
        (weapon.rarity && weapon.affix == RPGLootPolicy::Brutal ? weapon.power * 0.01 : 0.0);
}
static double CritChance() {
    const Equipment &weapon = profile.equipment[RPGLootPolicy::Weapon];
    return std::min(80.0, 5.0 + NumericStat(Fortune) * 0.25 +
        (weapon.rarity && weapon.affix == RPGLootPolicy::Keen ? weapon.power * 0.5 : 0.0));
}
static double SpeedMultiplier() {
    const Equipment &weapon = profile.equipment[RPGLootPolicy::Weapon];
    const Equipment &armor = profile.equipment[RPGLootPolicy::Armor];
    const double boost = (weapon.rarity && weapon.affix == RPGLootPolicy::Swift ? weapon.power * 0.002 : 0.0) +
        (armor.rarity && armor.affix == RPGLootPolicy::Swift ? armor.power * 0.002 : 0.0);
    return std::min(1.50, 1.0 + NumericStat(Agility) * 0.02 + boost);
}

void ShowProfile() {
    std::ostringstream msg;
    msg.precision(2);
    msg << std::fixed;
    msg << "ECWolf RPG - PERMANENT PROFILE\n"
        << "Level " << Compact(profile.level)
        << " XP " << Compact(profile.xp) << "/" << Compact(RequiredXP()) << "\n"
        << "Points to spend: " << Compact(profile.points) << "\n"
        << "Damage " << Compact(profile.stat[Damage])
        << "  Vitality " << Compact(profile.stat[Vitality]) << "\n"
        << "Defense " << Compact(profile.stat[Defense])
        << "  Recovery " << Compact(profile.stat[Recovery]) << "\n"
        << "Fortune " << Compact(profile.stat[Fortune])
        << "  Agility " << Compact(profile.stat[Agility]) << "\n"
        << "Dmg x" << DamageMultiplier()
        << "  Crit " << static_cast<int>(CritChance()) << "%"
        << "  Spd x" << SpeedMultiplier() << "\n"
        << "DR " << static_cast<int>((1.0 - 1.0 / ArmorMultiplier()) * 100.0) << "%"
        << "  Regen " << RegenPerSecond(players[ConsolePlayer].mo ? players[ConsolePlayer].mo->maxhealth : 100) << "/s\n"
        << "Smart auto-spend: " << (profile.autoSpend ? "ON" : "OFF") << "\n"
        << "RPG HUD: " << (profile.showHud ? "ON" : "OFF") << "\n"
        << "Weapon: " << RPGLootPolicy::FormatRelic(
            RPGLootPolicy::Weapon, profile.equipment[0].rarity,
            profile.equipment[0].affix, profile.equipment[0].power) << "\n"
        << "Armor: " << RPGLootPolicy::FormatRelic(
            RPGLootPolicy::Armor, profile.equipment[1].rarity,
            profile.equipment[1].affix, profile.equipment[1].power) << "\n"
        << "Saved to Documents/ECWolfRPG";
    Message(msg.str().c_str());
    IN_Ack(ACK_Local);
}
void AddStat(Stat stat) {
    if(!ready || stat < 0 || stat >= StatCount || profile.points.IsZero()) return;
    profile.points.Subtract(BigNumber(1));
    profile.stat[stat].Add(BigNumber(1));
    SaveProfile();
}
void AutoAssign() {
    if(!ready || profile.points.IsZero()) return;
    BigNumber each = profile.points;
    unsigned int rem = each.DivideSmall(StatCount);
    for(int i = 0; i < StatCount; ++i) {
        profile.stat[i].Add(each);
        if(static_cast<unsigned int>(i) < rem)
            profile.stat[i].Add(BigNumber(1));
    }
    profile.points = BigNumber(0);
    SaveProfile();
}

void SmartAutoAssign() {
    if(!ready || profile.points.IsZero() || Net::InitVars.mode != Net::MODE_SinglePlayer) return;
    unsigned health = 100;
    APlayerPawn *pawn = players[ConsolePlayer].mo;
    if(pawn && pawn->maxhealth > 0)
        health = static_cast<unsigned>(std::max(0.0, std::min(100.0,
            100.0 * pawn->health / pawn->maxhealth)));
    unsigned weights[StatCount];
    RPGLootPolicy::CombatWeights(health, BotEnabled, weights);
    unsigned total = 0;
    for(int i = 0; i < StatCount; ++i) total += weights[i];
    BigNumber share = profile.points;
    unsigned remainder = share.DivideSmall(total);
    unsigned extras[StatCount];
    RPGLootPolicy::WeightedRemainder(remainder, weights, extras);
    for(int i = 0; i < StatCount; ++i) {
        BigNumber portion = share;
        portion.MultiplySmall(weights[i]);
        profile.stat[i].Add(portion);
        profile.stat[i].Add(BigNumber(extras[i]));
    }
    profile.points = BigNumber(0);
    SaveProfile();
}
bool AutoSpendEnabled() { return profile.autoSpend; }
void ToggleAutoSpend() {
    profile.autoSpend = !profile.autoSpend;
    if(profile.autoSpend) SmartAutoAssign();
    SaveProfile();
}
bool HudEnabled() { return ready && profile.showHud; }
void ToggleHud() { profile.showHud = !profile.showHud; SaveProfile(); }
const char *HudLine() {
    static std::string line;
    if(!ready) return "";
    std::ostringstream msg;
    msg << "RPG Lv" << Compact(profile.level)
        << " XP " << Compact(profile.xp) << "/" << Compact(RequiredXP())
        << " P" << Compact(profile.points);
    if(profile.equipment[0].rarity || profile.equipment[1].rarity) {
        msg << " |";
        if(profile.equipment[0].rarity)
            msg << " W:" << RPGLootPolicy::ShortRarityName(profile.equipment[0].rarity)
                << "+" << profile.equipment[0].power;
        if(profile.equipment[1].rarity)
            msg << " A:" << RPGLootPolicy::ShortRarityName(profile.equipment[1].rarity)
                << "+" << profile.equipment[1].power;
    }
    line = msg.str();
    return line.c_str();
}
void OnEnemySpawn(AActor *actor) {
    if(!ready || !actor || actor->player || !(actor->flags & FL_COUNTKILL)
        || actor->health <= 0 || Net::InitVars.mode != Net::MODE_SinglePlayer) return;
    const double factor = Power() * RandomPercent(92, 110) / 100.0;
    actor->health = static_cast<int>(std::min(100000000.0, actor->health * factor));
    // Small, strictly capped speed variance; navigation cannot outrun collision.
    actor->speed = static_cast<int>(std::min(10000000.0,
        actor->speed * std::min(1.35, 1.0 + (factor - 1.0) * 0.04)));
}
void OnEnemyDeath(AActor *actor) {
    if(!ready || !actor || actor->player || !(actor->flags & FL_COUNTKILL)
        || Net::InitVars.mode != Net::MODE_SinglePlayer) return;
    const unsigned int base = static_cast<unsigned int>(
        std::max(5, actor->SpawnHealth() / 7 + std::max(0, actor->points) / 50));
    BigNumber gained = profile.level;
    gained.DivideSmall(4);
    gained.Add(BigNumber(1));
    gained.MultiplySmall(base);
    gained.MultiplySmall(RandomPercent(90, 125));
    gained.DivideSmall(100);
    GrantXP(gained);
    // Physical engine weapons remain untouched: rolls award persistent
    // automatically equipped RPG weapon/armor relics with gameplay affixes.
    const unsigned rarity = RPGLootPolicy::RollRarity(
        RandomPercent(0, 9999), NumericStat(Fortune, 100));
    if(!rarity) return;
    const unsigned slot = RandomPercent(0, 1);
    const unsigned affix = RandomPercent(0, 2);
    const unsigned level = static_cast<unsigned>(profile.level.Capped(1000));
    const unsigned power = RPGLootPolicy::Power(rarity, level);
    Equipment &current = profile.equipment[slot];
    if(!RPGLootPolicy::Better(current.rarity, current.power, rarity, power)) return;
    current.rarity = rarity;
    current.power = power;
    current.affix = affix;
    Printf("RPG: equipped %s %s %s relic (+%u).\n",
        RPGLootPolicy::RarityName(rarity),
        RPGLootPolicy::AffixName(slot, affix),
        slot == 0 ? "weapon" : "armor", power);
    SaveProfile();
}
unsigned OutgoingDamage(unsigned damage, const AActor *attacker) {
    if(!ready || !attacker || !attacker->player || Net::InitVars.mode != Net::MODE_SinglePlayer)
        return damage;
    const double attack = DamageMultiplier();
    const double crit = CritChance();
    const double multiplier = RandomPercent(0, 99) < crit ? 1.75 : 1.0;
    return BoundDamage(static_cast<double>(damage) * attack * multiplier);
}
int IncomingDamage(int damage, const AActor *attacker) {
    if(!ready || damage <= 0 || Net::InitVars.mode != Net::MODE_SinglePlayer) return damage;
    double amount = damage;
    if(attacker && !attacker->player)
        amount *= Power() * RandomPercent(85, 115) / 100.0;
    amount /= ArmorMultiplier();
    if(amount < 1.0) amount = 1.0; // No invulnerability from unlimited stats.
    return static_cast<int>(BoundDamage(amount));
}
int MovementSpeed(int baseSpeed) {
    if(!ready || Net::InitVars.mode != Net::MODE_SinglePlayer) return baseSpeed;
    const double factor = SpeedMultiplier();
    return static_cast<int>(std::min(10000000.0, baseSpeed * factor));
}
}
