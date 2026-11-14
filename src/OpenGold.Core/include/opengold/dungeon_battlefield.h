#ifndef OPENGOLD_DUNGEON_BATTLEFIELD_H
#define OPENGOLD_DUNGEON_BATTLEFIELD_H
#include "opengold/geo_map.h"
#include "opengold/rules.h"
#include <array>
#include <cstdint>
#include <functional>

namespace opengold::por
{
struct DungeonBattlefield
{
    rules::Battlefield geometry;
    // Zero-based DUNGCOM tile IDs; artwork is loaded from the user's files.
    std::vector<std::uint8_t> tiles;
    // Which sides (north, east, south, west) of the exploration squares the
    // arena shows are solid walls, as [row + 2][column + 6] from the party's
    // square. A doorway is not solid. A crowded group spills into a
    // neighbouring square only through a side that is not; hand-built fields
    // leave every side open, as the original's wilderness does.
    std::array<std::array<std::array<bool, 4>, 13>, 5> walled{};
};

// PC 1.3 dungeon geometry from the current, potentially script-modified GEO.
// Random floor decoration and creature placement are intentionally separate.
[[nodiscard]] DungeonBattlefield dungeon_battlefield(const GeoMap &map, unsigned x, unsigned y);

// What the field makes of an edge between two exploration squares: a wall or
// a doorway on either side wins (their bits 1 and 3).
enum class FieldEdge : unsigned
{
    open = 0,
    wall = 1,
    doorway = 3
};

// Whose tile layout a field follows: the DAX games' or Unlimited Adventures'
// kit's, which draws one corner piece of its own.
enum class FieldLayout : std::uint8_t
{
    dax,
    kit,
};

// The edge on side `direction` (0 north to 3 west) of the square `column`
// and `row` squares from the party's, both squares' sides combined.
using FieldEdges = std::function<FieldEdge(int column, int row, unsigned direction)>;

// Unlimited Adventures' dungeon field (cmbsetup.c, dungeon_map): the same
// 50 x 25 layout of DUNGCOM tiles as the DAX games' from the edges around the
// party's square; no square is open to the sky.
[[nodiscard]] DungeonBattlefield kit_dungeon_battlefield(const FieldEdges &edges);
} // namespace opengold::por
#endif
