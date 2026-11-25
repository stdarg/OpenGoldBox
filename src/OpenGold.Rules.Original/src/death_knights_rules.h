#ifndef OPENGOLD_DEATH_KNIGHTS_RULES_H
#define OPENGOLD_DEATH_KNIGHTS_RULES_H

#include "opengold/original_rules.h"
#include "opengold/original_spells.h"

namespace opengold::original
{
using KrynnSpellRows = std::array<std::array<int, 9>, 4>;

// A class's row among the progressions Death Knights and Dark Queen share:
// a cleric's by its deity, a knight's by its order, a mage's by its robe;
// none for a druid.
[[nodiscard]] std::optional<std::size_t> krynn_progression(ClassSlot slot, KrynnOrders orders);
// The druid spells the character's druid slots let it cast, each known from
// now on.
void learn_druid_spells(rules::CharacterSheet::Original &numbers, GoldBoxGame game);

[[nodiscard]] AbilityLimits death_knights_ability_limits(Race race, Gender gender);
[[nodiscard]] rules::AbilityArray<int> death_knights_class_minimums(Profession profession);
[[nodiscard]] std::span<const Profession> death_knights_professions(Race race);
[[nodiscard]] bool death_knights_alignment_allowed(Profession profession, Alignment alignment);
[[nodiscard]] int death_knights_level_limit(Race race, ClassSlot slot);
[[nodiscard]] std::optional<int> death_knights_experience_for_level(ClassSlot slot, int level,
                                                                    KrynnOrders orders = {});
[[nodiscard]] std::uint64_t death_knights_calendar_day(std::uint64_t minutes) noexcept;
[[nodiscard]] int death_knights_moon_allowance(const rules::CharacterSheet::Original &numbers);
void death_knights_calendar(rules::CharacterSheet::Original &numbers, std::uint64_t days);
[[nodiscard]] int death_knights_caster_level(const ClassLevels &levels, SpellClass caster, int robe,
                                             int phase);
[[nodiscard]] int death_knights_thac0(const ClassLevels &levels);
[[nodiscard]] std::array<int, 5> death_knights_saving_throws(const ClassLevels &levels);
void recalculate_death_knights_numbers(rules::CharacterSheet::Original &numbers,
                                       const rules::AbilityArray<int> &scores);
[[nodiscard]] std::vector<int> death_knights_learning_spells(int robe, int highest);
[[nodiscard]] std::vector<int> death_knights_starting_spells(int robe, int mage_level);

// Native class slots, before equipment and the shared moon allowance.
[[nodiscard]] KrynnSpellRows death_knights_base_slots(const ClassLevels &levels, int wisdom,
                                                      KrynnOrders orders);
} // namespace opengold::original

#endif
