#ifndef OPENGOLD_PHLAN_RESOURCES_H
#define OPENGOLD_PHLAN_RESOURCES_H

// What a town session reads of the original files (RolfTourSession::load):
// the town and its districts, and the creatures their scripts stage.

#include "opengold/buck_rogers_records.h"
#include "opengold/campaign_encounter.h"
#include "opengold/character.h"
#include "opengold/creature_catalog.h"
#include "opengold/difficulty.h"
#include "opengold/ecl_machine.h"
#include "opengold/encounter_data.h"
#include "opengold/formats.h"
#include "opengold/map_catalog.h"
#include "opengold/monster_bundle.h"
#include "opengold/overland.h"
#include "opengold/vga_picture.h"
#include "opengold/wall_art.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
// The ship fight's files (buck_rogers_ship_fight.h).
struct ShipFightFiles;

// Gateway's set of marks (8X8D1's record 202, its symbol set 4): the party's
// arrows on the overland, by facing, and the automap's walls.
inline constexpr unsigned gateway_marks = 202;

struct DarkQueenResources;

struct PhlanResources
{
    std::shared_ptr<const DarkQueenResources> dark_queen;
    // A campaign that starts by running a script's load entry (Curse's script
    // 1 of disk 2) rather than Rolf's tour of New Phlan.
    std::optional<unsigned> opening_script;
    // The walls a campaign's LOAD PIECES may load, by disk and the three banks
    // (Curse's; Pool of Radiance's areas list their walls with their maps).
    std::map<std::pair<unsigned, std::array<unsigned, 3>>, WallArtSet> wall_sets;
    std::map<unsigned, std::shared_ptr<const EclProgram>> programs;
    // Each area script's archive bank, including a script with no map of its
    // own (the dragon's cave).
    std::map<unsigned, unsigned> script_banks;
    std::map<unsigned, std::vector<Equipment>> treasure;
    // The 256-color engine's item definitions (ITEM0), by number from 1, which
    // its TREASURE and STORE name; and each of its stores' definitions, as
    // its program lists them (docs/games/darkness.md, Shops and treasure).
    std::map<unsigned, Equipment> item_definitions;
    std::vector<std::vector<std::uint8_t>> stores;

    // On the town only: Treasures of the Savage Frontier's overland maps'
    // pictures (OVER), by map, each with the color cycle it turns; the color
    // tables of the records that only set colors, by record; and the party's
    // arrow on them, by facing.
    struct OverlandPicture
    {
        VgaImage pixels;
        std::optional<VgaColorCycle> cycle;
    };

    std::map<unsigned, OverlandPicture> overland_pictures;
    std::map<unsigned, VgaColorTable> overland_colors;
    std::array<Image, 4> overland_arrows;
    // Pools of Darkness's marker of the party's square on its area map
    // (CURSOR record 1, drawn through color 13).
    std::optional<Image> area_map_marker;
    // Treasures' LUCK texts by record: the luck readings (0 to 14), the lucky
    // papers' words and the romances' scenes.
    std::map<unsigned, std::string> luck_texts;
    // Treasures' PIC records as they decode, and the color runs of its PIC
    // records that only set colors (PIC1 38, PIC1A 78 and the like), by record:
    // the picture cell 4Eh names is shown in the colors of the record cell
    // 4Fh names (its portrait.pas, LoadAnimation).
    std::map<unsigned, VgaAnimation> pic_records;
    std::map<unsigned, VgaPaletteRange> picture_colors;
    // ITEMS templates, indexed by original item type, for generated treasure.
    std::vector<ItemTemplate> item_templates;
    std::vector<std::uint8_t> sprite_archive;
    // Native cross-disk sprites, decoded once on the root resources.
    std::map<unsigned, std::array<Image, 3>> shared_sprites;
    std::map<unsigned, Image> heads, bodies, pictures;
    // PIC records: monster close-ups shown before combat, looping until a key press.
    std::map<unsigned, std::vector<AnimationFrame>> animations;
    // Explicit converted NPC profiles scoped to this resource bank. No guessed ID conversion.
    std::map<unsigned, opengold::Character> npc_profiles;
    // Immutable district profiles. Root owns districts; children never own root.
    std::map<unsigned, std::shared_ptr<const PhlanResources>> districts;
    std::optional<GeoMap> map;
    WallArtSet wall_art;
    std::map<unsigned, Creature> encounter_creatures;
    // On the town only: every creature record of every MONnCHA bank, from
    // which an NPC who joins under rules that play the original's records is
    // made.
    std::shared_ptr<const CreatureCatalog> creatures;
    std::map<unsigned, EncounterConversion> conversions;
    // A district's script, archive bank and LOAD PIECES wall banks; zero for the town.
    unsigned script{}, bank{};
    // Other scripts that load this map: Valjevo Castle's share its quarters.
    std::vector<unsigned> shared_with;

    [[nodiscard]] bool loaded_by(unsigned id) const
    {
        return id == script ||
               std::find(shared_with.begin(), shared_with.end(), id) != shared_with.end();
    }

    std::array<unsigned, 3> pieces{};
    // On the town only: Gateway's sets of eight-by-eight symbols, by record,
    // that its LOAD PIECES loads alone on the overland, outside a dungeon
    // (8X8D6's sets of 70, with the files' own for the disks of the scripts
    // that load maps by cells).
    std::map<unsigned, WallTiles> symbol_sets;
    std::vector<std::uint8_t> combat_archive;
    std::vector<Image> terrain_art;
    // On the town only: the wilderness from START.EXE, SQRPACI's 256 overland
    // tiles, the party's overland icon and the wilderness battlefield art
    // (WILDCOM, then RANDCOM). Null without START.EXE.
    std::shared_ptr<const OverlandWorld> overland;
    std::vector<Image> overland_tiles;
    // The same tiles once the Pyramid's pollution is stopped and the valley
    // heals (the original's Yarash ending).
    std::vector<Image> healed_overland_tiles;
    Image party_icon;
    std::vector<Image> wilderness_art;
    // Curse's: the battlefield tiles and places' terrain its fights out of
    // doors are built from (curse_wilderness); empty for Pool of Radiance.
    std::optional<CurseWilderness> curse_wilderness;
    // Gateway's: the terrains and grounds its fights on the overland are
    // built from (gateway_wilderness).
    std::optional<GatewayWilderness> gateway_wilderness;
    // On the town only: the original's closing story from GAME.OVR, one string
    // per page. Empty without that file.
    std::vector<std::string> closing_story;
    // The Combat Sprite Catalog's export (#274), on the town only: creatures'
    // names, gear and new art. Null without one.
    std::shared_ptr<const MonsterBundle> monsters;
    // On the town only: what scripts' LOAD MONSTER instructions load instead
    // of the original's (data/encounters/overrides.json), checked against them.
    std::map<EncounterSite, EncounterOverride> encounter_overrides;

    // On the town only: the Buck Rogers games' creatures as LOAD MONSTER and
    // ADD NPC take them (MON0CHA's records with MON0ITM's items of the same
    // number), the item templates TREASURE and SHOP copy (ITEM0, template n
    // at [n - 1]) and ITEMS' rows (docs/games/countdown.md, "The census").
    struct BuckRogersCreature
    {
        buck_rogers::Creature creature;
        std::vector<buck_rogers::Item> items;
    };

    std::map<unsigned, BuckRogersCreature> buck_rogers_creatures;
    std::vector<buck_rogers::Item> buck_rogers_templates;
    std::vector<buck_rogers::ItemType> buck_rogers_item_types;
    // Their ground-combat tile sets (HITEK, LOTEK, MARSCOM, VENUSCOM), by file,
    // each tile in the colors its picture sets.
    std::map<std::string, std::vector<Image>, std::less<>> buck_rogers_tile_sets;
    // Each disk's combat icons (CPICn), which LOAD MONSTER draws from by
    // the disk its script is on (maptiles.pas LoadIconAnim).
    std::map<unsigned, std::vector<std::uint8_t>> buck_rogers_icons;
    // The team's combat icons (CHARS), by the record's icon byte.
    std::vector<std::uint8_t> buck_rogers_team_icons;
    // The team's ship as a new game starts with it (SHIPS record 50).
    std::vector<std::uint8_t> buck_rogers_party_ship;
    // The enemy ships and the pictures of the ship fight (SHIPS.DAX).
    std::shared_ptr<const ShipFightFiles> buck_rogers_ship_fight_files;
    // The words their items are named with (START.EXE).
    std::vector<std::string> buck_rogers_item_words;
    // Their solar map's art: its backdrop (SHIPS picture 128, 176 pixels
    // square, 8 to a cell) and its tiles (8X8D0 record 1: the Sun, the bodies
    // at each place on their orbits and the rocket at each heading), each in
    // the colors its picture sets, the tiles' key color clear.
    Image solar_map_backdrop;
    std::vector<Image> solar_map_tiles;
};

// One creature an area's script loads (LOAD MONSTER), as the game stages it
// for a fight: its converted fighter and its combat art. The game's scripts
// and the developer tools that stage fights use this one way.
struct StagedCreature
{
    rules::Participant fighter;
    CombatArt art;
};

// Whether the area's creature `record` fights on the party's side: record byte
// 0x10E (coab combat_team) is 0, as only Valhingen Graveyard's efreeti's is.
[[nodiscard]] bool fights_for_party(const PhlanResources &area, unsigned record);

// The name the creature shows: its export's, or its original record's.
[[nodiscard]] std::string creature_name(const PhlanResources &area, unsigned record,
                                        const ExportedMonster *exported);

// The area's creature `record`, drawn with combat icon `icon`, as fighter `id`
// named `name` on `side`. `exported` is the creature's Combat Sprite Catalog
// export, or null. Under rules that play the original's records
// (`original_record`) it fights as its record; otherwise as the area's
// conversion of it. The area's icons are in the format of `game`. Throws when
// the area converts no such record or lacks the icon.
[[nodiscard]] StagedCreature
stage_area_creature(const PhlanResources &area, unsigned record, unsigned icon,
                    const ExportedMonster *exported, rules::EntityId id, std::string name,
                    rules::Side side, Difficulty difficulty, bool original_record,
                    GoldBoxGame game = GoldBoxGame::pool_of_radiance,
                    std::optional<std::array<VgaColor, 256>> native_palette = {});
} // namespace opengold::por
#endif
