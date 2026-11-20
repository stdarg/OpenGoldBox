#ifndef OPENGOLD_DARK_QUEEN_IMPORT_H
#define OPENGOLD_DARK_QUEEN_IMPORT_H

#include "opengold/campaign_party.h"

#include <filesystem>

namespace opengold
{
// Native Death Knights item conversion, using the destination's types and
// names. Keep surviving items readied with ConvertedItems::readied_as_they_were.
[[nodiscard]] CampaignParty::ItemConversion
death_knights_items_for_dark_queen(const std::filesystem::path &folder);
} // namespace opengold

#endif
