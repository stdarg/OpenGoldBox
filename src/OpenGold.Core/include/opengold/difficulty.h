#ifndef OPENGOLD_DIFFICULTY_H
#define OPENGOLD_DIFFICULTY_H
#include <algorithm>
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

[[nodiscard]] constexpr std::optional<Difficulty> parse_difficulty(std::string_view name) noexcept
{
    for (std::size_t n = 0; n < difficulty_names.size(); ++n)
    {
        if (difficulty_names[n] == name)
        {
            return static_cast<Difficulty>(n);
        }
    }
    return std::nullopt;
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
     .hit_points_percent = 220,
     .attack_bonus = 2},
    {.challenge_percent = 200,
     .creatures_per_ten_members = 20,
     .hit_points_percent = 230,
     .attack_bonus = 3,
     .morale_shift = 15},
    {.challenge_percent = 225,
     .creatures_per_ten_members = 20,
     .hit_points_percent = 265,
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
    strength.hit_points_percent = eased(normal.hit_points_percent, strength.hit_points_percent);
    strength.attack_bonus = strength.attack_bonus * level / int{full_strength_party_level};
    strength.morale_shift = strength.morale_shift * level / int{full_strength_party_level};
    return strength;
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
[[nodiscard]] inline std::vector<std::string>
abilities_off(Difficulty difficulty, std::span<const std::pair<std::string, std::string>> options)
{
    std::vector<std::pair<std::string, std::string>> merged(ability_defaults.begin(),
                                                            ability_defaults.end());
    for (const auto &[ability, value] : options)
    {
        if (value != "never" && !parse_difficulty(value))
        {
            continue;
        }
        const auto found = std::find_if(merged.begin(), merged.end(),
                                        [&](const auto &option)
                                        {
                                            return option.first == ability;
                                        });
        if (found == merged.end())
        {
            merged.emplace_back(ability, value);
        }
        else
        {
            found->second = value;
        }
    }
    std::vector<std::string> off;
    for (const auto &[ability, value] : merged)
    {
        const auto from = parse_difficulty(value);
        if (!from || difficulty < *from)
        {
            off.push_back(ability);
        }
    }
    return off;
}
} // namespace opengold
#endif
