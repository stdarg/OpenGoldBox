#include "opengold/wall_art.h"
#include <algorithm>

namespace opengold::por
{
std::optional<WallTiles> decode_wall_tiles(std::span<const std::uint8_t> record)
{
    // PoR 8X8D: one 17-byte image header, then count packed 8x8 frames.
    if (record.size() < 17 || record[0] != 8 || record[1] != 0 || record[2] != 1 ||
            record[3] != 0 || !record[8] ||
            record.size() != 17 + static_cast<std::size_t>(record[8]) * 32)
        return std::nullopt;
    WallTiles result(record[8]);
    for (std::size_t frame = 0; frame < result.size(); ++frame)
    {
        for (std::size_t pixel = 0; pixel < 64; ++pixel)
        {
            const auto packed = record[17 + frame * 32 + pixel / 2];
            result[frame][pixel] = pixel % 2 ? packed & 15 : packed >> 4;
        }
    }
    return result;
}

std::optional<WallArtSet> decode_wall_art(std::span<const std::uint8_t> definitions,
        std::span<const WallTile> tiles)
{
    constexpr unsigned slice_size = 156;
    if (definitions.empty() || definitions.size() % slice_size ||
            definitions.size() > 15 * slice_size || tiles.empty() || tiles.size() > 256)
        return std::nullopt;
    for (const auto &tile : tiles)
        if (std::any_of(tile.begin(), tile.end(),
                        [](auto color)
    {
        return color > 15;
    }))
    return std::nullopt;
    if (std::any_of(definitions.begin(), definitions.end(),
                    [&](auto index)
{
    return index >= tiles.size();
    }))
    return std::nullopt;

    // Data-layout facts: ten stored perspectives per 156-byte appearance.
    constexpr std::array<unsigned, 10> offsets{0, 2, 6, 10, 22, 38, 54, 110, 132, 154};
    constexpr std::array<unsigned, 10> columns{1, 1, 1, 3, 2, 2, 7, 2, 2, 1};
    constexpr std::array<unsigned, 10> rows{2, 4, 4, 4, 8, 8, 8, 11, 11, 2};
    // Normal EGA palette. Wall index 8 is gray; black (0) is opaque.
    constexpr std::array<std::array<std::uint8_t, 3>, 16> palette{{{0, 0, 0},
            {0, 0, 170},
            {0, 170, 0},
            {0, 170, 170},
            {170, 0, 0},
            {170, 0, 170},
            {170, 85, 0},
            {170, 170, 170},
            {85, 85, 85},
            {85, 85, 255},
            {85, 255, 85},
            {85, 255, 255},
            {255, 85, 85},
            {255, 85, 255},
            {255, 255, 85},
            {255, 255, 255}
        }};
    WallArtSet result;
    result.appearances.resize(definitions.size() / slice_size);
    for (std::size_t appearance = 0; appearance < result.appearances.size(); ++appearance)
    {
        for (unsigned view = 0; view < 10; ++view)
        {
            auto &image = result.appearances[appearance][view];
            image.width = columns[view] * 8;
            image.height = rows[view] * 8;
            image.rgba.resize(image.width * image.height * 4);
            for (unsigned y = 0; y < image.height; ++y)
            {
                for (unsigned x = 0; x < image.width; ++x)
                {
                    const auto index = definitions[appearance * slice_size + offsets[view] +
                                                   (y / 8) * columns[view] + x / 8];
                    const auto color = tiles[index][(y % 8) * 8 + x % 8];
                    const auto output = (y * image.width + x) * 4;
                    std::copy(palette[color].begin(), palette[color].end(),
                              image.rgba.begin() + output);
                    // Blank tile zero and the pink color key reveal the backdrop.
                    image.rgba[output + 3] = index == 0 || color == 13 ? 0 : 255;
                }
            }
        }
    }
    return result;
}
} // namespace opengold::por
