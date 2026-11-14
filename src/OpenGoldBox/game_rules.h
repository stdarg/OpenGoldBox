#ifndef OPENGOLDBOX_GAME_RULES_H
#define OPENGOLDBOX_GAME_RULES_H

#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/original_rules.h"
#include <filesystem>
#include <functional>
#include <memory>

// The rules a game plays by (docs/ORIGINAL-RULES.md, plan step 2): chosen when
// it begins and kept in its saves, with the setting chosen for them.
struct GameRules
{
    enum class Set
    {
        srd,
        original
    };

    Set set{Set::srd};
    // Under the original rules, how age changes ability scores.
    opengold::original::Aging aging{opengold::original::default_aging};

    bool operator==(const GameRules &) const = default;
};

// Where each set of rules finds what it plays with: the SRD's content pack
// (game_rules_file()), and the folder of the game played, whose monster
// records and item types its original rules read. For Unlimited Adventures
// that folder is the kit's, and `design` the design played, whose monsters
// the original rules fight (ua::load_design_content).
struct RulesFiles
{
    std::filesystem::path srd_pack, original_game;
    opengold::GoldBoxGame game{opengold::GoldBoxGame::pool_of_radiance};
    std::filesystem::path design;
};

// What the new game's question offers first: the SRD rules, until the
// original rules play from start to end and become the default.
inline constexpr GameRules new_game_rules{};

// The rules module of a game under these rules.
[[nodiscard]] std::unique_ptr<opengold::rules::RulesModule>
make_rules_module(const GameRules &rules, const RulesFiles &files);

// A new game's party under these rules.
[[nodiscard]] std::shared_ptr<opengold::CampaignParty> new_campaign_party(const GameRules &rules,
                                                                          const RulesFiles &files);

// A new character's start besides what creation gives it: under an
// Unlimited Adventures design, its game record's gems, jewelry and equipment
// (opengold::ua::outfit_new_character); nothing in the other games.
void outfit_for_game(opengold::CampaignParty &party, opengold::MemberId id,
                     const RulesFiles &files);

// The rules a save was made under (opengold::saved_campaign_rules), or a
// game's party plays by. Throws for rules this build does not play.
[[nodiscard]] GameRules game_rules_of(const opengold::CampaignRules &saved);
[[nodiscard]] GameRules game_rules_of(const opengold::CampaignParty &party);

// A save's party under the rules it was made with, its town built only when it
// has one, as the save dialog loads it.
struct LoadedCampaign
{
    std::shared_ptr<opengold::CampaignParty> party;
    opengold::SavedCampaign saved;
};

[[nodiscard]] LoadedCampaign
load_campaign_under_its_rules(std::string_view bytes, const RulesFiles &files,
                              std::string_view assets,
                              const std::function<opengold::por::RolfTourSession()> &make_town);

// Decodes a save of this party's game under the party's own rules, as a
// reload into the same game does.
[[nodiscard]] opengold::SavedCampaign
decode_under_party_rules(std::string_view bytes, const opengold::CampaignParty &party,
                         std::string_view assets,
                         const opengold::por::RolfTourSession *town_template = nullptr);

#endif
