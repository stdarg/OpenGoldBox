#ifndef OPENGOLD_EXPLORATION_VIEW_H
#define OPENGOLD_EXPLORATION_VIEW_H

#include "opengold/geo_map.h"
#include "opengold/wall_art.h"
#include <bitset>

namespace opengold::por
{
struct ExplorationWallHit
{
    int cell{-1}; // -1 means this pixel contains no wall artwork.
    MapDirection face{MapDirection::north};
    WallView perspective{WallView::far_front};
};

struct ExplorationView
{
    Image image;
    std::bitset<GeoMap::width * GeoMap::height> visible;
    // The last opaque source piece painted at each logical canvas pixel.
    // Transparent cutouts and the background have no wall hit.
    std::array<ExplorationWallHit, 88 * 88> wall_hits{};
};

// Visibility comes from the same clipped, occluded wall pixels and floor
// projection as the image. The caller records it only when showing this view.
[[nodiscard]] ExplorationView render_exploration_view(const GeoMap &map, const WallArtSet &art,
                                                      unsigned x, unsigned y, MapDirection facing);
// Original perspective pieces are composed on an 88x88 logical canvas. The UI
// controls scaling; this function never changes map, door or encounter state.
[[nodiscard]] Image compose_exploration_view(const GeoMap &map, const WallArtSet &art, unsigned x,
                                             unsigned y, MapDirection facing);
} // namespace opengold::por
#endif
