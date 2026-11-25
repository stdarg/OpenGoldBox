#ifndef OPENGOLD_ORIGINAL_RULES_MODULE_H
#define OPENGOLD_ORIGINAL_RULES_MODULE_H
#include "opengold/character_rules.h"
#include "opengold/original_combat.h"
#include "opengold/rules.h"

#include <span>

// What rules_module.cpp shares with the module's other translation units
// (camp_magic.cpp, check_dice.cpp): a character's numbers under the original
// rules.
namespace opengold::original
{
// A sheet's original numbers, which only a character made under the original
// rules has.
[[nodiscard]] const rules::CharacterSheet::Original &
original_numbers(const rules::CharacterSheet &sheet);

// The character as a fight fields it, readying `carried`.
[[nodiscard]] Combatant character_combatant(const rules::CharacterSheet &sheet,
                                            std::span<const CarriedItem> carried = {});
} // namespace opengold::original
#endif
