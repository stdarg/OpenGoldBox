#ifndef OPENGOLD_ORIGINAL_H
#define OPENGOLD_ORIGINAL_H
#include "opengold/character_rules.h"
#include "opengold/original_rules.h"
#include <memory>

// The original rules as a rules module the game plays by
// (docs/ORIGINAL-RULES.md).
namespace opengold::original
{
// What saves and sheets made under the original rules of a game carry: its
// content names the game whose tables they play by.
[[nodiscard]] rules::Identity identity(GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// The game whose original rules an identity names (identity()).
[[nodiscard]] GoldBoxGame game_of(const rules::Identity &identity);
// Character creation under a game's original rules, with this aging rule.
[[nodiscard]] std::unique_ptr<rules::CharacterRules>
character_rules(Aging aging = default_aging, GoldBoxGame game = GoldBoxGame::pool_of_radiance);
} // namespace opengold::original
#endif
