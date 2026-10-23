#ifndef OPENGOLD_TEST_PNG_SIZE_ONLY_H
#define OPENGOLD_TEST_PNG_SIZE_ONLY_H
#include "opengold/formats.h"
#include <cstdint>
#include <optional>
#include <span>

namespace opengold::test
{
// The game decodes PNGs through Godot. Here an opaque image of the PNG
// header's size stands in, which is all the export's rules depend on.
inline std::optional<Image> png_size_only(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 24 || bytes[1] != 'P' || bytes[2] != 'N' || bytes[3] != 'G')
    {
        return std::nullopt;
    }
    const auto word = [&](std::size_t at)
    {
        return static_cast<unsigned>(bytes[at] << 24 | bytes[at + 1] << 16 | bytes[at + 2] << 8 |
                                     bytes[at + 3]);
    };
    Image image;
    image.width = static_cast<std::uint16_t>(word(16));
    image.height = static_cast<std::uint16_t>(word(20));
    image.rgba.assign(std::size_t{image.width} * image.height * 4, 255);
    return image;
}
} // namespace opengold::test
#endif
