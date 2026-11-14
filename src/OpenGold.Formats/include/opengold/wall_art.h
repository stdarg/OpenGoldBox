#ifndef OPENGOLD_WALL_ART_H
#define OPENGOLD_WALL_ART_H

#include "opengold/formats.h"
#include "opengold/vga_picture.h"
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
    // Pixels per original pixel: 1 for decoded art, more for new art.
    unsigned scale{1};
    // For each appearance new art replaced, its original pieces at the same
    // scale, so the view can show the original instead (#274); empty images
    // for the others.
    std::vector<std::array<Image, 10>> originals;
    // Pools of Darkness draws a picture behind the walls (its DrawBackdrop):
    // the wall set's own where the map marks the square as indoors, the sky
    // elsewhere. The other games' views are flat colors, and have none.
    std::optional<Image> indoor_backdrop, sky_backdrop;
};

// The size in pixels the original draws a view's piece at, for every appearance.
struct WallViewSize
{
    unsigned width{}, height{};
};

[[nodiscard]] WallViewSize wall_view_size(WallView view);

// Inputs are decompressed DAX records. Resource selection belongs to the host.
[[nodiscard]] std::optional<WallTiles> decode_wall_tiles(std::span<const std::uint8_t> record);
[[nodiscard]] std::optional<WallArtSet> decode_wall_art(std::span<const std::uint8_t> definitions,
                                                        std::span<const WallTile> tiles);

// The 256-color games' walls (Pools of Darkness, Treasures): every appearance's
// ten views drawn from one picture of 8x8 tiles, the 8X8D1.DAX record with the
// WALLDEF record's id, whose frame n is symbol n. The original never draws
// symbol 0; it comes out in color 13, the color those games draw tiles through.
using VgaWallViews = std::array<VgaImage, 10>;
[[nodiscard]] std::optional<std::vector<VgaWallViews>>
decode_vga_wall_art(std::span<const std::uint8_t> definitions, const VgaPicture &tiles);
} // namespace opengold::por
#endif
