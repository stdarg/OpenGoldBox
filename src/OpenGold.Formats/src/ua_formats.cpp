#include "opengold/ua_formats.h"
#include "little_endian.h"
#include "ua_detail.h"

#include <algorithm>

namespace opengold::ua
{
namespace
{

using detail::Bytes;
using detail::find_chunk;
using detail::has_tag;
using detail::read_big_u16;
using detail::read_big_u32;
using detail::read_chunks;
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

// The text up to its first zero byte, or all of it.
std::string text_at(Bytes bytes)
{
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
    return {bytes.begin(), end};
}

bool has_terminator(Bytes bytes)
{
    return std::find(bytes.begin(), bytes.end(), std::uint8_t{0}) != bytes.end();
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

constexpr std::size_t area_header_size = 0x122;
constexpr std::size_t area_squares = 0x240;
constexpr std::size_t area_square_size = 6;
constexpr std::size_t area_events_size = 0x7D0;
constexpr std::size_t area_texts_size = 0x1C00;

const detail::Chunk *sized_chunk(const std::vector<detail::Chunk> &chunks, std::string_view tag,
                                 std::size_t size)
{
    const auto *chunk = find_chunk(chunks, tag);
    return chunk == nullptr || chunk->data.size() != size ? nullptr : chunk;
}

constexpr std::size_t game_settings_size = 0x184;
constexpr std::size_t game_names_offset = 0x34;
constexpr std::size_t game_name_size = 16;
constexpr std::size_t game_password_offset = 0x174;
constexpr std::size_t game_password_size = 9;

} // namespace

namespace detail
{

bool unpack_bits(Bytes packed, std::size_t &position, std::size_t count,
                 std::vector<std::uint8_t> &out)
{
    const auto target = out.size() + count;
    while (out.size() < target)
    {
        if (position >= packed.size())
        {
            return false;
        }
        const auto code = static_cast<std::int8_t>(packed[position++]);
        if (code == -128)
        {
            continue;
        }
        if (code >= 0)
        {
            const std::size_t length = static_cast<std::size_t>(code) + 1;
            if (position + length > packed.size() || out.size() + length > target)
            {
                return false;
            }
            out.insert(out.end(), packed.begin() + static_cast<std::ptrdiff_t>(position),
                       packed.begin() + static_cast<std::ptrdiff_t>(position + length));
            position += length;
        }
        else
        {
            const std::size_t length = static_cast<std::size_t>(1 - code);
            if (position >= packed.size() || out.size() + length > target)
            {
                return false;
            }
            out.insert(out.end(), length, packed[position++]);
        }
    }
    return true;
}

std::optional<std::vector<Chunk>> read_chunks(Bytes bytes, std::size_t offset)
{
    std::vector<Chunk> chunks;
    while (offset < bytes.size())
    {
        if (offset + 8 > bytes.size())
        {
            return std::nullopt;
        }
        const std::size_t size = read_big_u32(bytes, offset + 4);
        if (size > bytes.size() - offset - 8)
        {
            return std::nullopt;
        }
        chunks.push_back({std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4)),
                          bytes.subspan(offset + 8, size)});
        offset += 8 + size + (size & 1);
    }
    return chunks;
}

const Chunk *find_chunk(const std::vector<Chunk> &chunks, std::string_view tag)
{
    const auto found = std::find_if(chunks.begin(), chunks.end(),
                                    [tag](const Chunk &chunk)
                                    {
                                        return chunk.tag == tag;
                                    });
    return found == chunks.end() ? nullptr : &*found;
}

} // namespace detail

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

std::optional<std::vector<ItemRecord>> decode_items(std::span<const std::uint8_t> bytes)
{
    // The game reads exactly 254 items; item 0 is no item.
    constexpr std::size_t item_size = 18;
    constexpr std::size_t item_count = 254;
    if (bytes.size() != item_size * item_count)
    {
        return std::nullopt;
    }
    std::vector<ItemRecord> items;
    for (std::size_t at = 0; at < bytes.size(); at += item_size)
    {
        const auto b = bytes.subspan(at, item_size);
        ItemRecord item;
        item.type = b[0];
        item.name_words = {b[1], b[2], b[3]};
        item.weight = read_u16(b, 4);
        item.value = read_u16(b, 6);
        item.magic_bonus = static_cast<std::int8_t>(b[8]);
        item.save_bonus = static_cast<std::int8_t>(b[9]);
        item.readied = b[10];
        item.hidden_name_words = b[11];
        item.cursed = b[12];
        item.count = b[13];
        item.effects = {b[14], b[15], b[16]};
        items.push_back(item);
    }
    return items;
}

std::optional<Monster> decode_monster(std::span<const std::uint8_t> item)
{
    Monster monster;
    constexpr std::size_t record_size = std::tuple_size_v<decltype(Monster::raw)>;
    if (item.size() == record_size)
    {
        std::copy(item.begin(), item.end(), monster.raw.begin());
    }
    else
    {
        if (item.size() < 2 || read_big_u16(item, 0) != record_size)
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> record;
        std::size_t position = 2;
        if (!unpack_bits(item, position, record_size, record))
        {
            return std::nullopt;
        }
        std::copy(record.begin(), record.end(), monster.raw.begin());
    }
    const Bytes raw = monster.raw;
    constexpr std::size_t name_offset = 0x60;
    constexpr std::size_t name_size = 16;
    const auto name = raw.subspan(name_offset, name_size);
    if (!has_terminator(name))
    {
        return std::nullopt;
    }
    monster.name = text_at(name);
    monster.experience = read_u32(raw, 0x44);
    monster.race = raw[0x58];
    monster.character_class = raw[0x59];
    std::copy_n(raw.begin() + 0x9D, monster.levels.size(), monster.levels.begin());
    monster.hit_points = raw[0x81];
    monster.hit_dice = raw[0x89];
    monster.armor_class_raw = raw[0xB3];
    return monster;
}

std::string unpack_text(std::span<const std::uint8_t> packed)
{
    std::string text;
    std::uint32_t bits = 0;
    unsigned held = 0;
    for (const auto byte : packed)
    {
        bits = bits << 8 | byte;
        held += 8;
        while (held >= 6)
        {
            held -= 6;
            auto code = static_cast<char>((bits >> held) & 0x3F);
            if (code == 0)
            {
                return text;
            }
            if (code <= 0x1F)
            {
                code = static_cast<char>(code + 0x40);
            }
            text.push_back(code);
        }
    }
    return text;
}

std::optional<TextPool> decode_text_pool(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t header_size = 6;
    constexpr std::size_t slots = 0x190;
    constexpr std::size_t text_start = header_size + slots;
    constexpr std::uint8_t skipped_slot = 0xFF;
    if (bytes.size() < text_start)
    {
        return std::nullopt;
    }
    TextPool pool;
    pool.capacity = read_u16(bytes, 0);
    const std::size_t used = read_u16(bytes, 4);
    if (used > pool.capacity || text_start + pool.capacity > bytes.size())
    {
        return std::nullopt;
    }
    const auto texts = bytes.subspan(text_start, used);
    std::size_t offset = 0;
    for (std::size_t slot = 0; slot < slots; ++slot)
    {
        const auto length = bytes[header_size + slot];
        if (length == skipped_slot || length == 0)
        {
            pool.texts.emplace_back();
            continue;
        }
        if (offset + length > texts.size())
        {
            return std::nullopt;
        }
        pool.texts.push_back(unpack_text(texts.subspan(offset, length)));
        offset += length;
    }
    return pool;
}

std::optional<Area> decode_area(std::span<const std::uint8_t> item)
{
    // FORM, its size, AMOD and the size the kit writes for it, then the chunks.
    constexpr std::size_t chunks_start = 16;
    if (item.size() < chunks_start || !has_tag(item, 0, "FORM") || !has_tag(item, 8, "AMOD"))
    {
        return std::nullopt;
    }
    const auto chunks = read_chunks(item, chunks_start);
    if (!chunks)
    {
        return std::nullopt;
    }
    const auto *header = sized_chunk(*chunks, "HDR ", area_header_size);
    const auto *map = sized_chunk(*chunks, "MAP ", area_squares * area_square_size);
    const auto *events = sized_chunk(*chunks, "ENCR", area_events_size);
    const auto *texts = sized_chunk(*chunks, "STRG", area_texts_size);
    if (header == nullptr || map == nullptr || events == nullptr || texts == nullptr)
    {
        return std::nullopt;
    }
    Area area;
    std::copy(header->data.begin(), header->data.end(), area.header.begin());
    area.version = read_big_u16(header->data, 0);
    area.rows = header->data[2];
    area.columns = header->data[3];
    if (area.rows == 0 || area.columns == 0 || std::size_t{area.rows} * area.columns > area_squares)
    {
        return std::nullopt;
    }
    for (std::size_t square = 0; square < area_squares; ++square)
    {
        const auto b = map->data.subspan(square * area_square_size, area_square_size);
        area.squares.push_back({{b[0], b[1], b[2], b[3]}, b[4], b[5]});
    }
    area.events.assign(events->data.begin(), events->data.end());
    auto pool = decode_text_pool(texts->data);
    if (!pool)
    {
        return std::nullopt;
    }
    area.texts = std::move(*pool);
    return area;
}

std::optional<std::vector<Label>> decode_labels(std::span<const std::uint8_t> item)
{
    constexpr std::size_t label_size = 14;
    if (item.size() < label_size)
    {
        return std::nullopt;
    }
    // The first record holds only the count.
    const std::size_t count = read_u16(item, 0);
    if (item.size() != (count + 1) * label_size)
    {
        return std::nullopt;
    }
    std::vector<Label> labels;
    for (std::size_t label = 1; label <= count; ++label)
    {
        const auto b = item.subspan(label * label_size, label_size);
        labels.push_back({b[0], b[1], unpack_text(b.subspan(2))});
    }
    return labels;
}

std::optional<GameSettings> decode_game_settings(std::span<const std::uint8_t> item)
{
    constexpr std::size_t title_size = 0x20;
    if (item.size() != game_settings_size || !has_terminator(item.first(title_size)) ||
        !has_terminator(item.subspan(game_password_offset, game_password_size)))
    {
        return std::nullopt;
    }
    GameSettings settings;
    std::copy(item.begin(), item.end(), settings.raw.begin());
    settings.title = text_at(item.first(title_size));
    settings.start_experience = read_u32(item, 0x20);
    settings.start_platinum = read_u32(item, 0x24);
    settings.start_gems = read_u32(item, 0x28);
    settings.start_jewelry = read_u32(item, 0x2C);
    settings.start_area = item[0x30];
    settings.start_entry = item[0x31];
    settings.start_equipment = item[0x32];
    auto name_at = [item](std::size_t name)
    {
        return text_at(item.subspan(game_names_offset + name * game_name_size, game_name_size));
    };
    for (std::size_t key = 0; key < settings.key_names.size(); ++key)
    {
        settings.key_names[key] = name_at(key);
    }
    for (std::size_t special = 0; special < settings.special_item_names.size(); ++special)
    {
        settings.special_item_names[special] = name_at(settings.key_names.size() + special);
    }
    settings.password = text_at(item.subspan(game_password_offset, game_password_size));
    return settings;
}

std::optional<EditorForm> decode_editor_form(std::span<const std::uint8_t> item)
{
    constexpr std::uint8_t page_end = 2;
    constexpr std::uint8_t first_setting_kind = 0x20;
    constexpr std::uint8_t short_setting_kind = 0x22;
    // The numbers of event kinds the kit does not have are empty items.
    if (item.empty())
    {
        return EditorForm{};
    }
    if (item[0] == 0 || item[0] > item.size() || !has_terminator(item.subspan(1, item[0] - 1)))
    {
        return std::nullopt;
    }
    EditorForm form;
    form.title = text_at(item.subspan(1, item[0] - 1));
    std::size_t at = item[0];
    // Pages until a zero byte: a kind, the page's size, its number, then the
    // fields until the end mark.
    while (at < item.size() && item[at] != 0)
    {
        if (at + 4 > item.size())
        {
            return std::nullopt;
        }
        const std::size_t page_size = read_u16(item, at + 1);
        if (page_size < 5 || at + page_size > item.size())
        {
            return std::nullopt;
        }
        const auto page_bytes = item.subspan(at, page_size);
        EditorPage page;
        page.number = page_bytes[3];
        std::size_t field_at = 4;
        while (field_at < page_bytes.size() && page_bytes[field_at] != page_end)
        {
            if (field_at + 3 > page_bytes.size())
            {
                return std::nullopt;
            }
            const std::size_t field_size = read_u16(page_bytes, field_at + 1);
            if (field_size < 3 || field_at + field_size > page_bytes.size())
            {
                return std::nullopt;
            }
            const auto f = page_bytes.subspan(field_at, field_size);
            EditorField field;
            field.kind = f[0];
            // A setting's operations start after its value; a field's after its
            // place and its text, whose length counts its own byte.
            std::size_t operations = 3;
            if (field.kind >= first_setting_kind)
            {
                if (field.kind != short_setting_kind)
                {
                    if (f.size() < 4)
                    {
                        return std::nullopt;
                    }
                    field.setting = f[3];
                    operations = 4;
                }
            }
            else
            {
                if (f.size() < 6 || f[5] == 0 || 5 + std::size_t{f[5]} > f.size())
                {
                    return std::nullopt;
                }
                field.row = f[3];
                field.column = f[4];
                const auto text = f.subspan(6, f[5] - 1);
                if (!has_terminator(text))
                {
                    return std::nullopt;
                }
                field.text = text_at(text);
                operations = 5 + std::size_t{f[5]};
            }
            field.operations.assign(f.begin() + static_cast<std::ptrdiff_t>(operations), f.end());
            page.fields.push_back(std::move(field));
            field_at += field_size;
        }
        // The end mark is the page's last byte.
        if (field_at + 1 != page_bytes.size())
        {
            return std::nullopt;
        }
        form.pages.push_back(std::move(page));
        at += page_size;
    }
    if (at >= item.size())
    {
        return std::nullopt;
    }
    return form;
}

std::optional<SpeakerSound> decode_speaker_sound(std::span<const std::uint8_t> item)
{
    // A big-endian rate and byte count, then the bytes; the library pads an odd
    // count to an even item.
    constexpr std::size_t header_size = 6;
    if (item.size() < header_size)
    {
        return std::nullopt;
    }
    SpeakerSound sound;
    sound.rate = read_big_u16(item, 0);
    const std::size_t count = read_big_u32(item, 2);
    if (sound.rate == 0 || count > item.size() - header_size ||
        item.size() - header_size - count > 1)
    {
        return std::nullopt;
    }
    for (const auto byte : item.subspan(header_size, count))
    {
        sound.samples.push_back(byte & 0x0F);
        sound.samples.push_back(static_cast<std::uint8_t>(byte >> 4));
    }
    return sound;
}

} // namespace opengold::ua
