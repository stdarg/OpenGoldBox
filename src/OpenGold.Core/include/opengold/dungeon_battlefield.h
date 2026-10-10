#ifndef OPENGOLD_DUNGEON_BATTLEFIELD_H
#define OPENGOLD_DUNGEON_BATTLEFIELD_H
#include "opengold/geo_map.h"
#include "opengold/rules.h"
#include <array>

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
} // namespace opengold::por
#endif
