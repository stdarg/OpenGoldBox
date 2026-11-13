#include "game_rules.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/creature_catalog.h"
#include "opengold/srd5.h"
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>

namespace
{
// The original rules' monsters and item types from the game's folder, read
// once: they do not change while the game runs.
const opengold::original::Content &original_content(const std::filesystem::path &game)
{
    static std::mutex guard;
    static std::map<std::filesystem::path, opengold::original::Content> loaded;
    const std::scoped_lock lock(guard);
    if (const auto found = loaded.find(game); found != loaded.end())
    {
        return found->second;
    }
    opengold::original::Content content;
    const auto catalog = opengold::por::CreatureCatalog::load(game);
    for (const auto &[id, creature] : catalog.all())
    {
        std::vector<opengold::original::CarriedItem> carried;
        for (const auto &item : creature.equipment)
        {
            carried.push_back({item.stored, item.base});
        }
        std::vector<opengold::por::EffectRecord> effects;
        for (const auto &effect : creature.effects)
        {
            effects.push_back(effect.stored);
        }
        content.bestiary.emplace(
            id.key(), opengold::original::combatant_from_record(creature.stored, carried, effects));
    }
    std::ifstream items(game / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(items), {}};
    auto types = opengold::por::decode_item_templates(bytes);
    if (!types)
    {
        throw std::runtime_error("The game's ITEMS file cannot be read");
    }
    content.item_types = std::move(*types);
    return loaded.emplace(game, std::move(content)).first->second;
}

std::shared_ptr<const opengold::rules::CharacterRules> character_rules(const GameRules &rules)
{
    if (rules.set == GameRules::Set::original)
    {
        return opengold::original::character_rules(rules.aging);
    }
    return opengold::srd5::character_rules();
}
} // namespace

std::unique_ptr<opengold::rules::RulesModule> make_rules_module(const GameRules &rules,
                                                                const RulesFiles &files)
{
    if (rules.set == GameRules::Set::original)
    {
        return opengold::original::rules_module(original_content(files.original_game));
    }
    return opengold::srd5::load(files.srd_pack);
}

std::shared_ptr<opengold::CampaignParty> new_campaign_party(const GameRules &rules,
                                                            const RulesFiles &files)
{
    return std::make_shared<opengold::CampaignParty>(make_rules_module(rules, files),
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
load_campaign_under_its_rules(std::string_view bytes, const RulesFiles &files,
                              std::string_view assets,
                              const std::function<opengold::por::RolfTourSession()> &make_town)
{
    const auto rules = game_rules_of(opengold::saved_campaign_rules(bytes));
    auto module = make_rules_module(rules, files);
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
