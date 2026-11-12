#ifndef OPENGOLD_CAMPAIGN_SESSION_H
#define OPENGOLD_CAMPAIGN_SESSION_H

#include "opengold/encounter_data.h"
#include "opengold/gold_box_game.h"
#include "opengold/monster_bundle.h"
#include "opengold/rolf_tour.h"
#include "opengold/wall_art_replacements.h"

#include <filesystem>
#include <memory>

// The one way the game starts a campaign: Pool of Radiance's or another
// Gold Box game's, from that game's folder (docs/GAMES.md).
namespace opengold
{

// OpenGoldBox's own data for a game's campaign (data/): what its creatures
// fight as, new wall art and new monsters. Only Pool of Radiance has any yet.
struct CampaignData
{
    const por::EncounterData *encounters{};
    const por::WallArtReplacements *new_wall_art{};
    std::shared_ptr<const por::MonsterBundle> monsters;
};

// Throws for a game whose campaign cannot be played yet, or for Pool of
// Radiance without its encounter data.
[[nodiscard]] por::RolfTourSession load_campaign(const std::filesystem::path &folder,
                                                 GoldBoxGame game, const CampaignData &data = {});

} // namespace opengold

#endif
