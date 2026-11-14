// What Gateway to the Savage Frontier changed in its files, as the census reads
// them (docs/games/gateway.md): pictures with a color header, Curse's creature
// record, ITEMS.1, its scripts' commands and its digitized sound effects. The
// real files are read when OPENGOLD_GOLDBOX_DIR names the folder holding GATEWAY.
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/gateway_formats.h"
#include "opengold/pool_radiance.h"
#include "opengold/wall_art.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
namespace gateway = opengold::gateway;

using Bytes = std::vector<std::uint8_t>;

constexpr auto gateway_game = GoldBoxGame::gateway_to_the_savage_frontier;

void append_u16(Bytes &bytes, unsigned value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

// The nine-byte header: height, width in 8-pixel columns, origin, frame count.
Bytes picture_header(unsigned height, unsigned columns, unsigned count)
{
    Bytes bytes;
    append_u16(bytes, height);
    append_u16(bytes, columns);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    bytes.push_back(static_cast<std::uint8_t>(count));
    return bytes;
}

// The color header of five planes and a palette (flags 0Ch), whose color 18
// has the levels 1, 2, 3 and color 24 the levels 4, 5, 6.
Bytes planar_colors()
{
    Bytes bytes{0x0C};
    Bytes palette(24, 0);
    palette[6] = 0x01; // Color 18's red; color 26's red is the high nibble.
    palette[7] = 0x02;
    palette[8] = 0x03;
    palette[0] = 0x40; // Color 24's red, in the high nibble; color 16's in the low.
    palette[1] = 0x50;
    palette[2] = 0x60;
    bytes.insert(bytes.end(), palette.begin(), palette.end());
    return bytes;
}

// One row of eight pixels in five planes: pixel x has color colors[x].
Bytes planar_row(const std::array<std::uint8_t, 8> &colors)
{
    Bytes planes(5, 0);
    for (std::size_t x = 0; x < colors.size(); ++x)
    {
        for (std::size_t plane = 0; plane < planes.size(); ++plane)
        {
            if ((colors[x] >> plane) & 1)
            {
                planes[plane] |= static_cast<std::uint8_t>(0x80 >> x);
            }
        }
    }
    return planes;
}

std::uint8_t rgb_level(unsigned four_bit_level)
{
    return static_cast<std::uint8_t>(four_bit_level * 4 * 255 / 63);
}

void five_plane_pictures_read_their_own_colors()
{
    auto record = picture_header(1, 1, 1);
    const auto colors = planar_colors();
    record.insert(record.end(), colors.begin(), colors.end());
    const auto row = planar_row({0, 1, 15, 16, 17, 18, 24, 31});
    record.insert(record.end(), row.begin(), row.end());

    const auto picture = gateway::decode_picture(record);
    check(picture && picture->width == 8 && picture->height == 1 && picture->frames.size() == 1,
          "A one-row picture of five planes decodes");
    check(picture->frames[0] == Bytes{0, 1, 15, 16, 17, 18, 24, 31},
          "The first plane is a color's lowest bit, the fifth its highest");
    const auto image = gateway::picture_image(*picture);
    const auto rgb = [&image](std::size_t pixel)
    {
        return std::array<std::uint8_t, 3>{image.rgba[pixel * 4], image.rgba[pixel * 4 + 1],
                                           image.rgba[pixel * 4 + 2]};
    };
    check(rgb(2) == opengold::ega_rgb(15), "Colors below 16 are the EGA colors");
    check(rgb(3) == std::array<std::uint8_t, 3>{242, 194, 64},
          "Color 16 is the VGA mode's fixed orange");
    check(rgb(5) == std::array<std::uint8_t, 3>{rgb_level(1), rgb_level(2), rgb_level(3)},
          "Color 18 comes from the low nibbles of the palette");
    check(rgb(6) == std::array<std::uint8_t, 3>{rgb_level(4), rgb_level(5), rgb_level(6)},
          "Color 24 comes from the high nibbles of the palette");

    record.push_back(0);
    check(!gateway::decode_picture(record), "Bytes past the last frame are no picture");
}

// A DAX archive of one record `id`, stored as literal runs.
Bytes one_record_archive(std::uint8_t id, const Bytes &record)
{
    Bytes stored;
    for (std::size_t offset = 0; offset < record.size();)
    {
        const auto size = std::min<std::size_t>(128, record.size() - offset);
        stored.push_back(static_cast<std::uint8_t>(size - 1));
        stored.insert(stored.end(), record.begin() + static_cast<std::ptrdiff_t>(offset),
                      record.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += size;
    }
    Bytes bytes;
    append_u16(bytes, 9);
    bytes.push_back(id);
    bytes.insert(bytes.end(), {0, 0, 0, 0});
    append_u16(bytes, static_cast<unsigned>(record.size()));
    append_u16(bytes, static_cast<unsigned>(stored.size()));
    bytes.insert(bytes.end(), stored.begin(), stored.end());
    return bytes;
}

void combat_icons_key_black_and_show_dark_gray_black()
{
    auto record = picture_header(1, 1, 1);
    const auto colors = planar_colors();
    record.insert(record.end(), colors.begin(), colors.end());
    const auto row = planar_row({0, 8, 1, 18, 0, 0, 0, 0});
    record.insert(record.end(), row.begin(), row.end());
    const auto dax = one_record_archive(5, record);

    const auto icon = gateway::decode_combat_icon(dax, 5);
    check(icon && icon.image.width == 8 && icon.image.height == 1, "A combat icon decodes");
    const auto &rgba = icon.image.rgba;
    check(rgba[3] == 0, "Black is the icon's transparent color");
    check(rgba[4] == 0 && rgba[5] == 0 && rgba[6] == 0 && rgba[7] == 255, "Dark gray shows black");
    check(std::equal(rgba.begin() + 8, rgba.begin() + 11, opengold::ega_rgb(1).begin()) &&
              rgba[11] == 255,
          "Other colors keep the VGA mode's");
    check(rgba[12] == rgb_level(1) && rgba[15] == 255, "The icon's own colors show");
    check(gateway::decode_combat_icon(dax, 6).status == opengold::FormatResult::not_found,
          "An icon the archive lacks is not found");
}

void older_headers_hold_packed_pixels()
{
    auto record = picture_header(1, 1, 2);
    const Bytes cga_map(8, 0x10); // A first byte with none of the bits CCh set.
    record.insert(record.end(), cga_map.begin(), cga_map.end());
    const Bytes frames{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    record.insert(record.end(), frames.begin(), frames.end());

    const auto picture = gateway::decode_picture(record);
    check(picture && picture->frames.size() == 2, "Two packed frames of one row");
    check(picture->frames[1] == Bytes{8, 9, 10, 11, 12, 13, 14, 15},
          "Two pixels a byte, the high nibble first");
    check(gateway::ega_color(picture->colors, 13) == 13, "The older header maps no color");
}

void a_picture_may_hold_only_its_colors()
{
    auto record = picture_header(0, 0, 1);
    const Bytes colors{0x0E};
    record.insert(record.end(), colors.begin(), colors.end());
    record.resize(record.size() + 16 + 24, 0);
    const auto picture = gateway::decode_picture(record);
    check(picture && picture->frames.empty() && picture->colors.has_palette(),
          "COMSPR's frames keep only their colors");
}

void animations_store_later_frames_as_differences()
{
    Bytes record{2};
    const auto add_frame = [&record](const Bytes &colors, const std::array<std::uint8_t, 8> &row)
    {
        record.insert(record.end(), {10, 0, 0, 0});
        const auto header = picture_header(1, 1, 1);
        record.insert(record.end(), header.begin(), header.end());
        record.insert(record.end(), colors.begin(), colors.end());
        const auto pixels = planar_row(row);
        record.insert(record.end(), pixels.begin(), pixels.end());
    };
    add_frame(planar_colors(), {1, 2, 3, 4, 5, 6, 7, 24});
    add_frame({0x04}, {0, 0, 0, 0, 0, 0, 1, 1}); // "Five planes" only: the first frame's colors.

    const auto frames = gateway::decode_animation(record);
    check(frames && frames->size() == 2 && (*frames)[1].delay == 10, "Two frames and their delay");
    check((*frames)[1].picture.frames[0] == Bytes{1, 2, 3, 4, 5, 6, 6, 25},
          "A later frame is its difference from the first");
    check((*frames)[1].picture.colors.has_palette(), "It takes the first frame's colors");
}

void eight_by_eight_tiles_show_their_ega_colors()
{
    auto record = picture_header(8, 1, 1);
    Bytes colors{0x06}; // An EGA map and five planes.
    Bytes ega_map(16, 0);
    for (std::size_t color = 0; color < 16; ++color)
    {
        ega_map[color / 2] |= static_cast<std::uint8_t>(color << ((color % 2) * 4));
    }
    ega_map[8] = 0x0E; // Color 16 shows as yellow (14), color 17 as black.
    colors.insert(colors.end(), ega_map.begin(), ega_map.end());
    record.insert(record.end(), colors.begin(), colors.end());
    for (int row = 0; row < 8; ++row)
    {
        const auto pixels = planar_row({16, 13, 2, 3, 4, 5, 6, 7});
        record.insert(record.end(), pixels.begin(), pixels.end());
    }
    const auto tiles = opengold::por::decode_wall_tiles(record);
    check(tiles && tiles->size() == 1, "One tile with a color header");
    check((*tiles)[0][0] == 14 && (*tiles)[0][1] == 13,
          "A tile's colors are the EGA map's, the color key kept");
}

void item_types_are_twenty_four_bytes()
{
    Bytes items{0, 0x76};
    for (int type = 0; type < 128; ++type)
    {
        const Bytes fields{0, 1, 1, 6, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0x48, 1, 0};
        items.insert(items.end(), fields.begin(), fields.end());
        items.resize(items.size() + 8, 0xA2);
    }
    const auto types = opengold::por::decode_item_templates(items, gateway_game);
    check(types && types->size() == 128, "ITEMS.1 holds 128 types");
    check((*types)[127].hands == 1 && (*types)[127].class_restrictions == 0x48,
          "Each type's 16 rule bytes are read past the 8 unused ones");
    items.resize(2 + 16 * 5);
    check(!opengold::por::decode_item_templates(items, gateway_game),
          "Gateway's types are whole 24-byte records");
    check(opengold::por::decode_item_templates(items, GoldBoxGame::curse_of_the_azure_bonds)
                  ->size() == 5,
          "Curse's ITEMS keeps 16 bytes a type");
}

void scripts_name_items_by_type_and_words()
{
    check(opengold::por::ecl_opcode(0x32, gateway_game).operands == 4,
          "FIND ITEM takes a type and three name words");
    check(opengold::por::ecl_opcode(0x40, gateway_game).operands == 4,
          "DESTROY ITEMS takes a type and three name words");
    check(opengold::por::ecl_opcode(0x41, gateway_game).name == "AWARD EXPERIENCE",
          "AWARD EXPERIENCE stays");
    check(opengold::por::ecl_opcode(0x32, GoldBoxGame::secret_of_the_silver_blades).operands == 1,
          "Secret's FIND ITEM keeps one operand");
    const auto memory = opengold::por::ecl_memory(gateway_game);
    check(memory.origin == 0x8000, "Scripts load at 8000h, as in Curse");
    const auto routines = opengold::por::ecl_engine_routines(gateway_game);
    check(routines.redraw_view == 0x2E10 && routines.refresh_wall_ahead == 0xC01A,
          "CALL AE11h redraws the view; 401Bh rereads the party's cell");
}

void sound_effects_are_digitized()
{
    const Bytes samples(100, 0x80);
    const auto effect = gateway::decode_sound_sample(5, samples);
    check(effect && effect->rate == 9000 && effect->samples.size() == 100,
          "Effect 5 plays at 9000 samples a second");
    check(!gateway::decode_sound_sample(3, samples), "The game plays effect 3 as 9");
    check(!gateway::decode_sound_sample(4, Bytes(9001, 0x80)),
          "An effect fits the 9000-byte sound buffer");
}

std::optional<std::vector<opengold::DaxRecord>> archive(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    const Bytes bytes(std::istreambuf_iterator<char>(input), {});
    auto decoded = opengold::decode_dax_archive(bytes);
    if (!decoded)
    {
        return std::nullopt;
    }
    return std::move(decoded.records);
}

const Bytes &record(const std::vector<opengold::DaxRecord> &records, unsigned id)
{
    const auto found = std::find_if(records.begin(), records.end(),
                                    [id](const opengold::DaxRecord &candidate)
                                    {
                                        return candidate.id == id;
                                    });
    check(found != records.end(), "Record " + std::to_string(id) + " is there");
    return found->bytes;
}

// The developer's copy (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
void the_real_files_decode()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "GATEWAY" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != gateway_game)
    {
        std::cout << "Gateway's files are not here; they are not checked.\n";
        return;
    }
    const auto census = opengold::take_census(folder, gateway_game);
    for (const auto &row : census.rows)
    {
        check(row.complete(), row.kind + " decodes: " + row.first_failure);
    }
    check(census.complete(), "Every file decodes, the music too");

    const auto titles = archive(folder / "TITLE.DAX");
    check(titles.has_value(), "TITLE.DAX is an archive");
    const auto title = gateway::decode_picture(record(*titles, 1));
    check(title && title->width == 320 && title->height == 200 && title->colors.five_planes(),
          "The title fills the screen in five planes");

    const auto creatures = archive(folder / "MON2CHA.DAX");
    check(creatures.has_value(), "MON2CHA.DAX is an archive");
    const auto troll = opengold::por::decode_character(record(*creatures, 41), gateway_game);
    check(troll && troll->name == "TROLL" && troll->base_armor_class == 4 &&
              troll->max_hit_points == 36 && troll->game == gateway_game,
          "Creature 41 is a troll of Armor Class 4 and 36 hit points");

    const auto sounds = archive(folder / "ZOUNDS.DAX");
    check(sounds.has_value() && sounds->size() == 9, "ZOUNDS.DAX holds nine effects");
}

} // namespace

int main()
{
    try
    {
        five_plane_pictures_read_their_own_colors();
        older_headers_hold_packed_pixels();
        combat_icons_key_black_and_show_dark_gray_black();
        a_picture_may_hold_only_its_colors();
        animations_store_later_frames_as_differences();
        eight_by_eight_tiles_show_their_ega_colors();
        item_types_are_twenty_four_bytes();
        scripts_name_items_by_type_and_words();
        sound_effects_are_digitized();
        the_real_files_decode();
        std::cout << "Gateway census passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
