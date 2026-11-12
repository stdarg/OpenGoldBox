#ifndef OPENGOLD_ORIGINAL_H
#define OPENGOLD_ORIGINAL_H
#include "opengold/character_rules.h"
#include "opengold/original_rules.h"
#include <memory>

// The original rules as a rules module the game plays by
// (docs/ORIGINAL-RULES.md).
namespace opengold::original
{
// What saves and sheets made under the original rules carry.
[[nodiscard]] rules::Identity identity();
// Character creation under the original rules, for a game with this aging rule.
[[nodiscard]] std::unique_ptr<rules::CharacterRules> character_rules(Aging aging = default_aging);
} // namespace opengold::original
#endif
