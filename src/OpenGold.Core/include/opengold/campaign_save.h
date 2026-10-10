#ifndef OPENGOLD_CAMPAIGN_SAVE_H
#define OPENGOLD_CAMPAIGN_SAVE_H
#include "opengold/rolf_tour.h"
#include <functional>

namespace opengold
{
// A fully staged replacement. Loading never mutates the current campaign.
struct SavedCampaign
{
    PartyState party;
    std::optional<por::RolfTourSession> town;
};
template <bool Reading> struct SaveCodec;
[[nodiscard]] std::string campaign_asset_identity(const std::filesystem::path &directory);
[[nodiscard]] std::string encode_campaign(const CampaignParty &, const por::RolfTourSession *,
        std::string_view assets);
[[nodiscard]] SavedCampaign decode_campaign(std::string_view, const rules::CharacterRules &,
        const rules::RulesModule &, std::string_view assets,
        const por::RolfTourSession *town_template);
// The same, building the town template only once the save is known to need
// one: loading the town decodes every original archive, which a rejected or
// town-less save should not pay for (Effective C++ Item 26).
[[nodiscard]] SavedCampaign decode_campaign(std::string_view, const rules::CharacterRules &,
        const rules::RulesModule &, std::string_view assets,
        const std::function<por::RolfTourSession()> &make_town_template);
[[nodiscard]] std::string read_campaign_file(const std::filesystem::path &);
// Writes, flushes and verifies a temporary file before replacing the destination.
// An overwrite retains the previous file at <path>.bak.
void write_campaign_file(const std::filesystem::path &, std::string_view bytes);
} // namespace opengold
#endif
