#ifndef OPENGOLD_TREASURES_IMPORT_H
#define OPENGOLD_TREASURES_IMPORT_H

#include "opengold/campaign_party.h"

#include <filesystem>

namespace opengold
{

// How Treasures of the Savage Frontier takes in a Gateway to the Savage
// Frontier character's items (its LoadChar and ValidateItem): each name word
// found again among its own by its letters (Gateway's words are BORD.DAX
// record 10, its own GAME.EXE's), the type renumbered by its program's table,
// unreadied; an item with a word or type it lacks, or of effect 98, is left
// behind. Reads the Treasures folder's files; throws when they are not the
// release this engine reads.
[[nodiscard]] CampaignParty::ItemConversion
gateway_items_for_treasures(const std::filesystem::path &treasures_folder);

} // namespace opengold

#endif
