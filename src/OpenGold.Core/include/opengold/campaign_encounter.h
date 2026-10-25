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
// New art drawn as the Combat Sprite Catalog previews it (#274): `scale`
// 128-pixel art squares to a combat square, both poses standing on the row
// `ground` of the ready pose, centered.
struct PlantedArt
{
    double scale{1};
    unsigned ground{};
};

struct CombatArt
{
    rules::EntityId entity{};
    Image image;
    std::optional<Image> action;
    std::string missing_combination;
    std::optional<PlantedArt> planted; // empty for art fitted to the square
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

// A monster a script fields on the party's side (the original record's team
// byte 0x10E: Valhingen Graveyard's efreeti) takes an id from 2000 on. The
// computer fights it, as the original's quick fight did.
inline constexpr rules::EntityId first_ally_id = 2000;

[[nodiscard]] constexpr bool is_ally(rules::EntityId id) noexcept
{
    return id >= first_ally_id;
}

struct CampaignEncounter
{
    por::DungeonBattlefield field;
    std::vector<rules::Participant> enemies;
    std::vector<CombatArt> art;
    std::vector<Image> terrain_art;
    por::MapDirection facing{por::MapDirection::north};
    Surprise surprise{Surprise::none};
    // The original's encounter distance when the fight starts: exploration
    // squares (0 to 2) between the party and the monsters.
    unsigned distance{};
    // The encounter interrupted the party's rest.
    bool party_resting{};
    // Optional authored formation. Empty means the usual campaign placement.
    std::vector<rules::Cell> positions;
    // The original script's encounter morale (100 never breaks).
    unsigned morale{100};
    // A duel (the original's CALL 0x8001): only this member fights; 0 means
    // the whole party.
    rules::EntityId champion{};
    // The fight is by day, 06:00 to 17:59, so squares open to the sky are in
    // sunlight (Sunlight Sensitivity).
    bool daylight{};
    // Monsters on the party's side, standing beside it.
    std::vector<rules::Participant> allies;
};
} // namespace opengold
#endif
