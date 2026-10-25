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

// A monster ability's option names the lowest difficulty it is on at, or
// "never" (MONSTER-2). Legendary Actions are on from Hard (the user,
// 2026-10-25); every ability without an option is on at every difficulty.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 1> ability_defaults{
    {{"legendary", "hard"}}};

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
