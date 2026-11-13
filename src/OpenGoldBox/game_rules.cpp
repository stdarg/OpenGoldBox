#include "game_rules.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/srd5.h"
#include <stdexcept>

namespace
{
std::shared_ptr<const opengold::rules::CharacterRules> character_rules(const GameRules &rules)
{
    if (rules.set == GameRules::Set::original)
    {
        return opengold::original::character_rules(rules.aging);
    }
    return opengold::srd5::character_rules();
}
} // namespace

std::unique_ptr<opengold::rules::RulesModule>
make_rules_module(const GameRules &rules, const std::filesystem::path &srd_pack)
{
    if (rules.set == GameRules::Set::original)
    {
        return opengold::original::rules_module();
    }
    return opengold::srd5::load(srd_pack);
}

std::shared_ptr<opengold::CampaignParty> new_campaign_party(const GameRules &rules,
                                                            const std::filesystem::path &srd_pack)
{
    return std::make_shared<opengold::CampaignParty>(make_rules_module(rules, srd_pack),
                                                     character_rules(rules));
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
load_campaign_under_its_rules(std::string_view bytes, const std::filesystem::path &srd_pack,
                              std::string_view assets,
                              const std::function<opengold::por::RolfTourSession()> &make_town)
{
    const auto rules = game_rules_of(opengold::saved_campaign_rules(bytes));
    auto module = make_rules_module(rules, srd_pack);
    auto creation = character_rules(rules);
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
