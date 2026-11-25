#ifndef OPENGOLD_DIFFICULTY_H
#define OPENGOLD_DIFFICULTY_H
#include "opengold/gold_box_game.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opengold
{
// The game's difficulty (DIFF-1), easiest first. The original had none.
enum class Difficulty
{
    easy,
    normal,
    hard,
    nightmare,
    infernal
};

inline constexpr Difficulty default_difficulty = Difficulty::normal;

// The names settings.cfg spells the levels with, in order.
inline constexpr std::array<std::string_view, 5> difficulty_names{"easy", "normal", "hard",
                                                                  "nightmare", "infernal"};

// The Buck Rogers games' Novice, a level below Easy that only they offer
// (docs/BUCK-ROGERS.md, Difficulty). Every other game plays it as Easy.
inline constexpr std::string_view novice_name = "novice";

[[nodiscard]] constexpr std::optional<Difficulty> parse_difficulty(std::string_view name) noexcept
{
    if (name == novice_name)
    {
        return Difficulty::easy;
    }
    for (std::size_t n = 0; n < difficulty_names.size(); ++n)
    {
        if (difficulty_names[n] == name)
        {
            return static_cast<Difficulty>(n);
        }
    }
    return std::nullopt;
}

// The difficulty setting's values, easiest first: Novice, then Difficulty's
// five. The Buck Rogers games play the six as their Novice to Admiral; the
// other games play Novice as Easy.
enum class DifficultySetting
{
    novice,
    easy,
    normal,
    hard,
    nightmare,
    infernal
};

[[nodiscard]] constexpr std::optional<DifficultySetting>
parse_difficulty_setting(std::string_view name) noexcept
{
    if (name == novice_name)
    {
        return DifficultySetting::novice;
    }
    const auto level = parse_difficulty(name);
    return level ? std::optional{static_cast<DifficultySetting>(static_cast<int>(*level) + 1)}
                 : std::nullopt;
}

[[nodiscard]] constexpr std::string_view difficulty_setting_name(DifficultySetting setting) noexcept
{
    return setting == DifficultySetting::novice
               ? novice_name
               : difficulty_names[static_cast<std::size_t>(setting) - 1];
}

// The level a game plays a setting at: Novice as Easy.
[[nodiscard]] constexpr Difficulty played_difficulty(DifficultySetting setting) noexcept
{
    return setting == DifficultySetting::novice
               ? Difficulty::easy
               : static_cast<Difficulty>(static_cast<int>(setting) - 1);
}

[[nodiscard]] constexpr DifficultySetting difficulty_setting(Difficulty difficulty) noexcept
{
    return static_cast<DifficultySetting>(static_cast<int>(difficulty) + 1);
}

// Whether a game's Options screen offers Novice: only the Buck Rogers games'.
[[nodiscard]] constexpr bool offers_novice(GoldBoxGame game) noexcept
{
    return is_buck_rogers(game);
}

// Monsters drink their potions, read their scrolls and use their wands on Hard
// and above (DIFF-1).
[[nodiscard]] constexpr bool monsters_use_items(Difficulty difficulty) noexcept
{
    return difficulty >= Difficulty::hard;
}

// What each difficulty does to the monsters a fight meets (DIFF-2). Normal
// plays the encounter as it always has.
struct MonsterStrength
{
    // The encounter challenge setting is worth this percent of itself.
    unsigned challenge_percent{100};
    // Creatures per ten living party members.
    unsigned creatures_per_ten_members{10};
    // Fewer creatures than that, keeping at least one.
    unsigned creatures_fewer{};
    // At most this percent of the creatures Normal fields. A small band that grows
    // threefold, each creature also stronger, leaves the party no chance.
    unsigned most_creatures_percent{100};
    unsigned hit_points_percent{100};
    int attack_bonus{};
    // Added to the morale of those that can break: lower breaks sooner.
    int morale_shift{};
};

// Indexed by Difficulty: easy, normal, hard, nightmare, infernal.
inline constexpr std::array<MonsterStrength, 5> monster_strengths{{
    {.challenge_percent = 60,
     .creatures_fewer = 1,
     .hit_points_percent = 60,
     .attack_bonus = -2,
     .morale_shift = -30},
    {},
    {.challenge_percent = 175,
     .creatures_per_ten_members = 15,
     .most_creatures_percent = 160,
     .hit_points_percent = 240,
     .attack_bonus = 2},
    {.challenge_percent = 200,
     .creatures_per_ten_members = 20,
     .most_creatures_percent = 165,
     .hit_points_percent = 280,
     .attack_bonus = 3,
     .morale_shift = 15},
    {.challenge_percent = 225,
     .creatures_per_ten_members = 20,
     .most_creatures_percent = 170,
     .hit_points_percent = 320,
     .attack_bonus = 4,
     .morale_shift = 30},
}};

// Above Normal a level's strength comes on as the party grows, in full from this
// average level: level 1 characters are too frail for anything more, and a party
// that cannot win its first fights has no way to grow.
inline constexpr unsigned full_strength_party_level = 2;

// The strength a party of this average level meets.
[[nodiscard]] constexpr MonsterStrength monster_strength(Difficulty difficulty,
                                                         unsigned party_level) noexcept
{
    auto strength = monster_strengths[static_cast<std::size_t>(difficulty)];
    if (difficulty <= Difficulty::normal || party_level >= full_strength_party_level)
    {
        return strength;
    }
    const MonsterStrength normal{};
    const auto level = static_cast<int>(party_level == 0 ? 1 : party_level);
    const auto eased = [level](unsigned from, unsigned to)
    {
        return from + (to - from) * static_cast<unsigned>(level) / full_strength_party_level;
    };
    strength.challenge_percent = eased(normal.challenge_percent, strength.challenge_percent);
    strength.creatures_per_ten_members =
        eased(normal.creatures_per_ten_members, strength.creatures_per_ten_members);
    strength.most_creatures_percent =
        eased(normal.most_creatures_percent, strength.most_creatures_percent);
    strength.hit_points_percent = eased(normal.hit_points_percent, strength.hit_points_percent);
    strength.attack_bonus = strength.attack_bonus * level / int{full_strength_party_level};
    strength.morale_shift = strength.morale_shift * level / int{full_strength_party_level};
    return strength;
}

// OpenGoldBox's difficulty table under the original rules (the user,
// 2026-11-15): what each level does to the party's foes. Normal changes
// nothing.
struct OriginalMonsterStrength
{
    int to_hit{};
    unsigned hit_points_percent{100};
    // How much better (lower) its Armor Class is.
    int armor_class_bonus{};
};

// The table's row for `difficulty`.
[[nodiscard]] constexpr OriginalMonsterStrength
original_monster_strength(Difficulty difficulty) noexcept
{
    constexpr std::array<OriginalMonsterStrength, 5> table{
        {{-2, 75, -2}, {}, {1, 125, 1}, {2, 150, 2}, {3, 200, 3}}};
    return table[static_cast<std::size_t>(difficulty)];
}

// Easy's safety net (DIFF-2): flight always succeeds, and a won fight leaves no
// member dead or down.
[[nodiscard]] constexpr bool spares_the_party(Difficulty difficulty) noexcept
{
    return difficulty == Difficulty::easy;
}

// A monster ability's option names the lowest difficulty it is on at, or
// "never" (MONSTER-2). Legendary Actions are on from Hard (the user,
// 2026-10-25), Legendary Resistance with them; abilities that take a
// character out of the fight for long, leave a lasting harm or add foes are
// on from Normal (MONSTER-4); every ability without an option is on at every
// difficulty.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 19> ability_defaults{
    {{"legendary", "hard"},
     {"legendary_resistance", "hard"},
     {"petrifying_gaze", "normal"},
     {"paralyzing_claw", "normal"},
     {"paralyzing_venom", "normal"},
     {"life_drain", "normal"},
     {"life_drain_attack", "normal"},
     {"bite", "normal"},
     {"rotting_curse", "normal"},
     {"create_specter", "normal"},
     {"dreadful_glare", "normal"},
     {"captains_charm", "normal"},
     {"enthralling_panache", "normal"},
     {"repulsion_breath", "normal"},
     {"trampling_charge", "normal"},
     {"ethereal_jaunt", "normal"},
     {"riposte", "normal"},
     {"rampage", "normal"},
     {"protective_magic", "normal"}}};

// The monster abilities off at `difficulty`, given the options as (ability,
// value) pairs. An option whose value is no difficulty and not "never" keeps
// the default.
[[nodiscard]] std::vector<std::string>
abilities_off(Difficulty difficulty, std::span<const std::pair<std::string, std::string>> options);
} // namespace opengold
#endif
