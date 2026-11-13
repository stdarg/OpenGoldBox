#ifndef OPENGOLD_GATEWAY_FORMATS_H
#define OPENGOLD_GATEWAY_FORMATS_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "opengold/formats.h"

// The file formats Gateway to the Savage Frontier introduced: pictures of 32
// colors with a color header, and digitized sound effects (ZOUNDS.DAX). The
// layouts: docs/games/gateway.md.
namespace opengold::gateway
{

// A picture's color header: which maps follow, how its pixels are stored, and
// its own colors 16 to 31. A record with the older header (eight bytes of a CGA
// map) reads as packed pixels with the identity EGA map.
struct PictureColors
{
    static constexpr std::uint8_t cga_map_flag = 0x01, ega_map_flag = 0x02, five_planes_flag = 0x04,
                                  palette_flag = 0x08;

    std::uint8_t flags{};
    // The EGA color the original's EGA mode shows for each of the 32 colors.
    std::array<std::uint8_t, 32> ega_map{};
    // Colors 16 to 31 as 4-bit red, green and blue levels: the low nibbles of
    // the 24 bytes hold colors 16 to 23, the high nibbles colors 24 to 31.
    std::array<std::uint8_t, 24> palette{};

    [[nodiscard]] bool five_planes() const noexcept
    {
        return (flags & five_planes_flag) != 0;
    }

    [[nodiscard]] bool has_palette() const noexcept
    {
        return (flags & palette_flag) != 0;
    }
};

// A picture's frames as color numbers, 0 to 31, a byte a pixel, row by row.
struct Picture
{
    std::uint16_t width{}, height{}; // In pixels.
    std::uint16_t origin_x{}, origin_y{};
    PictureColors colors;
    std::vector<std::vector<std::uint8_t>> frames;
};

// A record as the original's LoadPic reads it: a nine-byte header (height,
// width in 8-pixel columns, origin, frame count), the color header, then each
// frame's rows; a picture of no rows or columns has no frames, only colors.
// nullopt for any other bytes.
[[nodiscard]] std::optional<Picture> decode_picture(std::span<const std::uint8_t> record);

struct AnimationFrame
{
    std::uint32_t delay{}; // Tenths of a second.
    Picture picture;       // One frame.
};

// A PIC record: a frame count, then for each frame its delay and a picture.
// Frames after the first hold the difference from the first, color by color;
// a later frame whose header says only "five planes" takes the first frame's
// colors. nullopt for any other bytes.
[[nodiscard]] std::optional<std::vector<AnimationFrame>>
decode_animation(std::span<const std::uint8_t> record);

// The frame in the colors of the original's VGA mode: the EGA colors, its two
// fixed colors 16 and 17, and the picture's own colors 18 to 31 (or, for a
// picture with no colors of its own, the EGA map's).
[[nodiscard]] Image picture_image(const Picture &picture, std::size_t frame = 0);

// The EGA color, 0 to 15, the original's EGA mode shows for a picture's color.
[[nodiscard]] std::uint8_t ega_color(const PictureColors &colors, std::uint8_t color) noexcept;

// A sound effect of ZOUNDS.DAX: unsigned 8-bit samples, played at its rate.
struct SoundSample
{
    unsigned id{};
    unsigned rate{}; // Samples a second.
    std::vector<std::uint8_t> samples;
};

// Record id of ZOUNDS.DAX; nullopt for an effect the game never loads or a
// record too large for its 9000-byte sound buffer.
[[nodiscard]] std::optional<SoundSample> decode_sound_sample(unsigned id,
                                                             std::span<const std::uint8_t> record);

// Record id of Pools of Darkness' or Treasures of the Savage Frontier's
// ZOUNDS.DAX: the games load records 1 to 11 (sound n plays record n - 1). An
// Amiga sound file's record gives its samples at the rate its header names;
// any other plays whole at 9000 samples a second (docs/games/later-sound.md).
// nullopt for any other record, one too large for the sound buffer, or a
// sound file that does not parse.
[[nodiscard]] std::optional<SoundSample>
decode_later_sound_sample(unsigned id, std::span<const std::uint8_t> record);

} // namespace opengold::gateway

#endif
