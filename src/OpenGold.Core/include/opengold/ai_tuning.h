#ifndef OPENGOLD_AI_TUNING_H
#define OPENGOLD_AI_TUNING_H
#include "opengold/difficulty.h"
#include <array>
#include <cstddef>

// Every parameter and weight of the computer's combat policy (choose_demo_command
// in combat_demo.cpp, explained in docs/AI.md): change one here, rebuild, and
// measure it with opengold_ai_lab (docs/AI-LAB.md). The policy's hard rules, such
// as what is legal or what it may know, are not tunable, and neither are the
// spell priorities, which are the original's table in combat_demo.cpp.
namespace opengold::ai_tuning
{
// How a creature picks its weapon.
enum class WeaponChoice
{
    arbitrary,       // At random among those it could use now, or it walks up instead.
    original_rating, // The original's rule: melee beside an enemy, else the better rating.
    expected_damage  // The rules' expected damage against this target.
};

// Which enemy a creature goes after.
enum class Targeting
{
    keep_target,       // Its last target while in view; else one at random.
    weakest_once_lost, // Its last target while in view; else the weakest.
    weakest_now,       // The weakest, every turn.
    most_harm_stopped  // As weakest_now, but weapons weigh harm_per_hit_point.
};

// What one difficulty level does. Each row is independent: a level does not
// inherit from the one below it.
struct LevelTuning
{
    WeaponChoice weapon_choice{WeaponChoice::original_rating};
    Targeting targeting{Targeting::keep_target};
    // May strike a downed character, once no standing enemy can be struck this
    // turn: downing the whole party ends the fight, a kill does not.
    bool finishes_downed{};
    // The walk heads for an enemy it can strike this turn, or else the nearest,
    // not a target across the field.
    bool walks_within_reach{};
    // A shooter backs off to keep_away_squares before it shoots.
    bool keeps_distance{};
    // Between equally close squares, the one beside the fewest enemies.
    bool avoids_crowds{};
    // Takes a Bonus Action Disengage before shooting beside an enemy.
    bool disengages_to_shoot{};
    // Rolls which spell priorities it tries, as the original did; false always
    // tries the strongest first.
    bool rolls_for_spells{true};
};

// Indexed by Difficulty: easy, normal, hard, nightmare, infernal.
inline constexpr std::array<LevelTuning, 5> levels{{
    {.weapon_choice = WeaponChoice::arbitrary, .targeting = Targeting::keep_target},
    {.weapon_choice = WeaponChoice::original_rating, .targeting = Targeting::keep_target},
    {.weapon_choice = WeaponChoice::expected_damage,
     .targeting = Targeting::weakest_once_lost,
     .walks_within_reach = true},
    {.weapon_choice = WeaponChoice::expected_damage,
     .targeting = Targeting::weakest_now,
     .finishes_downed = true,
     .walks_within_reach = true,
     .keeps_distance = true,
     .avoids_crowds = true},
    {.weapon_choice = WeaponChoice::expected_damage,
     .targeting = Targeting::most_harm_stopped,
     .finishes_downed = true,
     .walks_within_reach = true,
     .keeps_distance = true,
     .avoids_crowds = true,
     .disengages_to_shoot = true,
     .rolls_for_spells = false},
}};

[[nodiscard]] constexpr const LevelTuning &level(Difficulty difficulty) noexcept
{
    return levels[static_cast<std::size_t>(difficulty)];
}

// Party members on Quick play at this level whatever the setting: the setting
// makes the party's enemies harder, not its own members worse.
inline constexpr Difficulty quick_difficulty = Difficulty::infernal;

// How far a creature hears enemies through walls: 100 feet.
inline constexpr int hearing_squares = 20;

// How far a shooter that keeps its distance backs off. Every ranged weapon
// reaches this far without penalty, so it goes no further.
inline constexpr int keep_away_squares = 4;

// With no enemy beside it, a creature shoots if the shot is worth more than this
// share of its melee weapon, and otherwise closes in (the original's one half).
inline constexpr double shot_worth_share = 0.5;

// most_harm_stopped: the harm a target does is the better of its weapon ratings,
// at least minimum_harm, times this while it holds a spell a hit could break.
inline constexpr double minimum_harm = 1.0;
inline constexpr double concentration_weight = 2.0;

// A creature counts as hurt at this share of its Hit Points or below: for healing,
// Second Wind, Wholeness of Body and putting out a fire.
inline constexpr double hurt_share = 0.5;

// The Shield spell's bonus: it is cast only if this turns the hit into a miss.
inline constexpr int shield_margin = 5;

// The original's roll for spell priorities: 1d7, trying 7 down to 8 less the roll.
inline constexpr int spell_roll_sides = 7;
} // namespace opengold::ai_tuning
#endif
