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

// What a new character starts with where the game's maker chooses it: an
// Unlimited Adventures design's game record (docs/games/ua.md).
struct StartingStock
{
    // Shared among a character's classes, then trained to the levels it reaches.
    int experience{};
    int platinum{};
};

// Creation under Unlimited Adventures' rules for a design whose characters
// start with `start`.
[[nodiscard]] std::unique_ptr<rules::CharacterRules> design_character_rules(Aging aging,
                                                                            StartingStock start);
} // namespace opengold::original
#endif
