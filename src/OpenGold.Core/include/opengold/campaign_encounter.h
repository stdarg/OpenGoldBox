#ifndef OPENGOLD_CAMPAIGN_ENCOUNTER_H
#define OPENGOLD_CAMPAIGN_ENCOUNTER_H
#include "opengold/dungeon_battlefield.h"
#include "opengold/formats.h"
#include "opengold/rules.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// What exploration hands to combat: the field, the enemies and their art. Kept
// apart from the combat demo so exploration need not compile it (Effective C++
// Item 31).
namespace opengold
{
struct CombatArt
{
    rules::EntityId entity{};
    Image image;
    std::optional<Image> action;
    std::string missing_combination;
};

// Who an encounter caught unaware. The values are the original script's
// surprise bits (1 the party, 2 the monsters), so a script value converts
// directly and a swapped neighbouring number no longer compiles (Effective C++
// Item 18).
enum class Surprise : std::uint8_t
{
    none,
    party,
    monsters,
    both
};

struct CampaignEncounter
{
    por::DungeonBattlefield field;
    std::vector<rules::Participant> enemies;
    std::vector<CombatArt> art;
    std::vector<Image> terrain_art;
    por::MapDirection facing{por::MapDirection::north};
    Surprise surprise{Surprise::none};
    // The encounter interrupted the party's rest.
    bool party_resting{};
    // Optional authored formation. Empty means the usual campaign placement.
    std::vector<rules::Cell> positions;
    // The original script's encounter morale (100 never breaks).
    unsigned morale{100};
};
} // namespace opengold
#endif
