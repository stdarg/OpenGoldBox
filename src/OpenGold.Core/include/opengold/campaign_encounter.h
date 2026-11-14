#ifndef OPENGOLD_CAMPAIGN_ENCOUNTER_H
#define OPENGOLD_CAMPAIGN_ENCOUNTER_H
#include "opengold/difficulty.h"
#include "opengold/dungeon_battlefield.h"
#include "opengold/formats.h"
#include "opengold/rules.h"
#include <cstdint>
#include <map>
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

// A creature's original icon and its action pose.
struct OriginalArt
{
    Image image;
    std::optional<Image> action;
};

// One weapon set's new art, planted as the catalog previews it.
struct WeaponSetArt
{
    Image image, action;
    std::optional<PlantedArt> planted; // empty for art fitted to the square
};

struct CombatArt
{
    rules::EntityId entity{};
    Image image;
    std::optional<Image> action;
    std::string missing_combination;
    std::optional<PlantedArt> planted; // empty for art fitted to the square
    // The original icon new art replaced, for showing that instead (#274).
    std::optional<OriginalArt> original;
    // The weapon sets the creature shows once it attacks in melee or shoots.
    std::optional<WeaponSetArt> melee, ranged;
    // Its melee weapon after it took its shield off to shoot.
    std::optional<WeaponSetArt> melee_without_shield;
    // A party member, by what it has in hand: see in_hand_key.
    std::map<std::string, WeaponSetArt> in_hand;
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

// One creature of the band a script loaded, before the fight was sized to the party.
struct BandMember
{
    rules::Participant creature;
    unsigned record{}, xp{};
};

// The arena duel's copy of the champion.
inline constexpr rules::EntityId mirror_id = 1000;

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
    // The direction from the party each enemy starts in (Unlimited
    // Adventures' starting quarters), one an enemy; empty for all ahead of it.
    std::vector<por::MapDirection> enemy_quarters;
    // The original script's encounter morale (100 never breaks).
    unsigned morale{100};
    // A duel (the original's CALL 0x8001): only this member fights; 0 means
    // the whole party.
    rules::EntityId champion{};
    // The training hall's arena duel (CALL 0x8000): the champion fights a
    // copy of itself, with its gear, as the fight's one enemy (id 1000).
    bool mirror{};
    // The fight is by day, 06:00 to 17:59, so squares open to the sky are in
    // sunlight (Sunlight Sensitivity).
    bool daylight{};
    // The fight is out of doors, on the wilderness or the overland
    // (rules::Encounter::outdoors).
    bool outdoors{};
    // Monsters on the party's side, standing beside it.
    std::vector<rules::Participant> allies;
    // Party members who fight for the monsters, standing with them (the
    // original's hostile members: Tyranthraxus's converts, the graveyard's
    // treacherous mystic).
    std::vector<rules::EntityId> turncoats;
    // The whole band, with its gear, so the AI lab can size the fight for another
    // party and difficulty just as the game does.
    std::vector<BandMember> band;
    // Easy's safety net (DIFF-2).
    bool spares_the_party{};
    // Gateway's maze fights, which one member's escape ends
    // (rules::Encounter::escape_ends_fight).
    bool escape_ends_fight{};
};

// What the difficulty does to a fight's monsters and morale, those on the party's
// side left alone (DIFF-2).
void apply_difficulty(CampaignEncounter &encounter, Difficulty difficulty, unsigned party_level);
} // namespace opengold
#endif
