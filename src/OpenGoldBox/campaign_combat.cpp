#include "campaign_combat.h"
#include "combat_view.h"
#include <stdexcept>
#include <utility>
#include <vector>

presentation::NodeOwner<> campaign_combat_scene(
    const std::shared_ptr<opengold::CampaignParty> &party, const opengold::por::CharacterArt &art,
    const opengold::por::CombatBodyCatalog &catalog, opengold::CampaignEncounter encounter)
{
    std::vector<opengold::CombatArt> images;
    for (const auto &participant : party->participants())
    {
        const auto resolved =
            opengold::por::resolve_combat_appearance(party->member(participant.id), catalog);
        images.push_back({participant.id, resolved.icon(art, opengold::por::IconPose::ready),
                          resolved.icon(art, opengold::por::IconPose::action),
                          resolved.selection.matched ? std::string{} : resolved.selection.label});
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
