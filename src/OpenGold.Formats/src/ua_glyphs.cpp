#include "opengold/ua_formats.h"
#include "little_endian.h"
#include "ua_detail.h"

#include <algorithm>

namespace opengold::ua
{
namespace
{

using detail::Bytes;
using detail::has_tag;
using detail::unpack_bits;
using format::read_u16;
using format::read_u32;

constexpr std::size_t library_header_size = 16;
constexpr std::size_t glyph_header_size = 8;
constexpr unsigned planes = 4; // The VGA's unchained mode: a byte a pixel, four planes.
constexpr unsigned color_table_size = 256;
constexpr std::uint8_t see_through_key = 0xFF;

std::int16_t read_i16(Bytes bytes, std::size_t offset)
{
    return static_cast<std::int16_t>(read_u16(bytes, offset));
}

// A glyph's header: rows, the place it is drawn moved up and left by
// origin_row and origin_column, the bytes of each plane's row, and the layout.
struct GlyphHeader
{
    std::int16_t rows{};
    std::int16_t origin_row{};
    std::int16_t origin_column{};
    std::uint8_t row_bytes{};
    std::uint8_t layout{};
};

GlyphHeader read_glyph_header(Bytes item)
{
    return {read_i16(item, 0), read_i16(item, 2), read_i16(item, 4), item[6],
            static_cast<std::uint8_t>(item[7] & 0x0F)};
}

// The layouts' numbers, the low nibble of the header's last byte.
enum Layout : std::uint8_t
{
    planar = 0,
    masked = 1,
    packed_rows = 2,
    keyed = 5,
    coded_runs = 7,
    palette_layout = 8,
    composite_layout = 9,
};

// Each plane holds every fourth pixel of a row: pixel x is in plane x % 4.
std::size_t pixel_of(const VgaImage &image, unsigned plane, std::size_t row, std::size_t byte)
{
    return row * image.width + byte * planes + plane;
}

VgaImage blank_image(const GlyphHeader &header)
{
    VgaImage image;
    image.width = static_cast<std::uint16_t>(header.row_bytes * planes);
    image.height = static_cast<std::uint16_t>(header.rows);
    image.pixels.assign(std::size_t{image.width} * image.height, 0);
    return image;
}

// Plane after plane, each rows * row_bytes bytes.
void read_planes(Bytes planes_bytes, const GlyphHeader &header, VgaImage &image)
{
    const std::size_t plane_size = std::size_t{header.row_bytes} * image.height;
    for (unsigned plane = 0; plane < planes; ++plane)
    {
        for (std::size_t row = 0; row < image.height; ++row)
        {
            for (std::size_t byte = 0; byte < header.row_bytes; ++byte)
            {
                image.pixels[pixel_of(image, plane, row, byte)] =
                    planes_bytes[plane * plane_size + row * header.row_bytes + byte];
            }
        }
    }
}

std::optional<Glyph> decode_planar(Bytes body, const GlyphHeader &header)
{
    Glyph glyph;
    glyph.image = blank_image(header);
    if (body.size() < std::size_t{header.row_bytes} * header.rows * planes)
    {
        return std::nullopt;
    }
    read_planes(body, header, glyph.image);
    if (header.layout == keyed)
    {
        glyph.see_through.resize(glyph.image.pixels.size());
        std::transform(glyph.image.pixels.begin(), glyph.image.pixels.end(),
                       glyph.see_through.begin(),
                       [](std::uint8_t pixel)
                       {
                           return pixel == see_through_key;
                       });
    }
    return glyph;
}

// Four mask planes, then four image planes. The screen keeps its pixel where
// the mask is 0FFh and takes the image's where it is 0.
std::optional<Glyph> decode_masked(Bytes body, const GlyphHeader &header)
{
    const std::size_t block = std::size_t{header.row_bytes} * header.rows * planes;
    if (body.size() < 2 * block)
    {
        return std::nullopt;
    }
    Glyph glyph;
    VgaImage mask = blank_image(header);
    glyph.image = blank_image(header);
    read_planes(body.first(block), header, mask);
    read_planes(body.subspan(block, block), header, glyph.image);
    glyph.see_through.resize(mask.pixels.size());
    for (std::size_t pixel = 0; pixel < mask.pixels.size(); ++pixel)
    {
        if (mask.pixels[pixel] != 0 && mask.pixels[pixel] != 0xFF)
        {
            return std::nullopt;
        }
        glyph.see_through[pixel] = mask.pixels[pixel] == 0xFF;
    }
    return glyph;
}

// Each row is PackBits-packed on its own: the row's bytes of plane 0, then of
// planes 1, 2 and 3.
std::optional<Glyph> decode_packed_rows(Bytes body, const GlyphHeader &header)
{
    Glyph glyph;
    glyph.image = blank_image(header);
    const std::size_t row_size = std::size_t{header.row_bytes} * planes;
    std::size_t position = 0;
    std::vector<std::uint8_t> line;
    for (std::size_t row = 0; row < glyph.image.height; ++row)
    {
        line.clear();
        if (!unpack_bits(body, position, row_size, line))
        {
            return std::nullopt;
        }
        for (unsigned plane = 0; plane < planes; ++plane)
        {
            for (std::size_t byte = 0; byte < header.row_bytes; ++byte)
            {
                glyph.image.pixels[pixel_of(glyph.image, plane, row, byte)] =
                    line[plane * header.row_bytes + byte];
            }
        }
    }
    return glyph;
}

// Plane after plane, each row a list of signed counts ended by 0: a positive
// count copies that many bytes, a negative one skips see-through bytes.
std::optional<Glyph> decode_coded_runs(Bytes body, const GlyphHeader &header)
{
    Glyph glyph;
    glyph.image = blank_image(header);
    glyph.see_through.assign(glyph.image.pixels.size(), true);
    std::size_t position = 0;
    for (unsigned plane = 0; plane < planes; ++plane)
    {
        for (std::size_t row = 0; row < glyph.image.height; ++row)
        {
            std::size_t byte = 0;
            for (;;)
            {
                if (position >= body.size())
                {
                    return std::nullopt;
                }
                const auto count = static_cast<std::int8_t>(body[position++]);
                if (count == 0)
                {
                    break;
                }
                if (count < 0)
                {
                    byte += static_cast<std::size_t>(-count);
                    continue;
                }
                const auto length = static_cast<std::size_t>(count);
                if (byte + length > header.row_bytes || position + length > body.size())
                {
                    return std::nullopt;
                }
                for (std::size_t i = 0; i < length; ++i, ++byte)
                {
                    const auto pixel = pixel_of(glyph.image, plane, row, byte);
                    glyph.image.pixels[pixel] = body[position++];
                    glyph.see_through[pixel] = false;
                }
            }
            if (byte > header.row_bytes)
            {
                return std::nullopt;
            }
        }
    }
    return glyph;
}

// Bit 0 of rows: the header names the first entry and the count; otherwise
// all 256. Bit 1: row_bytes color cycles follow the colors.
std::optional<Glyph> decode_palette(Bytes body, const GlyphHeader &header)
{
    unsigned first = 0;
    unsigned count = color_table_size;
    if ((header.rows & 1) != 0)
    {
        if (header.origin_row < 0 || header.origin_column < 0)
        {
            return std::nullopt;
        }
        first = static_cast<unsigned>(header.origin_row);
        count = static_cast<unsigned>(header.origin_column);
    }
    const std::size_t cycles = (header.rows & 2) != 0 ? header.row_bytes : 0;
    if (first + count > color_table_size || body.size() < std::size_t{count} * 3 + cycles * 4)
    {
        return std::nullopt;
    }
    Glyph glyph;
    glyph.kind = GlyphKind::palette;
    glyph.palette.first = static_cast<std::uint8_t>(first);
    glyph.palette.colors.resize(count);
    // Levels are stored 0 to 255; the VGA takes their top six bits.
    for (std::size_t color = 0; color < count; ++color)
    {
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            glyph.palette.colors[color][channel] =
                static_cast<std::uint8_t>(body[color * 3 + channel] >> 2);
        }
    }
    for (std::size_t cycle = 0; cycle < cycles; ++cycle)
    {
        const auto at = std::size_t{count} * 3 + cycle * 4;
        glyph.cycles.push_back({body[at], body[at + 1], body[at + 2], body[at + 3]});
    }
    return glyph;
}

// Six-byte parts: the glyph, the count of parts (each part repeats it; the
// original reads it again after every part), the row and the column.
std::optional<Glyph> decode_composite(Bytes body)
{
    Glyph glyph;
    glyph.kind = GlyphKind::composite;
    constexpr std::size_t part_size = 6;
    std::size_t count = 1;
    for (std::size_t part = 0; part < count; ++part)
    {
        if ((part + 1) * part_size > body.size())
        {
            return std::nullopt;
        }
        const auto at = part * part_size;
        glyph.parts.push_back({body[at], read_i16(body, at + 2), read_i16(body, at + 4)});
        count = body[at + 1];
    }
    return glyph;
}

std::optional<Glyph> decode_glyph_body(Bytes body, const GlyphHeader &header)
{
    switch (header.layout)
    {
    case planar:
    case keyed:
        return decode_planar(body, header);
    case masked:
        return decode_masked(body, header);
    case packed_rows:
        return decode_packed_rows(body, header);
    case coded_runs:
        return decode_coded_runs(body, header);
    case palette_layout:
        return decode_palette(body, header);
    case composite_layout:
        return decode_composite(body);
    default:
        return std::nullopt;
    }
}

} // namespace

std::optional<Library> decode_library(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < library_header_size || !has_tag(bytes, 0, "HLIB"))
    {
        return std::nullopt;
    }
    const std::size_t size = read_u32(bytes, 4);
    const auto count = read_i16(bytes, 8);
    const auto method = bytes[10];
    // The size stored is the library's own; the file or item holding it may
    // add a byte to reach an even length.
    if (method != 0 || count < 0 || size > bytes.size() || bytes.size() - size > 1)
    {
        return std::nullopt;
    }
    const std::size_t table_end = library_header_size + 4 * (std::size_t(count) + 1);
    if (table_end > size)
    {
        return std::nullopt;
    }
    Library library;
    library.signature.assign(bytes.begin() + 12, bytes.begin() + 16);
    library.indexed = bytes[11] != 0;
    std::size_t start = read_u32(bytes, library_header_size);
    if (start < table_end)
    {
        return std::nullopt;
    }
    for (std::size_t item = 0; item < std::size_t(count); ++item)
    {
        const std::size_t end = read_u32(bytes, library_header_size + 4 * (item + 1));
        if (end < start || end > size)
        {
            return std::nullopt;
        }
        library.items.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                                   bytes.begin() + static_cast<std::ptrdiff_t>(end));
        start = end;
    }
    return library;
}

std::optional<std::vector<IndexEntry>> decode_index(const Library &library)
{
    if (!library.indexed || library.items.empty() || library.items[0].size() < 2)
    {
        return std::nullopt;
    }
    const Bytes index = library.items[0];
    const auto count = read_i16(index, 0);
    if (count < 0 || index.size() < 2 + 4 * std::size_t(count))
    {
        return std::nullopt;
    }
    std::vector<IndexEntry> entries;
    for (std::size_t entry = 0; entry < std::size_t(count); ++entry)
    {
        const IndexEntry read{read_u16(index, 2 + 4 * entry), read_u16(index, 4 + 4 * entry)};
        if (read.item == 0 || read.item >= library.items.size())
        {
            return std::nullopt;
        }
        entries.push_back(read);
    }
    return entries;
}

std::optional<Glyph> decode_glyph(std::span<const std::uint8_t> item)
{
    if (item.size() < glyph_header_size)
    {
        return std::nullopt;
    }
    const auto header = read_glyph_header(item);
    // 80h (a single plane) and 40h (packed rows for other video modes) are
    // never set in the shipped files.
    if ((item[7] & 0xC0) != 0 || header.rows < 0)
    {
        return std::nullopt;
    }
    auto glyph = decode_glyph_body(item.subspan(glyph_header_size), header);
    if (glyph)
    {
        glyph->origin_row = header.origin_row;
        glyph->origin_column = header.origin_column;
    }
    return glyph;
}

std::optional<TileLibrary> decode_tile_library(const Library &library)
{
    if (library.signature != "TILE")
    {
        return std::nullopt;
    }
    TileLibrary tiles;
    if (library.indexed)
    {
        auto index = decode_index(library);
        if (!index)
        {
            return std::nullopt;
        }
        tiles.index = std::move(*index);
    }
    for (std::size_t item = 0; item < library.items.size(); ++item)
    {
        if (item == 0 && library.indexed)
        {
            tiles.glyphs.emplace_back();
            continue;
        }
        auto glyph = decode_glyph(library.items[item]);
        if (!glyph)
        {
            return std::nullopt;
        }
        tiles.glyphs.push_back(std::move(glyph));
    }
    for (const auto &glyph : tiles.glyphs)
    {
        if (!glyph || glyph->kind != GlyphKind::composite)
        {
            continue;
        }
        for (const auto &part : glyph->parts)
        {
            if (part.glyph >= tiles.glyphs.size() || !tiles.glyphs[part.glyph] ||
                tiles.glyphs[part.glyph]->kind != GlyphKind::image)
            {
                return std::nullopt;
            }
        }
    }
    return tiles;
}

std::optional<Font> decode_font(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t cell_size = std::tuple_size_v<Font::Cell>;
    constexpr std::size_t characters = 96;
    constexpr std::size_t control_characters = 32;
    if (bytes.size() != characters * cell_size &&
        bytes.size() != (characters + control_characters) * cell_size)
    {
        return std::nullopt;
    }
    Font font;
    for (std::size_t at = 0; at < bytes.size(); at += cell_size)
    {
        Font::Cell cell{};
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(at), cell_size, cell.begin());
        (at < characters * cell_size ? font.characters : font.control_characters).push_back(cell);
    }
    return font;
}

} // namespace opengold::ua
