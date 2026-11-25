#ifndef OPENGOLD_ORIGINAL_CHECK_DICE_H
#define OPENGOLD_ORIGINAL_CHECK_DICE_H
#include "opengold/original_rules.h"
#include "opengold/rules.h"

#include <string_view>
#include <utility>

// The chances the original's checks outside a fight stand for, and Krynn's
// door dice (check_dice.cpp).
namespace opengold::original
{
[[nodiscard]] unsigned thief_chance(const rules::CharacterSheet &sheet, ThiefSkill skill);
[[nodiscard]] unsigned bash_chance(const rules::CharacterSheet &sheet);
[[nodiscard]] unsigned original_chance(const rules::CharacterSheet &sheet, rules::Ability ability,
                                       std::string_view skill);
[[nodiscard]] std::pair<unsigned, unsigned> krynn_door_die(const rules::CharacterSheet &sheet,
                                                           bool magical);
} // namespace opengold::original
#endif
