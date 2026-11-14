#include "opengold/campaign_session.h"

#include "area_loading.h"
#include "opengold/creature_catalog.h"
#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/por_sound.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace opengold
{
namespace
{

using por::EclError;

constexpr std::uint8_t load_files = 0x21, load_pieces = 0x37;

// What sets the campaigns after Pool of Radiance apart (docs/games/curse.md,
// docs/games/secret.md, docs/games/gateway.md).
struct LaterCampaign
{
    // A new game runs this script, on this disk.
    unsigned opening_script{}, opening_disk{};
    // The disk whose scripts play at the root, out of any map.
    unsigned root_disk{};
    // Curse's root disk 1 has no map: its scripts (80 to 82) are the
    // wilderness, fought on its own battlefield. Secret has none, and
    // Gateway's overland (script 21) loads its maps by cells.
    bool wilderness{};
};

LaterCampaign later_campaign(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return {1, 2, 1, true};
    case GoldBoxGame::secret_of_the_silver_blades:
        // The opening's vision of the valley has no map.
        return {17, 2, 2, false};
    case GoldBoxGame::gateway_to_the_savage_frontier:
        // A new game runs script 1 of disk 2 (Yartar); the overland's script
        // is on disk 6.
        return {1, 2, 6, false};
    case GoldBoxGame::pools_of_darkness:
    case GoldBoxGame::treasures_of_the_savage_frontier:
        // One disk: a new game runs script 16 (its GameLoop), New Phlan's
        // docks in Pools of Darkness, the waking in Llorkh's fortress in
        // Treasures.
        return {16, 1, 1, false};
    default:
        throw std::invalid_argument(std::string(game_title(game)) + " cannot be played yet");
    }
}

// What Curse's scripts load, read from the scripts themselves: a script's
// number is the same on every disk, its disk is the ECLn.DAX that holds it,
// and its LOAD FILES and LOAD PIECES name maps and wall banks of that disk.
struct ScriptLoads
{
    unsigned disk{};
    std::vector<unsigned> maps;
    std::vector<std::array<unsigned, 3>> walls;
    // A LOAD FILES whose map is a cell's (Gateway's overland, script 21, loads
    // the map its cell 4C24h names).
    bool maps_by_cell{};
};

// The 256-color engine's LOAD FILES names a map (below 7Fh) and one wall set
// (FFh keeps the current one), which fills the view's first slot and empties
// the others; Treasures' LOAD WALL SETS names a map and the three slots' sets.
void add_vga_loads(const por::EclInstruction &i, ScriptLoads &script)
{
    constexpr unsigned keep_walls = 255, first_non_map = 0x7F;
    constexpr std::uint8_t load_wall_sets = 0x42;
    const bool loads_files = i.opcode == load_files && i.operands.size() == 2;
    if (!loads_files && !(i.opcode == load_wall_sets && i.operands.size() == 4))
    {
        return;
    }
    const auto &map = i.operands[0];
    if (map.tag != 0)
    {
        script.maps_by_cell = true;
    }
    else if (map.value < first_non_map)
    {
        script.maps.push_back(map.value);
    }
    const bool literal_walls = std::all_of(i.operands.begin() + 1, i.operands.end(),
                                           [](const auto &operand)
                                           {
                                               return operand.tag == 0;
                                           });
    if (!literal_walls || (loads_files && i.operands[1].value == keep_walls))
    {
        return;
    }
    script.walls.push_back(
        loads_files ? std::array<unsigned, 3>{i.operands[1].value, por::detail::no_wall_bank,
                                              por::detail::no_wall_bank}
                    : std::array<unsigned, 3>{i.operands[1].value, i.operands[2].value,
                                              i.operands[3].value});
}

std::map<unsigned, ScriptLoads> script_loads(const por::EclCatalog &catalog, GoldBoxGame game)
{
    std::map<unsigned, ScriptLoads> loads;
    // Treasures' demonstration (script 4, which its original runs when the
    // title is left alone) loads the opening's map and the overland's; the
    // game's own scripts hold them.
    constexpr unsigned treasures_demonstration = 4;
    for (const auto &[id, program] : catalog.all())
    {
        auto &script = loads[id.record];
        script.disk = static_cast<unsigned>(std::stoul(id.archive.substr(3)));
        if (game == GoldBoxGame::treasures_of_the_savage_frontier &&
            id.record == treasures_demonstration)
        {
            continue;
        }
        for (const auto &i : por::walk_reachable(*program).instructions)
        {
            if (vga_engine(game))
            {
                add_vga_loads(i, script);
                continue;
            }
            if (i.opcode == load_files && !i.operands.empty() && i.operands[0].tag != 0)
            {
                script.maps_by_cell = true;
            }
            const bool written = std::all_of(i.operands.begin(), i.operands.end(),
                                             [](const auto &operand)
                                             {
                                                 return operand.tag == 0;
                                             });
            if (!written || i.operands.size() != 3)
            {
                continue;
            }
            const std::array<unsigned, 3> values{i.operands[0].value, i.operands[1].value,
                                                 i.operands[2].value};
            if (i.opcode == load_files && values[0] != 255 && values[0] != 127)
            {
                script.maps.push_back(values[0]);
            }
            if (i.opcode == load_pieces && values[0] != 127)
            {
                script.walls.push_back(por::detail::load_pieces_banks(values));
            }
        }
    }
    return loads;
}

// Where a release's ending (PROGRAM 8; its WinGame) keeps its lines: GAME.OVR's
// Pascal strings at a fixed place, four lines a screen. They are read from the
// player's copy, never stored here; another release yields none.
struct EndingStrings
{
    std::size_t release_size, offset, strings;
};

// Curse's ending, told when Tyranthraxus falls: six screens, its "Press any
// key" prompt among them.
constexpr EndingStrings curse_ending{272137, 0x265c6, 25};
// Gateway's, told when the statuettes are set in Ascore's altar: six screens,
// each ending with its "(More)" prompt.
constexpr EndingStrings gateway_ending{347622, 0x2cfa8, 24};

std::vector<std::string> read_ending(std::span<const std::uint8_t> overlay,
                                     const EndingStrings &release)
{
    constexpr std::size_t lines_a_screen = 4;
    constexpr std::string_view more = "(More)";
    if (overlay.size() != release.release_size)
    {
        return {};
    }
    std::vector<std::string> screens(1);
    std::size_t lines = 0;
    auto at = release.offset;
    for (std::size_t n = 0; n < release.strings; ++n)
    {
        const std::size_t length = overlay[at];
        if (length == 0 || at + 1 + length > overlay.size())
        {
            return {};
        }
        std::string line(overlay.begin() + static_cast<std::ptrdiff_t>(at) + 1,
                         overlay.begin() + static_cast<std::ptrdiff_t>(at + 1 + length));
        if (!std::all_of(line.begin(), line.end(),
                         [](char c)
                         {
                             return c >= ' ' && c <= '~';
                         }))
        {
            return {};
        }
        at += 1 + length;
        if (line.starts_with("Press any key"))
        {
            continue;
        }
        if (line.ends_with(more))
        {
            line.erase(line.find_last_not_of(' ', line.size() - more.size() - 1) + 1);
        }
        if (lines == lines_a_screen)
        {
            screens.emplace_back();
            lines = 0;
        }
        // Curse's lines end with the space that joins them; Gateway's do not,
        // and a word broken at a line's end is joined whole.
        auto &screen = screens.back();
        if (screen.ends_with('-'))
        {
            screen.pop_back();
        }
        else if (!screen.empty() && !screen.ends_with(' '))
        {
            screen += ' ';
        }
        screen += line;
        ++lines;
    }
    return screens;
}

// Fights out of doors (the script's 3D-area cell clear) are on open ground
// drawn with WILDCOM's 34 tiles, then RANDCOM's 6.
void add_wilderness_art(const std::filesystem::path &folder, por::PhlanResources &root)
{
    for (const auto &[name, frames] : {std::pair{"WILDCOM.DAX", 34u}, std::pair{"RANDCOM.DAX", 6u}})
    {
        const auto archive = por::detail::read_archive(por::detail::resolve_archive(folder, name));
        for (unsigned frame = 0; frame < frames; ++frame)
        {
            auto art = decode_ega_combat_icon(archive, 1, static_cast<std::uint8_t>(frame));
            if (!art)
            {
                throw EclError(std::string("Invalid wilderness battlefield tile in ") + name);
            }
            root.wilderness_art.push_back(std::move(art.image));
        }
    }
}

void add_curse_wilderness(const std::filesystem::path &folder,
                          std::span<const std::uint8_t> start_image, por::PhlanResources &root)
{
    root.curse_wilderness = por::curse_wilderness(start_image);
    if (!root.curse_wilderness)
    {
        throw EclError("Curse's START.EXE has no wilderness battlefield tables");
    }
    add_wilderness_art(folder, root);
}

// Gateway's fights on its overland, from its GAME.EXE's terrain tables.
void add_gateway_wilderness(const std::filesystem::path &folder,
                            std::span<const std::uint8_t> game_image, por::PhlanResources &root)
{
    root.gateway_wilderness = por::gateway_wilderness(game_image);
    if (!root.gateway_wilderness)
    {
        throw EclError("Gateway's GAME.EXE has no wilderness battlefield tables");
    }
    add_wilderness_art(folder, root);
}

// Pools of Darkness's eleven stores, as its program lists them (its EclShop):
// each store's first and last slot, eleven of each, then the slots' item
// definitions. They are found in the player's GAME.EXE by their shape, the
// first store starting at slot 1 and each one after the last; FFh is a scroll
// made at random.
std::vector<std::vector<std::uint8_t>> darkness_stores(std::span<const std::uint8_t> game_image)
{
    constexpr std::size_t store_count = 11;
    for (std::size_t at = 0; at + 2 * store_count < game_image.size(); ++at)
    {
        const auto first = game_image.subspan(at, store_count);
        const auto last = game_image.subspan(at + store_count, store_count);
        bool shaped = first[0] == 1;
        for (std::size_t n = 0; shaped && n < store_count; ++n)
        {
            shaped = last[n] >= first[n] && (n + 1 == store_count || first[n + 1] == last[n] + 1);
        }
        const std::size_t slots = last[store_count - 1];
        const auto items_at = at + 2 * store_count;
        if (!shaped || items_at + slots > game_image.size())
        {
            continue;
        }
        std::vector<std::vector<std::uint8_t>> stores;
        for (std::size_t n = 0; n < store_count; ++n)
        {
            const auto from =
                game_image.begin() + static_cast<std::ptrdiff_t>(items_at + first[n] - 1);
            stores.emplace_back(from, from + (last[n] - first[n] + 1));
        }
        return stores;
    }
    return {};
}

// Treasures' sixteen stores (its EclShop): runs of item definitions, each
// ended by FFh, at a fixed place in the unpacked GAME.EXE of the release this
// engine reads, whose first store sells item 1 first. Item 0 sells nothing.
std::vector<std::vector<std::uint8_t>> treasures_stores(std::span<const std::uint8_t> game_image)
{
    constexpr std::size_t table = 0xC3E2, longest = 256, store_count = 16;
    constexpr std::uint8_t end_of_run = 0xFF;
    if (game_image.size() < table + longest || game_image[table] != 1)
    {
        return {};
    }
    std::vector<std::vector<std::uint8_t>> stores(1);
    for (std::size_t at = table; at < table + longest && stores.size() <= store_count; ++at)
    {
        if (game_image[at] == end_of_run)
        {
            stores.emplace_back();
            continue;
        }
        stores.back().push_back(game_image[at]);
    }
    stores.pop_back();
    return stores.size() == store_count ? stores : std::vector<std::vector<std::uint8_t>>{};
}

// Treasures' overland (docs/games/treasure.md, The overland): each map's
// picture (OVER 51 to 57) in the colors of each record that only sets colors
// (1 to 4), which the cell 42h picks, and the party's arrow on it (the
// automap's symbols 16 to 19, north first).
void add_treasures_overland(const por::detail::AreaFiles &files, por::PhlanResources &root)
{
    std::map<unsigned, VgaPicture> pictures;
    for (const auto &record : files.archive("OVER.DAX").records)
    {
        auto picture = decode_vga_picture(record.bytes);
        if (!picture)
        {
            throw EclError("Invalid overland picture " + std::to_string(record.id));
        }
        pictures.emplace(record.id, std::move(*picture));
    }
    const auto colors_only = [](const VgaPicture &picture)
    {
        return picture.frames.empty() || picture.frames.front().width == 0;
    };
    for (const auto &[map, picture] : pictures)
    {
        for (const auto &[palette, colors] : pictures)
        {
            if (!colors_only(picture) && colors_only(colors))
            {
                root.overland_pictures.emplace(
                    map * 256 + palette,
                    vga_rgba(picture.frames.front(), vga_color_table(colors.palette)));
            }
        }
    }
    constexpr unsigned automap_symbols = 202, first_arrow = 16;
    constexpr std::uint8_t see_through = 13;
    const auto symbols = decode_vga_picture(files.record("8X8D0.DAX", automap_symbols));
    if (!symbols || symbols->frames.size() < first_arrow + root.overland_arrows.size())
    {
        throw EclError("8X8D0.DAX has no party arrows");
    }
    const auto symbol_colors = vga_color_table(symbols->palette);
    for (std::size_t facing = 0; facing < root.overland_arrows.size(); ++facing)
    {
        root.overland_arrows[facing] =
            vga_rgba(symbols->frames[first_arrow + facing], symbol_colors, see_through);
    }
}

// Every set of symbols in the files a disk's LOAD PIECES reads, the file
// without a disk number first, as the original looks there first.
void add_symbol_sets(const por::detail::AreaFiles &files, unsigned disk, por::PhlanResources &root)
{
    for (const auto &name : files.disk_archives("8X8D", disk))
    {
        if (!files.has_archive(name))
        {
            continue;
        }
        for (const auto &record : files.archive(name).records)
        {
            auto tiles = por::decode_wall_tiles(record.bytes);
            if (!tiles)
            {
                throw EclError("Invalid symbol set " + std::to_string(record.id) + " in " + name);
            }
            root.symbol_sets.emplace(record.id, std::move(*tiles));
        }
    }
    auto marks = por::decode_wall_tiles(files.record("8X8D1.DAX", por::gateway_marks));
    if (!marks)
    {
        throw EclError("Invalid symbol set " + std::to_string(por::gateway_marks) +
                       " in 8X8D1.DAX");
    }
    root.symbol_sets.emplace(por::gateway_marks, std::move(*marks));
}

por::RolfTourSession load_later_campaign(const std::filesystem::path &folder, GoldBoxGame game)
{
    const auto campaign = later_campaign(game);
    const auto catalog = por::EclCatalog::load(folder, game);
    const auto disk = std::to_string(campaign.opening_disk);
    const auto opening =
        catalog.find({"ECL" + disk + ".DAX", static_cast<std::uint8_t>(campaign.opening_script)});
    if (!opening)
    {
        throw EclError(std::string(game_title(game)) + " needs ECL" + disk + ".DAX record " +
                       std::to_string(campaign.opening_script));
    }
    const auto templates = por::decode_item_templates(
        por::detail::read_archive(por::detail::resolve_archive(folder, item_types_file(game))),
        game);
    if (!templates)
    {
        throw EclError("Invalid item templates");
    }
    // The program holding the item words and the game's tables: START.EXE, or
    // the later games' GAME.EXE.
    const bool treasures = game == GoldBoxGame::treasures_of_the_savage_frontier;
    const auto program = item_words_program(game);
    const auto program_image = por::unpack_sound_executable(
        por::detail::read_archive(por::detail::resolve_archive(folder, program)));
    auto words = por::decode_item_words(program_image, game);
    if (!words)
    {
        throw EclError(std::string(program) + " has no item name table");
    }
    const por::detail::AreaFiles files(folder, *templates, std::move(*words), game);
    const auto maps = por::MapCatalog::load(folder);
    const auto creatures =
        std::make_shared<const por::CreatureCatalog>(por::CreatureCatalog::load(folder, game));
    // Every creature record of a disk, which its scripts' LOAD MONSTER and ADD
    // NPC name by number; these games fight them as their records (the
    // original rules), with no conversions. Secret's one file serves them all.
    const auto creature_disks = game_disks(game).monsters;
    const auto disk_creatures = [&creatures, &creature_disks](unsigned disk)
    {
        const auto bank = creature_disks.size() == 1 ? creature_disks.front() : disk;
        std::map<unsigned, por::Creature> found;
        for (const auto &[id, creature] : creatures->all())
        {
            if (id.bank == bank)
            {
                found.emplace(id.record, creature);
            }
        }
        return found;
    };
    // The root plays the scripts out of any map (Curse's wilderness, Secret's
    // opening), with their disk's creatures, encounter sprites, combat icons
    // and treasure.
    auto root = std::make_shared<por::PhlanResources>();
    const auto root_disk = std::to_string(campaign.root_disk);
    root->creatures = creatures;
    root->item_templates = *templates;
    root->bank = campaign.root_disk;
    root->opening_script = campaign.opening_script;
    root->encounter_creatures = disk_creatures(campaign.root_disk);
    root->map = por::GeoMap{};
    // Pools of Darkness's TREASURE and STORE name ITEM0's definitions: it has
    // no lists.
    if (vga_engine(game))
    {
        constexpr auto definitions = "ITEM0.DAX";
        root->item_definitions =
            files.item_definitions(definitions, files.archive(definitions).records.at(0).id);
        root->stores = treasures ? treasures_stores(program_image) : darkness_stores(program_image);
        if (root->stores.empty())
        {
            throw EclError(std::string(game_title(game)) + "'s GAME.EXE has no store lists");
        }
    }
    else
    {
        root->treasure = files.item_lists("ITEM" + root_disk + ".DAX");
    }
    // Gateway has no sprite files.
    if (game != GoldBoxGame::gateway_to_the_savage_frontier)
    {
        root->sprite_archive = por::detail::read_archive(
            por::detail::resolve_archive(folder, files.disk_archive("SPRIT", campaign.root_disk)));
    }
    root->combat_archive = por::detail::read_archive(
        por::detail::resolve_archive(folder, files.disk_archive("CPIC", campaign.root_disk)));
    if (game == GoldBoxGame::curse_of_the_azure_bonds ||
        game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        root->closing_story = read_ending(
            por::detail::read_archive(por::detail::resolve_archive(folder, "GAME.OVR")),
            game == GoldBoxGame::curse_of_the_azure_bonds ? curse_ending : gateway_ending);
    }
    if (campaign.wilderness)
    {
        add_curse_wilderness(folder, program_image, *root);
    }
    if (game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        add_gateway_wilderness(folder, program_image, *root);
    }
    if (treasures)
    {
        add_treasures_overland(files, *root);
    }
    const auto loads = script_loads(catalog, game);
    for (const auto &[id, program] : catalog.all())
    {
        root->programs.emplace(id.record, program);
        root->script_banks.emplace(id.record, loads.at(id.record).disk);
    }
    for (const auto &[script, load] : loads)
    {
        // Gateway's overland script (21) loads symbol sets of its disk alone
        // for the overland's map (view3d.pas LoadWallSet outside a dungeon).
        if (game == GoldBoxGame::gateway_to_the_savage_frontier && load.maps_by_cell)
        {
            add_symbol_sets(files, load.disk, *root);
        }
        for (const auto &walls : load.walls)
        {
            const auto key = std::make_pair(load.disk, walls);
            if (!root->wall_sets.contains(key))
            {
                root->wall_sets.emplace(
                    key, vga_engine(game)
                             ? por::detail::load_vga_wall_art(files, walls)
                             : por::detail::load_wall_art(files, load.disk, walls, {}));
            }
        }
    }
    // Every disk's pictures and animations, for every map; Gateway's files
    // without a disk number come first, as its original looks there first.
    std::vector<std::string> picture_files;
    if (game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        picture_files = {"PIC.DAX", "BIGPIC.DAX"};
    }
    for (unsigned picture_disk = 1; picture_disk <= 6; ++picture_disk)
    {
        for (const auto *family : {"PIC", "BIGPIC"})
        {
            picture_files.push_back(family + std::to_string(picture_disk) + ".DAX");
        }
    }
    // Treasures looks for a picture in PIC1, then PIC1A.
    if (treasures)
    {
        picture_files.emplace_back("PIC1A.DAX");
    }
    std::map<unsigned, Image> pictures;
    std::map<unsigned, std::vector<AnimationFrame>> animations;
    for (const auto &name : picture_files)
    {
        if (!std::filesystem::exists(folder / name))
        {
            continue;
        }
        files.pictures(name, pictures);
        files.animations(name, animations);
    }
    // Each map once, held by the scripts that load it; its walls until a
    // script's LOAD PIECES replaces them are the first such script's.
    for (const auto &[map_id, map] : maps.all())
    {
        const auto map_disk = static_cast<unsigned>(std::stoul(map_id.archive.substr(3)));
        std::vector<unsigned> scripts;
        std::array<unsigned, 3> walls{por::detail::no_wall_bank, por::detail::no_wall_bank,
                                      por::detail::no_wall_bank};
        for (const auto &[script, load] : loads)
        {
            if (load.disk == map_disk &&
                std::find(load.maps.begin(), load.maps.end(), map_id.record) != load.maps.end())
            {
                if (scripts.empty() && !load.walls.empty())
                {
                    walls = load.walls.front();
                }
                scripts.push_back(script);
            }
        }
        // A map no script names is one a script of its disk loads by a cell
        // (Gateway's overland maps 21 to 26); its walls are the ones that
        // script's LOAD PIECES names as it runs.
        if (scripts.empty())
        {
            for (const auto &[script, load] : loads)
            {
                if (load.disk == map_disk && load.maps_by_cell)
                {
                    scripts.push_back(script);
                }
            }
        }
        if (scripts.empty())
        {
            continue;
        }
        const por::detail::AreaSource area{scripts.front(), map_disk, {}};
        const por::detail::DistrictMap geo{
            map_id.record, walls, {scripts.begin() + 1, scripts.end()}};
        auto district = por::detail::load_district(files, maps, area, geo, {}, {});
        district->encounter_creatures = disk_creatures(map_disk);
        // Pictures are numbered across the disks, so a map's script shows
        // other disks' too (map 21, on disk 3, shows disk 1's picture 80, and
        // the big pictures 120 to 123 are on disks 1, 2 and 6). Its own disk's
        // come first.
        district->pictures.insert(pictures.begin(), pictures.end());
        district->animations.insert(animations.begin(), animations.end());
        root->districts.emplace(map_id.record, std::move(district));
    }
    root->pictures = pictures;
    root->animations = animations;
    return por::RolfTourSession(por::GeoMap{}, opening, {}, 0, {}, std::move(root));
}

} // namespace

por::RolfTourSession load_campaign(const std::filesystem::path &folder, GoldBoxGame game,
                                   const CampaignData &data)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        if (!data.encounters)
        {
            throw std::invalid_argument("Pool of Radiance's campaign needs its encounter data");
        }
        return por::RolfTourSession::load(
            folder, *data.encounters,
            data.new_wall_art ? *data.new_wall_art : por::WallArtReplacements{}, data.monsters);
    case GoldBoxGame::curse_of_the_azure_bonds:
    case GoldBoxGame::secret_of_the_silver_blades:
    case GoldBoxGame::gateway_to_the_savage_frontier:
    case GoldBoxGame::pools_of_darkness:
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return load_later_campaign(folder, game);
    default:
        throw std::invalid_argument(std::string(game_title(game)) + " cannot be played yet");
    }
}

} // namespace opengold
