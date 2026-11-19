#ifndef OPENGOLD_DEATH_KNIGHTS_IMPORT_H
#define OPENGOLD_DEATH_KNIGHTS_IMPORT_H

#include "opengold/campaign_party.h"

#include <filesystem>

namespace opengold
{
// Native LoadChar retains Champions' item records except seven discontinued
// types, using Death Knights' own templates and name words.
[[nodiscard]] CampaignParty::ItemConversion
champions_items_for_death_knights(const std::filesystem::path &folder);

// Original characters own their coins. Split the shared campaign purse
// without losing remainder coins when all winners are imported.
[[nodiscard]] CampaignParty::BroughtMoney
champions_money_for_death_knights(const Purse &, std::size_t character, std::size_t count);
} // namespace opengold

#endif
