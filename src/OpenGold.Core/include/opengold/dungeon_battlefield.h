#ifndef OPENGOLD_DUNGEON_BATTLEFIELD_H
#define OPENGOLD_DUNGEON_BATTLEFIELD_H
#include "opengold/geo_map.h"
#include "opengold/rules.h"

namespace opengold::por
{
struct DungeonBattlefield
{
    rules::Battlefield geometry;
    // Zero-based DUNGCOM tile IDs; artwork is loaded from the user's files.
    std::vector<std::uint8_t> tiles;
};

// PC 1.3 dungeon geometry from the current, potentially script-modified GEO.
// Random floor decoration and creature placement are intentionally separate.
[[nodiscard]] DungeonBattlefield dungeon_battlefield(const GeoMap &map, unsigned x, unsigned y);
} // namespace opengold::por
#endif
