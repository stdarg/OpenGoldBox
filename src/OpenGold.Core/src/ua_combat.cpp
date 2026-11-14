#include "opengold/ua_combat.h"

#include "opengold/ua_view.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

using por::FieldEdge;

// The area header's combat sets: the dungeon's, then the outdoor one.
constexpr std::size_t header_dungeon_set = 0xC;
constexpr std::size_t field_width = 50, field_height = 25;
// A combat square is 24 pixels on a side.
constexpr int square_pixels = 24;
// The kit's action pose of icon n is icon n + 128.
constexpr unsigned action_pose = 128;

// A wall byte's picture (its low four bits) and what it does to a step (its
// high four): from 14 on (blocked, a false door) it stops one.
[[nodiscard]] FieldEdge side_of(std::uint8_t wall)
{
    if ((wall & 0xF) == 0)
    {
        return FieldEdge::open;
    }
    return (wall >> 4) >= 14 ? FieldEdge::wall : FieldEdge::doorway;
}

// One square's side as the field sees it (cmbsetup.c, wall_at): past the
// area's edge a wall, except east and west along the party's own row.
[[nodiscard]] FieldEdge square_side(const Area &area, int x, int y, unsigned direction, int party_y)
{
    if (x < 0 || y < 0 || x >= area.columns || y >= area.rows)
    {
        return y == party_y && (direction == 1 || direction == 3) ? FieldEdge::open
                                                                  : FieldEdge::wall;
    }
    const auto &square =
        area.squares[static_cast<std::size_t>(y) * area.columns + static_cast<std::size_t>(x)];
    return side_of(square.walls[direction]);
}

// The glyphs of combat set `set` of a library: the design's own loose file
// first, named by the kit's four letters, bank 1 and the set ("DUNG1003.TLB",
// tiles.c's tile_open_file), else the kit's library's set.
[[nodiscard]] TileLibrary set_tiles(const DesignFiles &files, std::string_view kit_file,
                                    std::string_view letters, unsigned set)
{
    const auto what = std::string(kit_file) + " set " + std::to_string(set);
    char loose_name[16];
    std::snprintf(loose_name, sizeof loose_name, "%.4s1%03u.TLB", letters.data(), set % 1000);
    if (const auto loose = files.library(loose_name))
    {
        auto tiles = decode_tile_library(*loose);
        if (!tiles)
        {
            throw std::runtime_error(what + ": its glyphs do not decode");
        }
        return std::move(*tiles);
    }
    const auto library = files.library(kit_file);
    if (!library)
    {
        throw std::runtime_error(what + ": " + std::string(kit_file) + " is missing");
    }
    return nested_tiles(*library, set, what);
}

// The fight's colors: the 16 EGA colors, the combat screen's own (COMSPR,
// cmbsetup.c's map_build), then the field set's over them.
[[nodiscard]] std::array<VgaColor, 256> fight_palette(const DesignFiles &files,
                                                      const TileLibrary &set)
{
    std::array<VgaColor, 256> palette = vga_color_table({});
    if (const auto screen = files.library("COMSPR.TLB"))
    {
        if (const auto tiles = decode_tile_library(*screen))
        {
            apply_palette_glyph(palette, *tiles);
        }
    }
    apply_palette_glyph(palette, set);
    return palette;
}

// A set's tile pictures: tile n is its image n + 1, after its palette.
[[nodiscard]] FightField field_with_art(const DesignFiles &files, const TileLibrary &set,
                                        por::DungeonBattlefield field, std::size_t tiles)
{
    FightField result{std::move(field), {}, fight_palette(files, set)};
    for (std::size_t tile = 0; tile < tiles; ++tile)
    {
        const auto at = tile + 1;
        const bool image =
            at < set.glyphs.size() && set.glyphs[at] && set.glyphs[at]->kind == GlyphKind::image;
        result.terrain_art.push_back(image ? glyph_image(*set.glyphs[at], result.palette)
                                           : Image{});
    }
    return result;
}

// WILDCOM's tiles and what they do to a step (combtab.c's tile_info, from
// its entry 1Fh): 0 stops one, 2 costs double.
constexpr std::size_t wild_tiles = 38;
constexpr std::array<std::uint8_t, wild_tiles> wild_step_cost{0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 2,
                                                              1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 0, 0, 0,
                                                              1, 1, 1, 1, 1, 0, 0, 2, 1, 0, 1, 1};
// The tiles wild_map lays: open ground, a tree's crown above its trunk, a
// lone tree, brush, grass and stones.
constexpr std::uint8_t open_ground = 37, first_crown = 0, first_trunk = 5, first_lone_tree = 10,
                       brush = 12, grass = 36, first_stone = 30;

// Trees stand where two squares of open ground are one above the other,
// one time in 20 (the kit's terrain 0 has no river, wild_woods).
void plant_trees(std::vector<std::uint8_t> &tiles, const std::function<unsigned(unsigned)> &roll)
{
    constexpr unsigned chance = 5;
    for (std::size_t column = 0; column < field_width; ++column)
    {
        for (std::size_t row = 1; row < field_height; ++row)
        {
            auto &here = tiles[row * field_width + column];
            auto &above = tiles[(row - 1) * field_width + column];
            if (here != open_ground || above != open_ground || roll(100) > chance)
            {
                continue;
            }
            if (roll(100) <= chance)
            {
                here = static_cast<std::uint8_t>(first_lone_tree + roll(2) - 1);
            }
            else
            {
                above = static_cast<std::uint8_t>(first_crown + roll(5) - 1);
                here = static_cast<std::uint8_t>(first_trunk + roll(5) - 1);
            }
        }
    }
}

// The rest of the open ground: brush on 3 in 100, grass on 14, stones on 5
// (wild_scatter at the kit's terrain 0).
void scatter(std::vector<std::uint8_t> &tiles, const std::function<unsigned(unsigned)> &roll)
{
    for (std::size_t column = 0; column < field_width; ++column)
    {
        for (std::size_t row = 0; row < field_height; ++row)
        {
            auto &tile = tiles[row * field_width + column];
            if (tile != open_ground)
            {
                continue;
            }
            const unsigned dice = roll(100);
            if (dice <= 3)
            {
                tile = brush;
            }
            else if (dice <= 17)
            {
                tile = grass;
            }
            else if (dice <= 22)
            {
                tile = static_cast<std::uint8_t>(first_stone + roll(6) - 1);
            }
        }
    }
}

[[nodiscard]] rules::Terrain wild_terrain(std::uint8_t tile)
{
    switch (wild_step_cost[tile])
    {
    case 0:
        return rules::Terrain::obstacle;
    case 2:
        return rules::Terrain::difficult;
    default:
        return rules::Terrain::open;
    }
}

[[nodiscard]] std::optional<Glyph> icon_glyph(const TileLibrary &icons, unsigned number)
{
    const auto entry = std::ranges::find(icons.index, number, &IndexEntry::number);
    if (entry == icons.index.end() || entry->item >= icons.glyphs.size())
    {
        return std::nullopt;
    }
    const auto &glyph = icons.glyphs[entry->item];
    return glyph && glyph->kind == GlyphKind::image ? glyph : std::nullopt;
}

// The squares an icon fills (icon_size's rows and columns of combat squares,
// sized as monster_add does: 1 one square, 2 tall, 3 wide, 4 two by two).
[[nodiscard]] rules::Footprint icon_footprint(const Glyph &glyph)
{
    const auto squares = [](int pixels)
    {
        return std::clamp((pixels + square_pixels - 1) / square_pixels, 1, 2);
    };
    return {squares(glyph.image.width), squares(glyph.image.height)};
}

} // namespace

FightField dungeon_field(const DesignFiles &files, const Area &area, unsigned x, unsigned y)
{
    const auto set = std::max<unsigned>(area.header[header_dungeon_set], 1);
    const int party_x = static_cast<int>(x), party_y = static_cast<int>(y);
    const auto edges = [&](int column, int row, unsigned direction)
    {
        static constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
        const int sx = party_x + column, sy = party_y + row;
        const auto here = square_side(area, sx, sy, direction, party_y);
        const auto there =
            square_side(area, sx + dx[direction], sy + dy[direction], (direction + 2) % 4, party_y);
        return FieldEdge{static_cast<unsigned>(here) | static_cast<unsigned>(there)};
    };
    constexpr std::size_t dungeon_tiles = 25;
    return field_with_art(files, set_tiles(files, "DUNGCOM.TLB", "DUNG", set),
                          por::kit_dungeon_battlefield(edges), dungeon_tiles);
}

FightField wilderness_field(const DesignFiles &files, unsigned set,
                            const std::function<unsigned(unsigned)> &roll)
{
    por::DungeonBattlefield field;
    auto &board = field.geometry;
    board.width = field_width;
    board.height = field_height;
    field.tiles.assign(field_width * field_height, open_ground);
    plant_trees(field.tiles, roll);
    scatter(field.tiles, roll);
    for (const auto tile : field.tiles)
    {
        board.terrain.push_back(wild_terrain(tile));
    }
    // Out of doors every square is under the sky.
    board.plants.assign(field.tiles.size(), true);
    return field_with_art(files, set_tiles(files, "WILDCOM.TLB", "WILD", std::max(set, 1U)),
                          std::move(field), wild_tiles);
}

std::optional<CombatIcon> combat_icon(const DesignFiles &files, unsigned number,
                                      const std::array<VgaColor, 256> &palette)
{
    std::optional<Glyph> ready, action;
    char loose_name[16];
    std::snprintf(loose_name, sizeof loose_name, "CPIC1%03u.TLB", number % 1000);
    if (const auto loose = files.library(loose_name))
    {
        // A design's own icon: its two poses in turn.
        const auto tiles = decode_tile_library(*loose);
        if (!tiles)
        {
            throw std::runtime_error(std::string(loose_name) + ": its glyphs do not decode");
        }
        std::vector<Glyph> images;
        for (const auto &glyph : tiles->glyphs)
        {
            if (glyph && glyph->kind == GlyphKind::image)
            {
                images.push_back(*glyph);
            }
        }
        if (!images.empty())
        {
            ready = images.front();
            action = images.size() > 1 ? images[1] : images.front();
        }
    }
    else if (const auto library = files.library("CPIC.TLB"))
    {
        const auto tiles = decode_tile_library(*library);
        if (!tiles)
        {
            throw std::runtime_error("CPIC.TLB: its glyphs do not decode");
        }
        ready = icon_glyph(*tiles, number);
        action = icon_glyph(*tiles, number + action_pose);
    }
    if (!ready)
    {
        return std::nullopt;
    }
    CombatIcon icon{glyph_image(*ready, palette), glyph_image(action ? *action : *ready, palette),
                    icon_footprint(*ready)};
    return icon;
}

} // namespace opengold::ua
