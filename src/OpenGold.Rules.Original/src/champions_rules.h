#ifndef OPENGOLD_CHAMPIONS_RULES_H
#define OPENGOLD_CHAMPIONS_RULES_H
#include "opengold/original_rules.h"
#include "opengold/pool_radiance.h"

namespace opengold::original
{
inline constexpr int champions_dragon_trait = 0x400;
inline constexpr int champions_reptile_trait = 0x800;

[[nodiscard]] constexpr bool champions_race(Race race) noexcept
{
    return race >= Race::silvanesti_elf && race <= Race::krynn_human;
}

// Borrowed record data: raw Champions race and robe codes, not canonical combat races.
[[nodiscard]] std::optional<std::string_view>
champions_ready_refusal(const por::ItemRecord &item, unsigned race, unsigned robe) noexcept;

[[nodiscard]] AbilityLimits champions_ability_limits(Race race, Gender gender);
[[nodiscard]] std::span<const Profession> champions_professions(Race race);
[[nodiscard]] std::array<int, 4> champions_age_categories(Race race);
[[nodiscard]] int champions_starting_age(Race race, Profession profession, RandomState &random);
[[nodiscard]] std::array<int, 8> champions_thief_adjustments(Race race);
[[nodiscard]] int champions_thac0(const ClassLevels &levels);
[[nodiscard]] std::array<int, 5> champions_saving_throws(const ClassLevels &levels);
} // namespace opengold::original
#endif
