#ifndef OPENGOLD_CAMPAIGN_SAVE_H
#define OPENGOLD_CAMPAIGN_SAVE_H
#include "opengold/difficulty.h"
#include "opengold/rolf_tour.h"
#include <functional>

namespace opengold
{
// What a save keeps of a design played with Unlimited Adventures' kit
// (docs/games/ua.md, Saving and loading a game): the design's folder, which
// must still be there to load it, and the session's own state
// (ua::UaSession::saved_state).
struct DesignSave
{
    std::string folder;
    std::string state;
};

// A fully staged replacement. Loading never mutates the current campaign.
struct SavedCampaign
{
    PartyState party;
    std::optional<por::RolfTourSession> town;
    // Members saved before characters chose original portraits, still on the
    // default head and body: the game gives each a random original portrait.
    std::vector<MemberId> unchosen_portraits;
    // The game the campaign belongs to; saves before format 48 are Pool of Radiance's.
    GoldBoxGame game{GoldBoxGame::pool_of_radiance};
    // A design's play, in an Unlimited Adventures save (format 51 on).
    std::optional<DesignSave> design;
};

// The rules a save was made under: their module and the setting the game chose
// for them (CharacterRules::game_setting), which the game must load it with.
// The game the save belongs to comes with them, so loading it can start that
// game.
struct CampaignRules
{
    rules::Identity identity;
    std::string setting;
    GoldBoxGame game{GoldBoxGame::pool_of_radiance};
    // An Unlimited Adventures save's design folder, whose monsters its rules
    // need; empty for the other games.
    std::string design_folder;
};

// Reads only the save's header, its rules and game; throws as decode_campaign
// does on a save that is not a campaign or is too old.
[[nodiscard]] CampaignRules saved_campaign_rules(std::string_view);
template <bool Reading> struct SaveCodec;
[[nodiscard]] std::string campaign_asset_identity(const std::filesystem::path &directory);
// An Unlimited Adventures campaign has no DAX town but a design (required for
// that game, refused for the others).
[[nodiscard]] std::string encode_campaign(const CampaignParty &, const por::RolfTourSession *,
                                          std::string_view assets,
                                          GoldBoxGame game = GoldBoxGame::pool_of_radiance,
                                          const DesignSave *design = nullptr);
// The campaign in an earlier format this build still reads, from 45, for
// checks that such saves load. Throws when the party holds what that format
// cannot record: before 47, a character made under the original rules; before
// 48, a game other than Pool of Radiance; before 49, a setting for its rules;
// before 50, what a level rolled; before 51, a design.
[[nodiscard]] std::string encode_campaign_in_format(
    const CampaignParty &, const por::RolfTourSession *, std::string_view assets, unsigned format,
    GoldBoxGame game = GoldBoxGame::pool_of_radiance, const DesignSave *design = nullptr);
[[nodiscard]] SavedCampaign decode_campaign(std::string_view, const rules::CharacterRules &,
                                            const rules::RulesModule &, std::string_view assets,
                                            const por::RolfTourSession *town_template);
// The same, building the town template only once the save is known to need
// one: loading the town decodes every original archive, which a rejected or
// town-less save should not pay for (Effective C++ Item 26).
[[nodiscard]] SavedCampaign
decode_campaign(std::string_view, const rules::CharacterRules &, const rules::RulesModule &,
                std::string_view assets,
                const std::function<por::RolfTourSession()> &make_town_template);
// Above Easy, a loaded game gets new dice from `fresh`, so what comes next cannot
// be known from an earlier play; on Easy every roll replays as it fell.
void reseed_after_load(SavedCampaign &, Difficulty, std::uint64_t fresh);
[[nodiscard]] std::string read_campaign_file(const std::filesystem::path &);
// Writes, flushes and verifies a temporary file before replacing the destination.
// An overwrite retains the previous file at <path>.bak.
void write_campaign_file(const std::filesystem::path &, std::string_view bytes);
} // namespace opengold
#endif
