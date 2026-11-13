// Unlimited Adventures' files as the census reads them (docs/games/ua.md):
// resource libraries, glyphs in the VGA's four planes, the kit's records and
// the common formats of its art, sounds and music. The real files are read
// when OPENGOLD_GOLDBOX_DIR names the folder holding UA.
#include "opengold/game_census.h"
#include "opengold/pool_radiance.h"
#include "opengold/ua_formats.h"
#include "opengold/ua_media.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
namespace ua = opengold::ua;

using Bytes = std::vector<std::uint8_t>;

void append_u16(Bytes &bytes, unsigned value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes &bytes, std::uint32_t value)
{
    append_u16(bytes, value & 0xFFFF);
    append_u16(bytes, value >> 16);
}

void append_big_u32(Bytes &bytes, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void append_text(Bytes &bytes, std::string_view text)
{
    bytes.insert(bytes.end(), text.begin(), text.end());
}

Bytes chunk(std::string_view tag, const Bytes &data)
{
    Bytes bytes;
    append_text(bytes, tag);
    append_big_u32(bytes, static_cast<std::uint32_t>(data.size()));
    bytes.insert(bytes.end(), data.begin(), data.end());
    if (data.size() % 2 != 0)
    {
        bytes.push_back(0);
    }
    return bytes;
}

// A library of the items, with its signature and packing method.
Bytes library(std::string_view signature, const std::vector<Bytes> &items, bool indexed = false,
              std::uint8_t method = 0)
{
    const std::size_t table_end = 16 + 4 * (items.size() + 1);
    std::size_t size = table_end;
    for (const auto &item : items)
    {
        size += item.size();
    }
    Bytes bytes;
    append_text(bytes, "HLIB");
    append_u32(bytes, static_cast<std::uint32_t>(size));
    append_u16(bytes, static_cast<unsigned>(items.size()));
    bytes.push_back(method);
    bytes.push_back(indexed ? 1 : 0);
    append_text(bytes, signature);
    std::size_t offset = table_end;
    append_u32(bytes, static_cast<std::uint32_t>(offset));
    for (const auto &item : items)
    {
        offset += item.size();
        append_u32(bytes, static_cast<std::uint32_t>(offset));
    }
    for (const auto &item : items)
    {
        bytes.insert(bytes.end(), item.begin(), item.end());
    }
    return bytes;
}

Bytes glyph_header(int rows, int origin_row, int origin_column, std::uint8_t row_bytes,
                   std::uint8_t layout)
{
    Bytes bytes;
    append_u16(bytes, static_cast<unsigned>(rows) & 0xFFFF);
    append_u16(bytes, static_cast<unsigned>(origin_row) & 0xFFFF);
    append_u16(bytes, static_cast<unsigned>(origin_column) & 0xFFFF);
    bytes.push_back(row_bytes);
    bytes.push_back(static_cast<std::uint8_t>(0x10 | layout));
    return bytes;
}

Bytes with(Bytes bytes, const Bytes &more)
{
    bytes.insert(bytes.end(), more.begin(), more.end());
    return bytes;
}

void libraries_hold_items_behind_an_index()
{
    Bytes index;
    append_u16(index, 1);
    append_u16(index, 40); // Number 40 is item 1.
    append_u16(index, 1);
    const auto bytes = library("DATA", {index, Bytes{7, 8, 9}}, true);
    const auto decoded = ua::decode_library(bytes);
    check(decoded && decoded->signature == "DATA" && decoded->indexed &&
              decoded->items.size() == 2 && decoded->items[1] == Bytes{7, 8, 9},
          "A library's items come out whole");
    const auto entries = ua::decode_index(*decoded);
    check(entries && entries->size() == 1 && (*entries)[0].number == 40 && (*entries)[0].item == 1,
          "The index maps number 40 to item 1");

    check(!ua::decode_library(library("DATA", {Bytes{1}}, false, 3)),
          "A packed library is refused: no shipped file is packed");
    auto backwards = library("DATA", {Bytes{1, 2}, Bytes{3}});
    backwards[20] = 0xFF; // Item 0 now ends past the library.
    check(!ua::decode_library(backwards), "Offsets past the end are refused");
}

// Pixel x of a row sits in plane x % 4; a row of one byte a plane is 4 pixels.
void planes_hold_every_fourth_pixel()
{
    const auto planar = ua::decode_glyph(with(glyph_header(1, 2, 3, 1, 0), {10, 11, 12, 13}));
    check(planar && planar->kind == ua::GlyphKind::image && planar->image.width == 4 &&
              planar->image.pixels == Bytes{10, 11, 12, 13} && planar->see_through.empty() &&
              planar->origin_row == 2 && planar->origin_column == 3,
          "A plain glyph takes a pixel from each plane in turn");

    // Two pixels a plane: plane 0 holds pixels 0 and 4.
    const auto wide = ua::decode_glyph(with(glyph_header(1, 0, 0, 2, 0), {0, 4, 1, 5, 2, 6, 3, 7}));
    check(wide && wide->image.pixels == Bytes{0, 1, 2, 3, 4, 5, 6, 7},
          "Each plane's bytes are every fourth pixel");

    const auto keyed = ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 5), {0xFF, 1, 0xFF, 3}));
    check(keyed && keyed->see_through == std::vector<bool>{true, false, true, false},
          "A keyed glyph's 0FFh is see-through");

    const auto masked =
        ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 1), {0xFF, 0, 0, 0xFF, 0, 5, 6, 0}));
    check(masked && masked->image.pixels == Bytes{0, 5, 6, 0} &&
              masked->see_through == std::vector<bool>{true, false, false, true},
          "A masked glyph shows the image where its mask is 0");
    check(!ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 1), {0x0F, 0, 0, 0, 1, 1, 1, 1})),
          "A mask of part of a pixel is refused");

    // PackBits: a run of three 9s and a literal 4.
    const auto packed = ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 2), {0xFE, 9, 0, 4}));
    check(packed && packed->image.pixels == Bytes{9, 9, 9, 4}, "Packed rows unpack by plane");
    check(!ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 2), {0xFC, 9})),
          "A run longer than its row is refused");

    // Planes 0 and 2 copy a byte, planes 1 and 3 skip theirs.
    const auto runs =
        ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 7), {1, 7, 0, 0xFF, 0, 1, 8, 0, 0}));
    check(runs && runs->image.pixels[0] == 7 && runs->image.pixels[2] == 8 &&
              runs->see_through == std::vector<bool>{false, true, false, true},
          "Coded runs copy bytes and skip see-through ones");

    check(!ua::decode_glyph(with(glyph_header(1, 0, 0, 1, 3), {0, 0, 0, 0})),
          "A layout the shipped files do not use is refused");
}

void palettes_set_a_run_of_colors()
{
    // Rows 3: a first entry and count, and one color cycle.
    const auto palette = ua::decode_glyph(
        with(glyph_header(3, 32, 2, 1, 8), {0, 128, 255, 4, 8, 252, 1, 10, 32, 2}));
    check(palette && palette->kind == ua::GlyphKind::palette && palette->palette.first == 32 &&
              palette->palette.colors.size() == 2 &&
              palette->palette.colors[0] == opengold::VgaColor{0, 32, 63} &&
              palette->cycles.size() == 1 && palette->cycles[0].rate == 10 &&
              palette->cycles[0].first == 32,
          "A palette's 8-bit levels become the VGA's six bits");
    check(!ua::decode_glyph(with(glyph_header(1, 250, 10, 0, 8), Bytes(30, 0))),
          "Colors past entry 255 are refused");
}

void composites_name_images_of_their_library()
{
    const auto image = with(glyph_header(1, 0, 0, 1, 0), {1, 2, 3, 4});
    Bytes composite = glyph_header(1, 0, 0, 1, 9);
    composite.insert(composite.end(), {1, 2, 4, 0, 8, 0, 1, 2, 0, 0, 0, 0});
    const auto tiles =
        ua::decode_tile_library(*ua::decode_library(library("TILE", {image, image, composite})));
    check(tiles && tiles->glyphs.size() == 3 && tiles->glyphs[2]->parts.size() == 2 &&
              tiles->glyphs[2]->parts[0].row == 4 && tiles->glyphs[2]->parts[0].column == 8,
          "A composite lists its parts and where they go");

    Bytes self = glyph_header(1, 0, 0, 1, 9);
    self.insert(self.end(), {0, 1, 0, 0, 0, 0});
    check(!ua::decode_tile_library(*ua::decode_library(library("TILE", {self}))),
          "A composite that names a composite is refused");
}

void records_unpack()
{
    // "AB 1": codes 1, 2, 32 and 49 in six bits each.
    check(ua::unpack_text(Bytes{0x04, 0x28, 0x31}) == "AB 1", "Six-bit text unpacks");
    check(ua::unpack_text(Bytes{0x04, 0x00, 0x31}) == "A", "A zero code ends the text");

    Bytes raw(450, 0);
    raw[0x60] = 'O';
    raw[0x61] = 'R';
    raw[0x62] = 'C';
    raw[0x81] = 5;
    const auto orc = ua::decode_monster(raw);
    check(orc && orc->name == "ORC" && orc->hit_points == 5, "An unpacked monster reads whole");
    // 450 zero bytes: three runs of 128 and one of 66.
    const Bytes packed{0x01, 0xC2, 0x81, 0, 0x81, 0, 0x81, 0, 0xBF, 0};
    check(ua::decode_monster(packed).has_value(), "A packed monster unpacks to 450 bytes");
    check(!ua::decode_monster(Bytes{0x01, 0xC2, 0x81, 0}),
          "A monster that unpacks short is refused");

    Bytes labels(14, 0);
    labels[0] = 1;
    labels.insert(labels.end(), {7, 1, 0x04, 0x28, 0x31});
    labels.resize(28, 0);
    const auto decoded = ua::decode_labels(labels);
    check(decoded && decoded->size() == 1 && (*decoded)[0].number == 7 &&
              (*decoded)[0].text == "AB 1",
          "A label table counts its labels in its first record");

    // Rate 7418, one byte: two samples, the low nibble first.
    const auto sound = ua::decode_speaker_sound(Bytes{0x1C, 0xFA, 0, 0, 0, 1, 0x21, 0});
    check(sound && sound->rate == 7418 && sound->samples == Bytes{1, 2},
          "A speaker sound holds two 4-bit samples a byte");

    check(opengold::por::decode_item_templates(Bytes(2048, 0), GoldBoxGame::unlimited_adventures)
                  ->size() == 128,
          "ITEMS.DAT holds 128 item types of 16 bytes, with no header");
}

Bytes voice_file(const Bytes &blocks)
{
    Bytes bytes;
    append_text(bytes, "Creative Voice File\x1A");
    append_u16(bytes, 26);
    append_u16(bytes, 0x010A);
    append_u16(bytes, static_cast<std::uint16_t>(~0x010A + 0x1234));
    bytes.insert(bytes.end(), blocks.begin(), blocks.end());
    return bytes;
}

void voice_files_split_at_markers()
{
    // Marker 3, then 4-bit ADPCM at 10,000 a second: a starting level of 128,
    // a step of +7 (which doubles the next steps), then +1 doubled.
    const Bytes blocks{4, 2, 0, 0, 3, 0, 1, 4, 0, 0, 156, 1, 128, 0x71, 0};
    const auto sections = ua::decode_voice_file(voice_file(blocks));
    check(sections && sections->size() == 1 && (*sections)[0].marker == 3 &&
              (*sections)[0].rate == 10000 && (*sections)[0].samples == Bytes{128, 135, 137},
          "A marker starts an effect, and ADPCM expands");
    check(!ua::decode_voice_file(voice_file(Bytes{1, 3, 0, 0, 156, 2, 128, 0})),
          "Packings other than 8-bit and 4-bit ADPCM are refused");
    check(!ua::decode_voice_file(voice_file(Bytes{4, 2, 0, 0, 3, 0})),
          "A file with no terminator is refused");
}

Bytes xmi(const Bytes &events)
{
    Bytes info;
    append_u16(info, 1);
    Bytes directory;
    append_text(directory, "XDIR");
    directory = with(directory, chunk("INFO", info));
    Bytes form;
    append_text(form, "XMID");
    form = with(form, chunk("TIMB", Bytes{1, 0, 25, 0}));
    form = with(form, chunk("EVNT", events));
    Bytes catalog;
    append_text(catalog, "XMID");
    catalog = with(catalog, chunk("FORM", form));
    return with(chunk("FORM", directory), chunk("CAT ", catalog));
}

void music_is_xmi()
{
    // A note of 128 ticks, a pause of 16, and the end.
    const Bytes events{0x90, 60, 64, 0x81, 0x00, 0x10, 0xFF, 0x2F, 0};
    const auto sequences = ua::decode_xmi(xmi(events));
    check(sequences && sequences->size() == 1 && (*sequences)[0].timbres.size() == 1 &&
              (*sequences)[0].timbres[0][0] == 25,
          "An XMI file lists its sequences and their timbres");
    const auto &decoded = (*sequences)[0].events;
    check(decoded.size() == 2 && decoded[0].data1 == 60 && decoded[0].duration == 128 &&
              decoded[1].tick == 16 && decoded[1].data1 == 0x2F,
          "A note carries its length, and pauses add up");
    check(!ua::decode_xmi(xmi(Bytes{0x90, 60, 64, 0x10})),
          "A sequence that does not end is refused");
    check(!ua::decode_xmi(xmi(Bytes{0x90, 60, 0x80, 0x10, 0xFF, 0x2F, 0})),
          "A data byte with its top bit set is refused");
}

Bytes read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

ua::Library open(const std::filesystem::path &path)
{
    auto library = ua::decode_library(read_file(path));
    check(library.has_value(), path.filename().string() + " is a library");
    return std::move(*library);
}

// The developer's copy (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
void the_real_files_decode()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "UA" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::unlimited_adventures)
    {
        std::cout << "Unlimited Adventures' files are not here; they are not checked.\n";
        return;
    }
    const auto census = opengold::take_census(folder, GoldBoxGame::unlimited_adventures);
    for (const auto &row : census.rows)
    {
        check(row.complete(), row.kind + " decodes: " + row.first_failure);
    }
    check(census.complete() && census.rows.size() == 18, "Every file of every kind is read");

    const auto title = open(folder / "DISK1/TITLE.TLB");
    const auto backdrop = ua::decode_tile_library(*ua::decode_library(title.items[1]));
    check(backdrop && backdrop->glyphs[0]->kind == ua::GlyphKind::palette &&
              backdrop->glyphs[0]->palette.colors.size() == 256 &&
              backdrop->glyphs[1]->image.width == 320 && backdrop->glyphs[1]->image.height == 200,
          "The title's backdrop fills the screen with its own 256 colors");

    const auto icons = ua::decode_tile_library(open(folder / "DISK2/CPIC.TLB"));
    check(icons && icons->index.size() == 254 && icons->glyphs[2]->image.width == 24,
          "CPIC.TLB holds the combat icons behind an index");

    const auto monsters = open(folder / "DISK2/MONST.GLB");
    const auto kobold = ua::decode_monster(monsters.items[1]);
    check(kobold && kobold->name == "KOBOLD" && kobold->hit_points == 3, "Monster 1 is a kobold");

    const auto areas = open(folder / "DISK3/GEO.GLB");
    const auto area = ua::decode_area(areas.items[1]);
    check(area && area->version == 0x6A && area->squares.size() == 576 &&
              area->texts.texts.size() == 400,
          "An area holds 576 squares and a pool of 400 texts");

    const auto labels = ua::decode_labels(open(folder / "DISK3/STRG.GLB").items[1]);
    check(labels && (*labels)[1].text == "BASILISK", "The monsters' labels name them");

    const auto settings = ua::decode_game_settings(open(folder / "DISK3/GAME.GLB").items[1]);
    check(settings && settings->key_names[0] == "Brass" &&
              settings->special_item_names[0] == "Item 1",
          "The game's settings name the keys and the special items");

    const auto forms = open(folder / "DISK3/SCRIPT.GLB");
    const auto common = ua::decode_editor_form(forms.items[0]);
    check(common && common->title == "common event page 0" && !common->pages.empty() &&
              common->pages[0].fields[0].text == "event control:",
          "The event editor's first form lays out its fields");
    check(ua::decode_editor_form(forms.items[28])->pages.empty(),
          "An unused event kind has an empty form");

    const auto items = ua::decode_items(read_file(folder / "DISK1/ITEM.DAT"));
    check(items && items->size() == 254, "ITEM.DAT holds 254 items");

    const auto effects = ua::decode_voice_file(read_file(folder / "DISK1/SFXDQ.VOC"));
    check(effects && effects->size() == 13 && effects->back().marker == 12,
          "SFXDQ.VOC holds effects 0 to 12");

    const auto songs = ua::decode_xmi(read_file(folder / "DISK1/ADDQ2.XMI"));
    check(songs && songs->size() == 6, "ADDQ2.XMI holds six sequences");

    const auto town = ua::decode_pcx(read_file(folder / "ART/TOWN.PCX"));
    const auto town_lbm = ua::decode_ilbm(read_file(folder / "ART/TOWN.LBM"));
    check(town && town_lbm && town->image.width == 320 && town_lbm->image.height == 200,
          "The art templates are whole screens");
}

} // namespace

int main()
{
    try
    {
        libraries_hold_items_behind_an_index();
        planes_hold_every_fourth_pixel();
        palettes_set_a_run_of_colors();
        composites_name_images_of_their_library();
        records_unpack();
        voice_files_split_at_markers();
        music_is_xmi();
        the_real_files_decode();
        std::cout << "Unlimited Adventures census passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
