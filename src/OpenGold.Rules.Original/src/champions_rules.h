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

// A Krynn race's row in the Krynn games' tables; throws for any other race.
[[nodiscard]] std::size_t krynn_race_index(Race race);

// The three Krynn games share a cleric's deity: its gifts and the way it
// favors the cleric in a fight.
// A spell the character knows from now on, added once; 0 is no spell.
void learn_known_spell(rules::CharacterSheet::Original &numbers, int number);
// A cleric's gifts from its deity (its spells 501 to 507) and the slot that
// holds them: three for the fourth deity, none for the sixth, one otherwise.
void learn_deity_gifts(rules::CharacterSheet::Original &numbers);
// One off the number a cleric of the third deity needs to hit, and a dwarf
// of the sixth.
void favor_thac0_by_deity(rules::CharacterSheet::Original &numbers);

[[nodiscard]] AbilityLimits champions_ability_limits(Race race, Gender gender);
[[nodiscard]] std::span<const Profession> champions_professions(Race race);
[[nodiscard]] std::array<int, 4> champions_age_categories(Race race);
[[nodiscard]] int champions_starting_age(Race race, Profession profession, RandomState &random);
[[nodiscard]] std::array<int, 8> champions_thief_adjustments(Race race);
[[nodiscard]] int champions_thac0(const ClassLevels &levels);
[[nodiscard]] std::array<int, 5> champions_saving_throws(const ClassLevels &levels);
} // namespace opengold::original
#endif
