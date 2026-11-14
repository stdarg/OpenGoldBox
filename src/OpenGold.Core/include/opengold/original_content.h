#ifndef OPENGOLD_ORIGINAL_CONTENT_H
#define OPENGOLD_ORIGINAL_CONTENT_H

#include "opengold/campaign_party.h"
#include "opengold/creature_catalog.h"
#include "opengold/gold_box_game.h"
#include "opengold/original_combat.h"

#include <filesystem>
#include <map>

namespace opengold
{

// What the original rules need of a game's folder: every creature as its
// record fights, with its items and effects, and the item types. Throws when
// the folder's creatures or ITEMS cannot be read.
[[nodiscard]] original::Content load_original_content(const std::filesystem::path &folder,
                                                      GoldBoxGame game);

// The item definitions Pools of Darkness copies a new item from (its ITEM0,
// docs/games/darkness.md, Items), by number from 1, without the empty ones,
// named as the game names them. Throws when the folder's ITEMS, item words or
// definitions cannot be read.
[[nodiscard]] std::map<unsigned, por::Equipment>
load_item_definitions(const std::filesystem::path &folder, GoldBoxGame game);

// Gives a character just made under the original rules the game's starting
// equipment (original::starting_items), copied from `definitions` and known
// for what it is, and readies what the original readies, as its rules allow:
// an item the character's class or hands refuse stays in the pack.
void outfit_original_character(CampaignParty &party, MemberId id,
                               const std::map<unsigned, por::Equipment> &definitions);

} // namespace opengold

#endif
