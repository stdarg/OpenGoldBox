#include "campaign_combat.h"
#include "combat_view.h"
#include <stdexcept>
#include <utility>
#include <vector>

presentation::NodeOwner<> campaign_combat_scene(
    const std::shared_ptr<opengold::CampaignParty> &party, const opengold::por::CharacterArt &art,
    const opengold::por::CombatBodyCatalog *looks, opengold::CampaignEncounter encounter)
{
    std::vector<opengold::CombatArt> images;
    for (const auto &participant : party->participants())
    {
        auto icons = opengold::por::party_combat_icons(party->member(participant.id), art, looks);
        images.push_back({participant.id, std::move(icons.ready), std::move(icons.action),
                          std::move(icons.missing_combination)});
    }
    auto owned = presentation::instantiate_scene("res://scenes/combat_demo.tscn");
    auto *combat = godot::Object::cast_to<CombatView>(owned.get());
    if (!combat)
    {
        throw std::runtime_error("Invalid combat scene");
    }
    combat->set_name("CampaignCombat");
    combat->campaign_party(party, std::move(images));
    combat->campaign_encounter(std::move(encounter));
    combat->prepare_combat();
    return owned;
}
