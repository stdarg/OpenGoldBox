#ifndef OPENGOLD_ORIGINAL_GAME_H
#define OPENGOLD_ORIGINAL_GAME_H
#include "opengold/gold_box_game.h"
#include "opengold/rules.h"

// What a game played under its original rules carries in its saves and
// sheets: facts of the original games' systems, which Core uses without the
// original rules module (opengold/original.h).
namespace opengold::original
{
// What saves and sheets made under the original rules of a game carry: its
// content names the game whose tables they play by.
[[nodiscard]] rules::Identity identity(GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// The game whose original rules an identity names (identity()).
[[nodiscard]] GoldBoxGame game_of(const rules::Identity &identity);

// What a new character starts with where the game's maker chooses it: an
// Unlimited Adventures design's game record (docs/games/ua.md).
struct StartingStock
{
    // Shared among a character's classes, then trained to the levels it reaches.
    int experience{};
    int platinum{};
};

// What a refused proposal of Treasures' romance leaves on its party member,
// for the party to keep (CampaignParty::keep_lasting_change): 6 Charisma
// lost for good and a refused love (its romance.pas, UpdateRelationship).
[[nodiscard]] rules::AdvancementChoice romance_refused();
} // namespace opengold::original
#endif
