#ifndef OPENGOLD_SRD5_COMBAT_GEOMETRY_H
#define OPENGOLD_SRD5_COMBAT_GEOMETRY_H
#include "actor_model.h"
#include "damage.h"
#include "definition.h"
#include "dice.h"
#include "weapons.h"
#include "opengold/rules.h"
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// Where creatures stand and how far apart, and the options an attack command
// offers: Tactical Master's masteries, Brutal and Cunning Strikes, and what an
// attack settles.
namespace opengold::srd5::detail
{
using rules::Cell;
using rules::Command;
using rules::Footprint;

constexpr int distance(Cell a, Cell b)
{
    // std::abs is not constexpr before C++23.
    const int across = a.x > b.x ? a.x - b.x : b.x - a.x;
    const int down = a.y > b.y ? a.y - b.y : b.y - a.y;
    return std::max(across, down) * 5;
}

// The squares a creature fills now: its Beast form's while it has one (FOOT-1).
[[nodiscard]] Footprint footprint(const Actor &a);

[[nodiscard]] std::vector<Cell> squares_of(Cell anchor, Footprint footprint);

[[nodiscard]] std::vector<Cell> squares_of(const Actor &a);

[[nodiscard]] bool occupies(const Actor &a, Cell cell);

// Whether any of the creature's squares is one of `cells`.
[[nodiscard]] bool touches(const std::vector<Cell> &cells, const Actor &a);

// Feet between the nearest squares of two footprints: 5 when they touch.
constexpr int distance(Cell a, Footprint a_size, Cell b, Footprint b_size)
{
    const int across =
        std::max({0, a.x - (b.x + b_size.width - 1), b.x - (a.x + a_size.width - 1)});
    const int down =
        std::max({0, a.y - (b.y + b_size.height - 1), b.y - (a.y + a_size.height - 1)});
    return std::max(across, down) * 5;
}

[[nodiscard]] int distance(const Actor &a, const Actor &b);

[[nodiscard]] int distance(const Actor &a, Cell cell);

[[nodiscard]] int distance(Cell cell, const Actor &a);

// To a creature of `size` with its top-left square at `anchor`, such as a
// mover's next square.
[[nodiscard]] int distance(const Actor &a, Cell anchor, Footprint size);

// The creature's square nearest `toward`: where an area it casts or breathes
// starts and its range is measured from (FOOT-1).
[[nodiscard]] Cell origin_toward(const Actor &a, Cell toward);

// A summoned creature's squares, as its combat art fills them: a Giant
// Insect's wide art, a Draconic Spirit's 2 by 2 (FOOT-1).
[[nodiscard]] Footprint summoned_footprint(std::string_view definition);

[[nodiscard]] bool same_command(const Command &a, const Command &b);

// Tactical Master's three masteries, by the attack verbs' suffixes, with
// their commands' labels.
struct TacticalOption
{
    std::string_view suffix;
    detail::Mastery kind;
    std::string_view melee_label, ranged_label;
};

inline constexpr std::array<TacticalOption, 3> tactical_masteries{
    {{"push", detail::Mastery::push, "Melee attack (Push)", "Ranged attack (Push)"},
     {"sap", detail::Mastery::sap, "Melee attack (Sap)", "Ranged attack (Sap)"},
     {"slow", detail::Mastery::slow, "Melee attack (Slow)", "Ranged attack (Slow)"}}};

// Brutal Strike's blows (Barbarian 9; Staggering and Sundering from 13).
enum class BrutalBlow : unsigned
{
    forceful,
    hamstring,
    staggering,
    sundering
};

// Brutal Strike's blows by verb, with their commands' labels.
inline constexpr std::array<std::tuple<std::string_view, std::string_view, BrutalBlow>, 4>
    brutal_blows{{{"brutal_forceful", "Brutal Strike (Forceful Blow)", BrutalBlow::forceful},
                  {"brutal_hamstring", "Brutal Strike (Hamstring Blow)", BrutalBlow::hamstring},
                  {"brutal_staggering", "Brutal Strike (Staggering Blow)", BrutalBlow::staggering},
                  {"brutal_sundering", "Brutal Strike (Sundering Blow)", BrutalBlow::sundering}}};

// Cunning Strike's effects (Rogue 5).
enum class CunningStrike : unsigned
{
    poison, // with a Poisoner's Kit
    trip,
    withdraw,
    stealth, // Supreme Sneak's Stealth Attack (Thief 9)
    daze,    // Devious Strikes (Rogue 14)
    knock_out,
    obscure
};

// Cunning Strike's options by verb suffix, with their names and the Sneak
// Attack dice they cost.
// A command's verb is "melee_" or "ranged_" and one suffix, or two joined by
// "+" with Improved Cunning Strike (Rogue 11).
struct CunningOption
{
    std::string_view suffix;
    CunningStrike kind;
    std::string_view name;
    int cost;
};

inline constexpr std::array<CunningOption, 7> cunning_strikes{
    {{"poison", CunningStrike::poison, "Poison", 1},
     {"trip", CunningStrike::trip, "Trip", 1},
     {"withdraw", CunningStrike::withdraw, "Withdraw", 1},
     {"stealth", CunningStrike::stealth, "Stealth Attack", 1},
     {"daze", CunningStrike::daze, "Daze", 2},
     {"knock_out", CunningStrike::knock_out, "Knock Out", 6},
     {"obscure", CunningStrike::obscure, "Obscure", 3}}};

[[nodiscard]] const CunningOption &cunning_option(CunningStrike kind);

// Loading: a weapon fired in this action shoots again only with another
// action, such as an Action Surge's or Haste's.
[[nodiscard]] bool reloaded(const Actor &a);

// An attack the Attack action makes, which Extra Attack repeats; Reckless
// Attack and the pact weapon's damage types arrive here as "melee".
[[nodiscard]] bool attack_action(std::string_view verb);

[[nodiscard]] bool turns_to_attack(std::string_view verb);

// Named rather than a bool so a melee/ranged argument cannot be confused with
// the other flags beside it at a call site.
enum class AttackRange
{
    melee,
    ranged
};

// Whether an attack roll is a weapon's or a spell's; several modifiers apply to only one.
enum class AttackSource
{
    weapon,
    spell
};

// What a spell attack deals; a weapon attack takes its damage from the weapon.
struct SpellAttack
{
    Dice dice;
    detail::DamageType type{};
    int bursts{};             // how many extra d8s an 8 may add (Sorcerous Burst)
    std::string_view spell{}; // its attack bonus follows Magic Initiate's ability
};

// What attack() settled before apply_hit reports and applies it. Four ints and
// three flags in a row were easy to pass out of order, so callers name each one.
// " (2d6: 3 + 5, 1d8: 4, + 3 = 15)" for the dice behind a damage total: each run
// of one die size, then what was added and the total; with no total, the dice
// alone. Empty with no dice.
[[nodiscard]] std::string dice_detail(const std::vector<RolledDie> &dice,
                                      std::optional<int> total = {});

struct ResolvedAttack
{
    int natural{}; // the d20 as rolled
    int bonus{};   // everything added to the d20
    int mode{};    // below zero with Disadvantage, above zero with Advantage
    int damage{};  // after every reduction; zero on a miss
    detail::DamageType type{};
    AttackRange range{AttackRange::melee};
    bool spell{};
    bool savage{};    // Savage Attacker rerolled the damage
    int parry{};      // the AC a Parry added against this attack
    std::string dice; // the damage dice for the log, as dice_detail writes them
};

// What an attack is made with when it is not the attacker's own loadout: an
// Unarmed Strike, a thrown, off-hand or Torch strike, True Strike.
struct Loadout
{
    Definition definition;
    unsigned selected_weapon{};
    bool light_damage{};
};
} // namespace opengold::srd5::detail
#endif
