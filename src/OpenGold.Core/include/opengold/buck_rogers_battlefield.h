#ifndef OPENGOLD_BUCK_ROGERS_BATTLEFIELD_H
#define OPENGOLD_BUCK_ROGERS_BATTLEFIELD_H
#include "opengold/dungeon_battlefield.h"
#include "opengold/geo_map.h"

#include <cstdint>
#include <string_view>

// The Buck Rogers games' ground-combat arena (docs/games/countdown.md,
// "Ground fights in the campaign"): 50 by 25 cells, each holding one of the
// 154 kinds of combat terrain (gdconsts.pas combatTerrain), laid from the
// walls round the team's square indoors (combatst.pas BuildDungeonArena) or
// scattered over the ground of Mars and Venus (BuildWildernessArena), and
// drawn with the tile set the area's kind names.
namespace opengold::por
{
// The tile set an area kind's fights are drawn with (combatst.pas
// LoadCombatTileSet): its file, the slot its first tile goes to, and how
// many tiles the game copies from it.
struct BuckRogersTileSet
{
    std::string_view file;
    unsigned first_slot{}, copied{};
};

inline constexpr std::uint8_t mars_wilderness = 0xA8, venus_wilderness = 0xA2;

[[nodiscard]] BuckRogersTileSet buck_rogers_tile_set(unsigned area_kind) noexcept;

// What a kind of combat terrain is (types.pas TCombatTerrain): the movement
// it costs (255 none can pay), whether it blocks a line of fire (1) or gives
// cover (2), and the tile slot it is drawn from.
struct CombatTerrain
{
    std::uint8_t cost{}, line_class{}, tile{};
};

inline constexpr unsigned combat_terrain_kinds = 154;
[[nodiscard]] CombatTerrain combat_terrain(unsigned kind);

// The rules' ground for a kind of combat terrain (pathing.pas WalkLine): a
// line class 1 kind is a wall; any other kind costing 255 stops feet, not
// shots; line class 2 is cover. What a step onto it costs is its own cost
// (the arena's Battlefield::move_cost), whatever its line class.
[[nodiscard]] rules::Terrain combat_ground(unsigned kind);

// The arena of a fight at the team's square (x, y) of the map, in an area of
// this kind, started by this script (whose number seeds the indoor arena's
// decoration, as the original reseeds its dice with it); out of doors the
// wilderness terrain cell (4BB7h) chooses the ground, and `seed` the dice.
// The field's tiles are frames of the tile set's file; a terrain whose tile
// the file does not hold is counted in `missing_tiles`.
struct BuckRogersBattlefield
{
    DungeonBattlefield field;
    std::vector<std::uint8_t> terrain;
    unsigned missing_tiles{};
};

[[nodiscard]] BuckRogersBattlefield
buck_rogers_battlefield(const GeoMap &map, unsigned x, unsigned y, unsigned area_kind,
                        unsigned script, unsigned wilderness_terrain, std::uint32_t seed,
                        unsigned tiles_in_file);
} // namespace opengold::por

#endif
