#include "opengold/vga_picture.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold
{
namespace
{

using format::read_u16;

constexpr unsigned table_size = 256;

// The original unpacks every animation frame into one buffer of 88 rows of
// 88 pixels, so no frame is larger.
constexpr std::size_t largest_animation_frame = 88 * 88;

// The original keeps at most nine frames of an animation.
constexpr unsigned most_animation_frames = 9;

// Colors at offset: count of them, three bytes each. Each color is followed,
// after all of them, by the EGA color the game draws it with on an EGA card,
// two to a byte; the VGA picture does not need those.
struct PaletteRead
{
    VgaPaletteRange palette;
    std::size_t end{}; // The offset after the EGA colors.
};

std::optional<PaletteRead> read_palette(std::span<const std::uint8_t> record, std::size_t offset,
                                        unsigned first, unsigned count)
{
    if (first + count > table_size)
    {
        return std::nullopt;
    }
    const std::size_t end = offset + std::size_t{count} * 3 + count / 2;
    if (end > record.size())
    {
        return std::nullopt;
    }

    PaletteRead read;
    read.palette.first = static_cast<std::uint8_t>(first);
    read.palette.colors.resize(count);
    for (std::size_t color = 0; color < count; ++color)
    {
        std::copy_n(record.begin() + static_cast<std::ptrdiff_t>(offset + color * 3), 3,
                    read.palette.colors[color].begin());
    }
    read.end = end;
    return read;
}

VgaImage image_from(std::span<const std::uint8_t> pixels, unsigned width, unsigned height)
{
    VgaImage image;
    image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    image.pixels.assign(pixels.begin(), pixels.end());
    return image;
}

// An animation frame's packing: a byte up to 7Fh copies that many plus one
// bytes as they are; a larger byte n repeats the next byte 257 - n times. The
// frame must fill exactly `size` bytes and use all of `packed`.
std::optional<std::vector<std::uint8_t>> unpack_frame(std::span<const std::uint8_t> packed,
                                                      std::size_t size)
{
    std::vector<std::uint8_t> frame;
    frame.reserve(size);
    std::size_t at = 0;
    while (at < packed.size())
    {
        const unsigned code = packed[at];
        if (packed.size() - at < 2)
        {
            return std::nullopt;
        }
        if (code <= 0x7f)
        {
            const std::size_t count = code + 1;
            if (count > packed.size() - at - 1 || count > size - frame.size())
            {
                return std::nullopt;
            }
            const auto copied = packed.subspan(at + 1, count);
            frame.insert(frame.end(), copied.begin(), copied.end());
            at += 1 + count;
        }
        else
        {
            const std::size_t count = 257 - code;
            if (count > size - frame.size())
            {
                return std::nullopt;
            }
            frame.insert(frame.end(), count, packed[at + 1]);
            at += 2;
        }
    }
    if (frame.size() != size)
    {
        return std::nullopt;
    }
    return frame;
}

} // namespace

std::optional<VgaPicture> decode_vga_picture(std::span<const std::uint8_t> record)
{
    // Rows, width in 8-pixel cells, x, y, frame count, first color, color count - 1.
    constexpr std::size_t header_size = 10;
    if (record.size() < header_size)
    {
        return std::nullopt;
    }
    const unsigned height = record[0];
    const unsigned width = record[1] * 8u;
    const unsigned frame_count = read_u16(record, 6);
    const unsigned first = record[8];
    const unsigned color_count = record[9] + 1u;
    auto palette = read_palette(record, header_size, first, color_count);
    if (!palette || frame_count == 0)
    {
        return std::nullopt;
    }
    // Four bytes follow the EGA colors (Treasures keeps a color cycle there),
    // and 28 more after the 8x8 tile sets' 112 colors from entry 32.
    const bool tile_colors = first == 32 && color_count == 112;
    const std::size_t pixel_start = palette->end + (tile_colors ? 32 : 4);
    const std::size_t frame_size = std::size_t{width} * height;
    if (record.size() < pixel_start || record.size() - pixel_start != frame_size * frame_count)
    {
        return std::nullopt;
    }

    VgaPicture picture;
    picture.x = read_u16(record, 2);
    picture.y = read_u16(record, 4);
    picture.palette = std::move(palette->palette);
    picture.frames.reserve(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame)
    {
        picture.frames.push_back(image_from(
            record.subspan(pixel_start + frame * frame_size, frame_size), width, height));
    }
    return picture;
}

std::optional<VgaAnimation> decode_vga_animation(std::span<const std::uint8_t> record)
{
    // Rows, width in 8-pixel cells, x and y as words; the last frame's number,
    // the first color and the color count as bytes.
    constexpr std::size_t header_size = 11;
    if (record.size() < header_size)
    {
        return std::nullopt;
    }
    const unsigned height = read_u16(record, 0);
    const unsigned width = read_u16(record, 2) * 8u;
    const unsigned frame_count = record[8] + 1u;
    auto palette = read_palette(record, header_size, record[9], record[10]);
    const std::size_t frame_size = std::size_t{width} * height;
    if (!palette || frame_count > most_animation_frames || frame_size == 0 ||
        frame_size > largest_animation_frame)
    {
        return std::nullopt;
    }
    // Four bytes as in a still picture, then the key frame, the frame the
    // animation loops back to, and a packed size and a delay for each frame.
    const std::size_t table_start = palette->end + 4 + 2;
    std::size_t at = table_start + std::size_t{frame_count} * 3;
    if (at > record.size())
    {
        return std::nullopt;
    }
    const unsigned key_frame = record[table_start - 2];
    VgaAnimation animation;
    animation.x = read_u16(record, 4);
    animation.y = read_u16(record, 6);
    animation.palette = std::move(palette->palette);
    animation.first_frame = record[table_start - 1];
    if (key_frame >= frame_count || animation.first_frame >= frame_count)
    {
        return std::nullopt;
    }

    std::vector<std::vector<std::uint8_t>> frames;
    for (unsigned frame = 0; frame < frame_count; ++frame)
    {
        const std::size_t packed_size = read_u16(record, table_start + frame * 3);
        if (packed_size > record.size() - at)
        {
            return std::nullopt;
        }
        auto pixels = unpack_frame(record.subspan(at, packed_size), frame_size);
        if (!pixels)
        {
            return std::nullopt;
        }
        frames.push_back(std::move(*pixels));
        at += packed_size;
    }
    if (at != record.size())
    {
        return std::nullopt;
    }

    // Every frame but the key frame holds its difference from the key frame.
    for (unsigned frame = 0; frame < frame_count; ++frame)
    {
        if (frame != key_frame)
        {
            std::transform(frames[frame].begin(), frames[frame].end(), frames[key_frame].begin(),
                           frames[frame].begin(),
                           [](std::uint8_t difference, std::uint8_t key)
                           {
                               return static_cast<std::uint8_t>(difference ^ key);
                           });
        }
        animation.frames.push_back(
            {record[table_start + frame * 3 + 2], image_from(frames[frame], width, height)});
    }
    return animation;
}

} // namespace opengold
