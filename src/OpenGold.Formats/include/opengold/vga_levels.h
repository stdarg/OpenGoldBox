#ifndef OPENGOLD_VGA_LEVELS_H
#define OPENGOLD_VGA_LEVELS_H
#include <cstdint>

// The VGA's colour levels, 0 to 63 a channel, and the 8-bit values the game
// draws them with. Every decoder widens and narrows through these two so one
// level is one colour whichever game's picture it comes from (docs/TECH.md,
// "VGA colour levels").
namespace opengold::format
{
// A 6-bit level as the VGA's digital-to-analog converter puts it out, scaled
// over the whole 8-bit range: 0 stays 0 and 63 becomes 255.
[[nodiscard]] constexpr std::uint8_t widen_vga_level(std::uint8_t level) noexcept
{
    return static_cast<std::uint8_t>((level & 63) * 255 / 63);
}

// The level an 8-bit colour came from: the top six bits. They give back
// every widened level, and also the level under the UA kit's own encoding of
// its palette files (level * 4 + 3, 0 for 0), which rounding value * 63 / 255
// would misread (its level 1, written as 7, would read as 2).
[[nodiscard]] constexpr std::uint8_t narrow_vga_level(std::uint8_t value) noexcept
{
    return static_cast<std::uint8_t>(value >> 2);
}
} // namespace opengold::format
#endif
