#ifndef OPENGOLD_DARK_QUEEN_RULES_H
#define OPENGOLD_DARK_QUEEN_RULES_H

#include "death_knights_rules.h"

namespace opengold::original
{
struct Effect;
// Ordinary green fire shares the same native defenses in combat and on the road.
[[nodiscard]] int dark_queen_green_fire_damage(int damage, std::vector<Effect> &effects,
                                               RandomState &random);
// Canonical spell after the native Death Knights import remaps its spell byte.
[[nodiscard]] int dark_queen_import_spell(int canonical) noexcept;
[[nodiscard]] std::vector<StartingItem> dark_queen_starting_items(Race race, Profession profession);
[[nodiscard]] AbilityLimits dark_queen_ability_limits(Race race, Gender gender);
[[nodiscard]] rules::AbilityArray<int> dark_queen_class_minimums(Profession profession);
[[nodiscard]] std::span<const Profession> dark_queen_professions(Race race);
[[nodiscard]] bool dark_queen_alignment_allowed(Profession profession, Alignment alignment);
[[nodiscard]] int dark_queen_level_limit(Race race, ClassSlot slot, KrynnOrders orders = {});
[[nodiscard]] std::optional<int> dark_queen_experience_for_level(ClassSlot slot, int level,
                                                                 KrynnOrders orders = {});
[[nodiscard]] int dark_queen_starting_age(Race race, Profession profession, RandomState &random);
[[nodiscard]] KrynnSpellRows dark_queen_base_slots(const ClassLevels &levels, int wisdom,
                                                   int intelligence, KrynnOrders orders);
[[nodiscard]] int dark_queen_caster_level(const ClassLevels &levels, SpellClass caster, int robe,
                                          int phase);
[[nodiscard]] int dark_queen_hit_point_maximum(const ModifiedCharacter &character,
                                               int constitution);
[[nodiscard]] int dark_queen_hit_point_minimum(const ModifiedCharacter &character,
                                               int constitution);
[[nodiscard]] int dark_queen_thac0(const ClassLevels &levels);
[[nodiscard]] std::array<int, 5> dark_queen_saving_throws(const ClassLevels &levels);
void recalculate_dark_queen_numbers(rules::CharacterSheet::Original &numbers,
                                    const rules::AbilityArray<int> &scores);
[[nodiscard]] std::vector<int> dark_queen_learning_spells(int robe, int highest);
[[nodiscard]] std::vector<int> dark_queen_starting_spells(int robe,
                                                          const std::array<int, 9> &mage_slots);
} // namespace opengold::original

#endif
