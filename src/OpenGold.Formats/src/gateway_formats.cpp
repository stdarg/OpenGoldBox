#include "opengold/gateway_formats.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::gateway
{
namespace
{

constexpr std::size_t picture_header_size = 9;

// The older header's first byte has none of these bits; the newer header's
// flags byte always has one.
constexpr std::uint8_t newer_header_bits = 0xCC;

// Colors 16 and 17 of the VGA mode, which no picture changes, as 6-bit levels.
constexpr std::array<std::array<std::uint8_t, 3>, 2> fixed_vga_levels{
    {{0x3C, 0x30, 0x10}, {0x3C, 0x24, 0x10}}};

// The rate of each sound effect, by its number; 0 for one the game never loads
// (it plays 3 and 6 as 9).
constexpr std::array<unsigned, 13> sample_rates{0,    0,    4000, 0,    7075, 9000, 0,
                                                8200, 8000, 8101, 8107, 7546, 7075};
constexpr std::size_t sound_buffer_size = 9000;

std::uint8_t level_to_rgb(unsigned six_bit_level)
{
    return static_cast<std::uint8_t>(six_bit_level * 255 / 63);
}

// Reads the color header at offset, advancing offset past it; nullopt when the
// bytes run out.
std::optional<PictureColors> read_colors(std::span<const std::uint8_t> record, std::size_t &offset)
{
    if (offset >= record.size())
    {
        return std::nullopt;
    }
    PictureColors colors;
    for (std::size_t color = 0; color < colors.ega_map.size(); ++color)
    {
        colors.ega_map[color] = static_cast<std::uint8_t>(color % 16);
    }
    if ((record[offset] & newer_header_bits) == 0)
    {
        // Eight bytes of a CGA map, which only the CGA mode reads.
        constexpr std::size_t cga_map_size = 8;
        if (record.size() - offset < cga_map_size)
        {
            return std::nullopt;
        }
        colors.flags = PictureColors::cga_map_flag | PictureColors::ega_map_flag;
        offset += cga_map_size;
        return colors;
    }
    colors.flags = record[offset++];
    const auto take = [&](std::size_t size) -> std::optional<std::span<const std::uint8_t>>
    {
        if (record.size() - offset < size)
        {
            return std::nullopt;
        }
        const auto bytes = record.subspan(offset, size);
        offset += size;
        return bytes;
    };
    if ((colors.flags & PictureColors::cga_map_flag) != 0 && !take(8))
    {
        return std::nullopt;
    }
    if ((colors.flags & PictureColors::ega_map_flag) != 0)
    {
        const auto packed = take(16);
        if (!packed)
        {
            return std::nullopt;
        }
        for (std::size_t color = 0; color < colors.ega_map.size(); ++color)
        {
            colors.ega_map[color] = ((*packed)[color / 2] >> ((color % 2) * 4)) & 0x0F;
        }
    }
    if (colors.has_palette())
    {
        const auto palette = take(colors.palette.size());
        if (!palette)
        {
            return std::nullopt;
        }
        std::copy(palette->begin(), palette->end(), colors.palette.begin());
    }
    return colors;
}

// One row's pixels: packed two to a byte, high nibble first, or five planes of
// a bit a pixel, the first plane the color's lowest bit.
void read_row(std::span<const std::uint8_t> bytes, std::uint16_t columns, bool five_planes,
              std::uint8_t *pixels)
{
    const std::size_t width = std::size_t{columns} * 8;
    for (std::size_t x = 0; x < width; ++x)
    {
        if (!five_planes)
        {
            const auto packed = bytes[x / 2];
            pixels[x] = x % 2 == 0 ? packed >> 4 : packed & 0x0F;
            continue;
        }
        const auto bit = static_cast<std::uint8_t>(0x80 >> (x % 8));
        std::uint8_t color = 0;
        for (std::size_t plane = 0; plane < 5; ++plane)
        {
            if ((bytes[plane * columns + x / 8] & bit) != 0)
            {
                color |= static_cast<std::uint8_t>(1U << plane);
            }
        }
        pixels[x] = color;
    }
}

std::size_t row_size(std::uint16_t columns, bool five_planes)
{
    return std::size_t{columns} * (five_planes ? 5 : 4);
}

// Reads count frames of rows at offset, advancing offset past them.
std::optional<std::vector<std::vector<std::uint8_t>>>
read_frames(std::span<const std::uint8_t> record, std::size_t &offset, std::uint16_t height,
            std::uint16_t columns, std::size_t count, bool five_planes)
{
    const auto row_bytes = row_size(columns, five_planes);
    if ((record.size() - offset) / row_bytes / height < count)
    {
        return std::nullopt;
    }
    std::vector<std::vector<std::uint8_t>> frames(count);
    for (auto &frame : frames)
    {
        frame.resize(std::size_t{height} * columns * 8);
        for (std::size_t row = 0; row < height; ++row)
        {
            read_row(record.subspan(offset, row_bytes), columns, five_planes,
                     frame.data() + row * columns * 8);
            offset += row_bytes;
        }
    }
    return frames;
}

struct PictureHeader
{
    std::uint16_t height{}, columns{}, origin_x{}, origin_y{};
    std::uint8_t count{};
};

std::optional<PictureHeader> read_header(std::span<const std::uint8_t> record, std::size_t offset)
{
    if (record.size() - offset < picture_header_size)
    {
        return std::nullopt;
    }
    PictureHeader header{format::read_u16(record, offset), format::read_u16(record, offset + 2),
                         format::read_u16(record, offset + 4), format::read_u16(record, offset + 6),
                         record[offset + 8]};
    // The screen is 320 by 200 pixels.
    if (header.height > 200 || header.columns > 40)
    {
        return std::nullopt;
    }
    return header;
}

bool has_pixels(const PictureHeader &header)
{
    return header.height != 0 && header.columns != 0 && header.count != 0;
}

std::array<std::uint8_t, 3> vga_rgb(const PictureColors &colors, std::uint8_t color)
{
    // The VGA mode shows colors 0 to 15 unmapped.
    if (color < 16)
    {
        return ega_rgb(color);
    }
    if (color >= 18 && !colors.has_palette())
    {
        return ega_rgb(ega_color(colors, color));
    }
    if (color < 18)
    {
        const auto &levels = fixed_vga_levels[color - 16];
        return {level_to_rgb(levels[0]), level_to_rgb(levels[1]), level_to_rgb(levels[2])};
    }
    // A 4-bit level is a 6-bit DAC level divided by four.
    const auto first = static_cast<std::size_t>(color < 24 ? color - 16 : color - 24) * 3;
    const auto level = [&](std::size_t channel)
    {
        const auto packed = colors.palette[first + channel];
        return level_to_rgb((color < 24 ? packed & 0x0F : packed >> 4) * 4U);
    };
    return {level(0), level(1), level(2)};
}

} // namespace

std::optional<Picture> decode_picture(std::span<const std::uint8_t> record)
{
    const auto header = read_header(record, 0);
    if (!header)
    {
        return std::nullopt;
    }
    std::size_t offset = picture_header_size;
    const auto colors = read_colors(record, offset);
    if (!colors)
    {
        return std::nullopt;
    }
    // A record without pixels carries only its colors, as COMSPR's 254 and 255
    // do for the view's and the combat's frames.
    auto frames = has_pixels(*header) ? read_frames(record, offset, header->height, header->columns,
                                                    header->count, colors->five_planes())
                                      : std::vector<std::vector<std::uint8_t>>{};
    if (!frames || offset != record.size())
    {
        return std::nullopt;
    }
    return Picture{static_cast<std::uint16_t>(header->columns * 8),
                   header->height,
                   header->origin_x,
                   header->origin_y,
                   *colors,
                   std::move(*frames)};
}

std::optional<std::vector<AnimationFrame>> decode_animation(std::span<const std::uint8_t> record)
{
    if (record.empty() || record[0] == 0)
    {
        return std::nullopt;
    }
    constexpr std::size_t delay_size = 4;
    std::vector<AnimationFrame> frames;
    std::size_t offset = 1;
    for (std::size_t n = 0; n < record[0]; ++n)
    {
        if (record.size() - offset < delay_size)
        {
            return std::nullopt;
        }
        const auto delay = format::read_u32(record, offset);
        offset += delay_size;
        const auto header = read_header(record, offset);
        if (!header || !has_pixels(*header))
        {
            return std::nullopt;
        }
        offset += picture_header_size;
        auto colors = read_colors(record, offset);
        if (!colors)
        {
            return std::nullopt;
        }
        auto pixels =
            read_frames(record, offset, header->height, header->columns, 1, colors->five_planes());
        if (!pixels)
        {
            return std::nullopt;
        }
        Picture picture{static_cast<std::uint16_t>(header->columns * 8),
                        header->height,
                        header->origin_x,
                        header->origin_y,
                        *colors,
                        std::move(*pixels)};
        if (n > 0)
        {
            const auto &first = frames.front().picture;
            if (picture.width != first.width || picture.height != first.height)
            {
                return std::nullopt; // A difference needs a picture of the same size.
            }
            if (picture.colors.flags == PictureColors::five_planes_flag)
            {
                picture.colors = first.colors;
            }
            std::transform(picture.frames[0].begin(), picture.frames[0].end(),
                           first.frames[0].begin(), picture.frames[0].begin(),
                           [](std::uint8_t difference, std::uint8_t base)
                           {
                               return static_cast<std::uint8_t>(difference ^ base);
                           });
        }
        frames.push_back({delay, std::move(picture)});
    }
    if (offset != record.size())
    {
        return std::nullopt;
    }
    return frames;
}

std::uint8_t ega_color(const PictureColors &colors, std::uint8_t color) noexcept
{
    return colors.ega_map[color % colors.ega_map.size()];
}

Image picture_image(const Picture &picture, std::size_t frame)
{
    Image image;
    image.width = picture.width;
    image.height = picture.height;
    const auto &pixels = picture.frames.at(frame);
    image.rgba.resize(pixels.size() * 4);
    for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
    {
        const auto rgb = vga_rgb(picture.colors, pixels[pixel]);
        std::copy(rgb.begin(), rgb.end(),
                  image.rgba.begin() + static_cast<std::ptrdiff_t>(pixel * 4));
        image.rgba[pixel * 4 + 3] = 255;
    }
    return image;
}

std::optional<SoundSample> decode_sound_sample(unsigned id, std::span<const std::uint8_t> record)
{
    if (id >= sample_rates.size() || sample_rates[id] == 0 || record.empty() ||
        record.size() > sound_buffer_size)
    {
        return std::nullopt;
    }
    return SoundSample{id, sample_rates[id], {record.begin(), record.end()}};
}

} // namespace opengold::gateway
