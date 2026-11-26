#ifndef OPENGOLD_ORIGINAL_H
#define OPENGOLD_ORIGINAL_H
#include "opengold/character_rules.h"
#include "opengold/original_combat.h"
#include "opengold/original_game.h"
#include "opengold/original_rules.h"
#include <memory>

// The original rules as a rules module the game plays by
// (docs/ORIGINAL-RULES.md). Core never includes this header: it reaches the
// module through the rules interface (docs/TECH.md).
namespace opengold::original
{
// Character creation under a game's original rules, with this aging rule.
[[nodiscard]] std::unique_ptr<rules::CharacterRules>
character_rules(Aging aging = default_aging, GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// Creation under Unlimited Adventures' rules for a design whose characters
// start with `start`.
[[nodiscard]] std::unique_ptr<rules::CharacterRules> design_character_rules(Aging aging,
                                                                            StartingStock start);

// The original rules as the module the game's fights and party run through.
[[nodiscard]] std::unique_ptr<rules::RulesModule> rules_module(Content content = {});
} // namespace opengold::original
#endif
