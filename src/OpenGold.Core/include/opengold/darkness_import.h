#ifndef OPENGOLD_DARKNESS_IMPORT_H
#define OPENGOLD_DARKNESS_IMPORT_H

#include "opengold/campaign_party.h"

#include <filesystem>

namespace opengold
{

// How Pools of Darkness takes in a Secret of the Silver Blades character's
// items (its LoadChar, from Secret's files): each record as it was, its type
// and name words by the numbers both games share (to be kept readied as they
// were, CampaignParty::ConvertedItems), named again by Pools of Darkness's own words. A bundle
// (type 73) is left behind with what it holds; boots of type 45 become +2 and a cloak of type 58
// gives displacement (effect 89). Reads the Pools of Darkness folder's ITEMS and GAME.EXE; throws
// when they cannot be read.
[[nodiscard]] CampaignParty::ItemConversion
secret_items_for_darkness(const std::filesystem::path &darkness_folder);

} // namespace opengold

#endif
