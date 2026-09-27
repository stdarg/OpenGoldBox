#ifndef OPENGOLD_WALL_ART_H
#define OPENGOLD_WALL_ART_H

#include "opengold/formats.h"
#include <array>
#include <optional>

namespace opengold::por
{
using WallTile = std::array<std::uint8_t, 64>;
using WallTiles = std::vector<WallTile>;
enum class WallView : unsigned
{
    far_front,
    far_left,
    far_right,
    middle_front,
    middle_left,
    middle_right,
    near_front,
    near_left,
    near_right,
    far_extension
};

struct WallArtSet
{
    // GEO appearance ID n selects appearances[n-1]. ID 0 has no artwork.
    std::vector<std::array<Image, 10>> appearances;
};

// Inputs are decompressed DAX records. Resource selection belongs to the host.
[[nodiscard]] std::optional<WallTiles> decode_wall_tiles(std::span<const std::uint8_t> record);
[[nodiscard]] std::optional<WallArtSet> decode_wall_art(std::span<const std::uint8_t> definitions,
        std::span<const WallTile> tiles);
} // namespace opengold::por
#endif
