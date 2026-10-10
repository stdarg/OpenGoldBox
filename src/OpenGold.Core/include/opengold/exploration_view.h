#ifndef OPENGOLD_EXPLORATION_VIEW_H
#define OPENGOLD_EXPLORATION_VIEW_H

#include "opengold/geo_map.h"
#include "opengold/wall_art.h"
#include <bitset>

namespace opengold::por
{
struct ExplorationView
{
    Image image;
    std::bitset<GeoMap::width * GeoMap::height> visible;
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
