#ifndef OPENGOLD_DIFFICULTY_H
#define OPENGOLD_DIFFICULTY_H
#include <array>
#include <optional>
#include <string_view>

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
} // namespace opengold
#endif
