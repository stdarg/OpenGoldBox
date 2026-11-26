#ifndef OPENGOLD_CHAMPIONS_RULES_H
#define OPENGOLD_CHAMPIONS_RULES_H
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
#include "opengold/pool_radiance.h"

#include <algorithm>

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

// The three Krynn games lay out their thac0, spell and profession tables
// differently but read them the same way; each passes its table as a reader.

// The thac0 of the best attack score among the classes the character has,
// never below floor; score(slot, level) reads the game's table for a class
// with a level above 0. Under a floor of 0, a character with no class gets 20.
template <typename AttackScore>
[[nodiscard]] int best_thac0(const ClassLevels &levels, AttackScore score, int floor)
{
    int best = floor;
    for (std::size_t at = 0; at < levels.size(); ++at)
    {
        if (levels[at] > 0)
        {
            best = std::max(best, score(static_cast<ClassSlot>(at), levels[at]));
        }
    }
    return best ? 60 - best : 20;
}

// The spells of the first highest spell levels (at most level_count) in table
// order; row(level) gives a level's native spell numbers, counting levels from
// 0, and an entry with no canonical spell is no spell.
template <typename SpellRow>
[[nodiscard]] std::vector<int> learning_spells_by_level(SpellRow row, int level_count, int highest,
                                                        GoldBoxGame game)
{
    std::vector<int> spells;
    for (int level = 0; level < std::clamp(highest, 0, level_count); ++level)
    {
        for (const int own : row(static_cast<std::size_t>(level)))
        {
            if (const auto number = canonical_spell(own, game))
            {
                spells.push_back(number);
            }
        }
    }
    return spells;
}

// Each Krynn race's professions in its native order; row(race) gives a race's
// row, a count and then that many native class ids, where knight_id is the
// knight's.
template <typename RaceRow>
[[nodiscard]] std::array<std::vector<Profession>, 7> professions_by_race(RaceRow row, int knight_id)
{
    std::array<std::vector<Profession>, 7> choices;
    for (std::size_t race = 0; race < choices.size(); ++race)
    {
        const auto native = row(race);
        for (int at = 1; at <= native[0]; ++at)
        {
            const int id = native[static_cast<std::size_t>(at)];
            choices[race].push_back(id == knight_id ? Profession::knight
                                                    : static_cast<Profession>(id));
        }
    }
    return choices;
}

[[nodiscard]] AbilityLimits champions_ability_limits(Race race, Gender gender);
[[nodiscard]] std::span<const Profession> champions_professions(Race race);
[[nodiscard]] std::array<int, 4> champions_age_categories(Race race);
[[nodiscard]] int champions_starting_age(Race race, Profession profession, RandomState &random);
[[nodiscard]] std::array<int, 8> champions_thief_adjustments(Race race);
[[nodiscard]] int champions_thac0(const ClassLevels &levels);
[[nodiscard]] std::array<int, 5> champions_saving_throws(const ClassLevels &levels);
} // namespace opengold::original
#endif
