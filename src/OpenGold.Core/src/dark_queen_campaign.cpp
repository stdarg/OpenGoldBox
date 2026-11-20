#include "dark_queen_campaign.h"
#include "dark_queen_shop_table.h"

#include "opengold/dark_queen_records.h"
#include "opengold/ua_view.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace opengold::por
{
namespace
{
const std::vector<std::uint8_t> &indexed(const ua::Library &library, const ua::IndexEntry &entry)
{
    return library.items.at(entry.item);
}

ua::Library required_library(const ua::DesignFiles &files, const std::string &name)
{
    auto library = files.library(name);
    if (!library)
    {
        throw EclError("Dark Queen needs " + name);
    }
    return std::move(*library);
}

std::vector<ua::IndexEntry> library_index(const ua::Library &library, const std::string &name)
{
    auto index = ua::decode_index(library);
    if (!index)
    {
        throw EclError("Dark Queen's " + name + " has no index");
    }
    return std::move(*index);
}

std::vector<AnimationFrame> picture_frames(const ua::TileLibrary &tiles, bool layered)
{
    auto palette = vga_color_table({});
    ua::apply_palette_glyph(palette, tiles);
    std::vector<AnimationFrame> frames;
    for (const auto &glyph : tiles.glyphs)
    {
        if (glyph && glyph->kind != ua::GlyphKind::palette && glyph->image.width &&
            glyph->image.height)
        {
            frames.push_back({4, ua::glyph_image(*glyph, palette)});
        }
    }
    if (layered && !frames.empty())
    {
        // PIC frame zero is the base; subsequent glyphs are drawn over that
        // same base, with their own origins and transparent pixels.
        const auto first =
            std::ranges::find_if(tiles.glyphs,
                                 [](const auto &glyph)
                                 {
                                     return glyph && glyph->kind != ua::GlyphKind::palette &&
                                            glyph->image.width && glyph->image.height;
                                 });
        std::size_t frame = 0;
        for (auto glyph = first; glyph != tiles.glyphs.end(); ++glyph)
        {
            if (!*glyph || (*glyph)->kind == ua::GlyphKind::palette || !(*glyph)->image.width ||
                !(*glyph)->image.height)
            {
                continue;
            }
            if (frame > 0)
            {
                const auto overlay = std::move(frames[frame].image);
                auto &canvas = frames[frame].image;
                canvas = frames.front().image;
                const int left = (*first)->origin_column - (*glyph)->origin_column;
                const int top = (*first)->origin_row - (*glyph)->origin_row;
                for (int y = 0; y < overlay.height; ++y)
                {
                    for (int x = 0; x < overlay.width; ++x)
                    {
                        const int dx = left + x, dy = top + y;
                        const auto source = (std::size_t(y) * overlay.width + x) * 4;
                        if (dx >= 0 && dy >= 0 && dx < canvas.width && dy < canvas.height &&
                            overlay.rgba[source + 3])
                        {
                            const auto target = (std::size_t(dy) * canvas.width + dx) * 4;
                            std::copy_n(overlay.rgba.begin() + source, 4,
                                        canvas.rgba.begin() + target);
                        }
                    }
                }
            }
            ++frame;
        }
    }
    return frames;
}
} // namespace

ua::FightField dark_queen_dungeon_field(const DarkQueenResources &resources, const GeoMap &map,
                                        PartyPose pose, unsigned set)
{
    const auto side = [&](int x, int y, unsigned direction)
    {
        if (x < 0 || y < 0 || x >= static_cast<int>(map.width) || y >= static_cast<int>(map.height))
        {
            return y == static_cast<int>(pose.y) && (direction == 1 || direction == 3)
                       ? FieldEdge::open
                       : FieldEdge::wall;
        }
        const auto &cell = map.at(static_cast<unsigned>(x), static_cast<unsigned>(y));
        if (!cell.walls[direction])
        {
            return FieldEdge::open;
        }
        return cell.doors[direction] ? FieldEdge::doorway : FieldEdge::wall;
    };
    const auto edges = [&](int column, int row, unsigned direction)
    {
        constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
        const int x = static_cast<int>(pose.x) + column, y = static_cast<int>(pose.y) + row;
        return static_cast<FieldEdge>(
            static_cast<unsigned>(side(x, y, direction)) |
            static_cast<unsigned>(side(x + dx[direction], y + dy[direction], (direction + 2) % 4)));
    };
    return ua::dungeon_field(resources.files, set, edges);
}

ua::WildernessStyle dark_queen_wilderness_style(unsigned terrain, bool river)
{
    // All 32 bytes match the installed executable's terrain-flag table.
    constexpr std::array<unsigned, 32> terrain_flags{
        0x00, 0x18, 0x11, 0x15, 0x01, 0x01, 0x60, 0x14, 0x08, 0x01, 0x00,
        0x21, 0x71, 0x09, 0x06, 0x04, 0x01, 0x09, 0x09, 0x08, 0x59, 0x00,
        0x11, 0x11, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00, 0x20, 0x20};
    if (terrain >= terrain_flags.size())
    {
        throw EclError("Invalid native wilderness terrain " + std::to_string(terrain));
    }
    const auto flags = terrain_flags[terrain];
    const auto adjustment = [flags](unsigned bit, int change)
    {
        return flags & bit ? change : 0;
    };
    ua::WildernessStyle style;
    style.river = river;
    style.art_tiles = 39;
    style.tree_chance = static_cast<unsigned>(std::max(
        1, 5 + adjustment(2, -3) + adjustment(4, -3) + adjustment(0x40, 2) + adjustment(8, 5)));
    const int density = 50 + adjustment(0x10, 10) + adjustment(0x20, 30) + adjustment(0x40, 20) +
                        adjustment(4, -10) + adjustment(2, -20);
    if (density < 10)
    {
        style.scatter = {0, 0, 0, 30, 15};
    }
    else if (density < 30)
    {
        style.scatter = {0, 1, 5, 20, 10};
    }
    else if (density >= 90)
    {
        style.scatter = {15, 5, 15, 10, 1};
    }
    else if (density >= 70)
    {
        style.scatter = {10, 2, 10, 10, 1};
    }
    return style;
}

RolfTourSession load_dark_queen_campaign(const std::filesystem::path &folder)
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    auto native = std::make_shared<DarkQueenResources>(folder);
    auto root = std::make_shared<PhlanResources>();
    root->dark_queen = native;
    root->bank = 1;
    root->opening_script = 4;
    root->map = GeoMap{};
    const auto frame_library = required_library(native->files, "FRAME.TLB");
    const auto frame = ua::decode_tile_library(frame_library);
    constexpr unsigned wilderness_marker = 29;
    if (!frame || frame->glyphs.size() <= wilderness_marker || !frame->glyphs[wilderness_marker])
    {
        throw EclError("Dark Queen's FRAME.TLB has no wilderness marker");
    }
    auto frame_palette = vga_color_table({});
    ua::apply_palette_glyph(frame_palette, *frame);
    root->area_map_marker = ua::glyph_image(*frame->glyphs[wilderness_marker], frame_palette);
    const auto raw_types = native->files.file_bytes(item_types_file(game));
    const auto image = native->files.file_bytes(item_words_program(game));
    if (!raw_types || !image)
    {
        throw EclError("Dark Queen needs ITEMS.DAT and DQK.EXE");
    }
    auto types = decode_item_templates(*raw_types, game);
    auto words = decode_item_words(*image, game);
    if (!types || !words)
    {
        throw EclError("Dark Queen's item types or words do not decode");
    }
    root->item_templates = std::move(*types);
    native->item_words = *words;
    const auto definitions = native->files.file_bytes("ITEM.DAT");
    const auto items = definitions ? dark_queen::decode_items(*definitions) : std::nullopt;
    if (!items)
    {
        throw EclError("Dark Queen's ITEM.DAT does not decode");
    }
    for (std::size_t number = 0; number < items->size(); ++number)
    {
        const auto &stored = items->at(number);
        if (stored.type == 255)
        {
            continue;
        }
        if (stored.type >= root->item_templates.size())
        {
            throw EclError("Invalid Dark Queen item type");
        }
        Equipment item;
        item.stored = stored;
        item.base = root->item_templates[stored.type];
        item.bonuses = equipment_bonuses(stored, item.base);
        item.decoded_name = item_name(stored, *words);
        item.unidentified_name = item_name(stored, *words, stored.hidden_name_words);
        root->item_definitions.emplace(static_cast<unsigned>(number + 1), std::move(item));
    }
    for (std::size_t shop = 0; shop < dark_queen_shops::shop_first.size(); ++shop)
    {
        std::vector<std::uint8_t> goods;
        for (auto index = dark_queen_shops::shop_first[shop];
             index <= dark_queen_shops::shop_last[shop]; ++index)
        {
            const auto number = dark_queen_shops::shop_items.at(index - 1);
            if (number == 255)
            {
                goods.push_back(static_cast<std::uint8_t>(number));
                continue;
            }
            const auto item = root->item_definitions.find(number);
            if (item != root->item_definitions.end() && item->second.stored.type != 30 &&
                item->second.stored.type != 12 && item->second.stored.type != 74)
            {
                goods.push_back(static_cast<std::uint8_t>(number));
            }
        }
        root->stores.push_back(std::move(goods));
    }
    root->creatures = std::make_shared<const CreatureCatalog>(CreatureCatalog::load(folder, game));
    for (const auto &[id, creature] : root->creatures->all())
    {
        root->encounter_creatures.emplace(id.record, creature);
    }
    const auto scripts = required_library(native->files, "ECL.GLB");
    std::vector<unsigned> script_ids;
    for (const auto &entry : library_index(scripts, "ECL.GLB"))
    {
        auto program = std::make_shared<const EclProgram>(EclProgram::decode(
            indexed(scripts, entry), "ECL.GLB:" + std::to_string(entry.number), game));
        root->programs.emplace(entry.number, std::move(program));
        root->script_banks.emplace(entry.number, 1);
        script_ids.push_back(entry.number);
    }
    if (!root->programs.contains(4))
    {
        throw EclError("Dark Queen's opening ECL 4 is missing");
    }
    for (const auto *name :
         {"PICA.TLB", "PICB.TLB", "PICC.TLB", "BIGPIC.TLB", "BACK.TLB", "SPRIT.TLB"})
    {
        const auto library = required_library(native->files, name);
        for (const auto &entry : library_index(library, name))
        {
            const bool layered = std::string_view{name}.starts_with("PIC");
            auto frames = picture_frames(
                ua::nested_tiles(library, entry.number,
                                 std::string(name) + ":" + std::to_string(entry.number)),
                layered);
            if (frames.empty())
            {
                continue;
            }
            if (std::string_view{name} == "BACK.TLB")
            {
                native->backdrops.emplace(entry.number, std::move(frames.front().image));
            }
            else if (std::string_view{name} == "SPRIT.TLB")
            {
                std::array<Image, 3> sprite;
                for (std::size_t distance = 0; distance < sprite.size(); ++distance)
                {
                    sprite[distance] = frames[std::min(distance, frames.size() - 1)].image;
                }
                root->shared_sprites.emplace(entry.number, std::move(sprite));
            }
            else if (std::string_view{name} == "BIGPIC.TLB")
            {
                if (entry.number >= 240)
                {
                    root->pictures.emplace(entry.number, frames.front().image);
                    root->animations.emplace(entry.number, frames);
                }
                native->big_pictures.emplace(entry.number, std::move(frames));
            }
            else
            {
                root->pictures.emplace(entry.number, frames.front().image);
                root->animations.emplace(entry.number, std::move(frames));
            }
        }
    }
    const auto walls_b = required_library(native->files, "8X8DB.TLB");
    const auto walls_c = required_library(native->files, "8X8DC.TLB");
    const auto areas = required_library(native->files, "GEO.GLB");
    for (const auto &entry : library_index(areas, "GEO.GLB"))
    {
        auto area = dark_queen::decode_area(indexed(areas, entry));
        if (!area)
        {
            throw EclError("Invalid native area " + std::to_string(entry.number));
        }
        auto district = std::make_shared<PhlanResources>();
        district->dark_queen = native;
        district->bank = 1;
        // Native scripts and maps are global libraries; NEW ECL retains the
        // current map until an explicit LOAD FILES names another one.
        district->shared_with = script_ids;
        GeoMap map;
        map.width = area->columns;
        map.height = area->rows;
        map.cells = std::move(area->cells);
        map.raw = std::move(area->raw);
        map.wrap_view = true;
        district->map = std::move(map);
        ua::ViewArt art;
        for (std::size_t slot = 0; slot < area->wall_sets.size(); ++slot)
        {
            const auto number = area->wall_sets[slot];
            if (number == 255)
            {
                continue;
            }
            auto tiles = ua::nested_tiles(number >= 10 ? walls_c : walls_b, number,
                                          "Dark Queen wall set " + std::to_string(number));
            ua::WallSet set;
            set.palette = vga_color_table({});
            ua::apply_palette_glyph(set.palette, tiles);
            set.glyphs = std::move(tiles.glyphs);
            art.sets[slot] = std::move(set);
        }
        district->wall_art = ua::wall_art(art);
        root->districts.emplace(entry.number, std::move(district));
    }
    auto opening = root->programs.at(4);
    return RolfTourSession(GeoMap{}, std::move(opening), {}, 0, {}, std::move(root));
}
} // namespace opengold::por
