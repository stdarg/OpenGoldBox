#include "game_rules.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/srd5.h"
#include "opengold/ua_session.h"
#include <map>
#include <mutex>
#include <stdexcept>

namespace
{
// The original rules' monsters and item types from the game's folder, read
// once: they do not change while the game runs.
const opengold::original::Content &original_content(const RulesFiles &files)
{
    static std::mutex guard;
    static std::map<std::filesystem::path, opengold::original::Content> loaded;
    const std::scoped_lock lock(guard);
    // A design's monsters are its own: each design is read once.
    const auto design = files.game == opengold::GoldBoxGame::unlimited_adventures;
    const auto &folder = design ? files.design : files.original_game;
    if (const auto found = loaded.find(folder); found != loaded.end())
    {
        return found->second;
    }
    return loaded
        .emplace(folder, design ? opengold::ua::load_design_content(
                                      opengold::ua::DesignFiles(files.original_game, files.design))
                                : opengold::load_original_content(folder, files.game))
        .first->second;
}

std::shared_ptr<const opengold::rules::CharacterRules> character_rules(const GameRules &rules,
                                                                       const RulesFiles &files)
{
    if (rules.set != GameRules::Set::original)
    {
        return opengold::srd5::character_rules();
    }
    if (files.game != opengold::GoldBoxGame::unlimited_adventures)
    {
        return opengold::original::character_rules(rules.aging, files.game);
    }
    // A design's characters start as its game record says.
    const opengold::ua::DesignFiles design(files.original_game, files.design);
    const auto record = design.data("GAME", 1);
    const auto game = record ? opengold::ua::decode_game_settings(*record) : std::nullopt;
    if (!game)
    {
        throw std::runtime_error("The design's game record does not decode");
    }
    return opengold::original::design_character_rules(rules.aging,
                                                      opengold::ua::starting_stock(*game));
}
} // namespace

void outfit_for_game(opengold::CampaignParty &party, opengold::MemberId id, const RulesFiles &files)
{
    if (files.game == opengold::GoldBoxGame::unlimited_adventures)
    {
        opengold::ua::outfit_new_character(
            party, id, opengold::ua::DesignFiles(files.original_game, files.design));
    }
}

std::unique_ptr<opengold::rules::RulesModule> make_rules_module(const GameRules &rules,
                                                                const RulesFiles &files)
{
    if (rules.set == GameRules::Set::original)
    {
        return opengold::original::rules_module(original_content(files));
    }
    return opengold::srd5::load(files.srd_pack);
}

std::shared_ptr<opengold::CampaignParty> new_campaign_party(const GameRules &rules,
                                                            const RulesFiles &files)
{
    return std::make_shared<opengold::CampaignParty>(make_rules_module(rules, files),
                                                     character_rules(rules, files));
}

GameRules game_rules_of(const opengold::CampaignRules &saved)
{
    // Any other module is the SRD's: decoding refuses one this build does not
    // play, or an older version of it, with the older-save message.
    if (saved.identity.module != opengold::original::identity().module)
    {
        return {};
    }
    const auto aging = opengold::original::aging_named(saved.setting);
    if (!aging)
    {
        throw std::runtime_error("Unknown aging setting in the campaign save");
    }
    return {GameRules::Set::original, *aging};
}

GameRules game_rules_of(const opengold::CampaignParty &party)
{
    return game_rules_of(opengold::CampaignRules{party.identity(), party.game_setting()});
}

LoadedCampaign
load_campaign_under_its_rules(std::string_view bytes, const RulesFiles &files,
                              std::string_view assets,
                              const std::function<opengold::por::RolfTourSession()> &make_town)
{
    const auto rules = game_rules_of(opengold::saved_campaign_rules(bytes));
    auto module = make_rules_module(rules, files);
    auto creation = character_rules(rules, files);
    auto saved = opengold::decode_campaign(bytes, *creation, *module, assets, make_town);
    return {std::make_shared<opengold::CampaignParty>(std::move(module), std::move(creation)),
            std::move(saved)};
}

opengold::SavedCampaign
decode_under_party_rules(std::string_view bytes, const opengold::CampaignParty &party,
                         std::string_view assets,
                         const opengold::por::RolfTourSession *town_template)
{
    return opengold::decode_campaign(bytes, *party.character_rules(), party.rule_module(), assets,
                                     town_template);
}
