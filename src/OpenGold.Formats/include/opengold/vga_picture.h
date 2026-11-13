#ifndef OPENGOLD_VGA_PICTURE_H
#define OPENGOLD_VGA_PICTURE_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// The pictures of the 256-color games, Pools of Darkness and Treasures of the
// Savage Frontier: one byte a pixel, an index into the 256-entry color table,
// and the run of that table each picture sets. The layouts:
// docs/games/darkness.md, "Pictures and graphics".
namespace opengold
{

// Red, green and blue, 0 to 63 each, as the games hand a color to the display.
using VgaColor = std::array<std::uint8_t, 3>;

// The colors a picture sets: entries first, first + 1 and so on of the color
// table. Its pixels may also use entries other pictures set.
struct VgaPaletteRange
{
    std::uint8_t first{};
    std::vector<VgaColor> colors;
};

// Pixels row by row, each an index into the color table.
struct VgaImage
{
    std::uint16_t width{};
    std::uint16_t height{};
    std::vector<std::uint8_t> pixels;
};

// A still picture, or a strip of same-sized frames such as a set of 8x8 tiles.
struct VgaPicture
{
    std::uint16_t x{}; // The position the record gives; most records give 0.
    std::uint16_t y{};
    VgaPaletteRange palette;
    std::vector<VgaImage> frames; // A record that only sets colors has 0 x 0 frames.
};

// A decompressed record of TITLE, BIGPIC, BACK, SKYGRND, CPIC, CHEAD, CBODY,
// COMSPR, DUNGCOM, WILDCOM, 8X8D, BORDERS, CURSOR and the like: the record
// the original's picture loader reads. nullopt when the sizes do not add up.
[[nodiscard]] std::optional<VgaPicture> decode_vga_picture(std::span<const std::uint8_t> record);

struct VgaAnimationFrame
{
    std::uint8_t delay{}; // Tenths of a second on screen; 99 stops the animation there.
    VgaImage image;
};

// A PIC or SPRIT record: up to nine frames of the same size. The original
// shows them in turn and, after the last, goes back to first_frame. A sprite's
// frames are its near, middle and far views, and color 0 is see-through in
// them.
struct VgaAnimation
{
    std::uint16_t x{};
    std::uint16_t y{};
    VgaPaletteRange palette;
    std::uint8_t first_frame{};
    std::vector<VgaAnimationFrame> frames;
};

// A decompressed PIC or SPRIT record, every frame whole. nullopt when a frame
// does not unpack to the picture's size or the sizes do not add up.
[[nodiscard]] std::optional<VgaAnimation>
decode_vga_animation(std::span<const std::uint8_t> record);

} // namespace opengold

#endif
