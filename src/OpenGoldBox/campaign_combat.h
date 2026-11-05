#ifndef OPENGOLDBOX_CAMPAIGN_COMBAT_H
#define OPENGOLDBOX_CAMPAIGN_COMBAT_H
#include "godot_nodes.h"
#include "opengold/campaign_encounter.h"
#include "opengold/campaign_party.h"
#include "opengold/character_art.h"
#include "opengold/combat_body_catalog.h"
#include <memory>

// The game's combat screen, prepared for the party to fight `encounter`, not
// yet attached. The party's members wear the art their equipment selects.
// The game and the developer tools that stage fights open it this one way.
[[nodiscard]] presentation::NodeOwner<> campaign_combat_scene(
    const std::shared_ptr<opengold::CampaignParty> &party, const opengold::por::CharacterArt &art,
    const opengold::por::CombatBodyCatalog &catalog, opengold::CampaignEncounter encounter);

#endif
