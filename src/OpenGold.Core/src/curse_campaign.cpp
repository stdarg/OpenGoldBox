#include "opengold/campaign_session.h"

#include "area_loading.h"
#include "opengold/creature_catalog.h"
#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/por_sound.h"

#include <algorithm>
#include <map>
#include <span>
#include <stdexcept>
#include <string>

namespace opengold
{
namespace
{

using por::EclError;

constexpr unsigned curse_opening_script = 1, curse_opening_disk = 2;
// Disk 1 has no map: its scripts (80 to 82) are the wilderness.
constexpr unsigned curse_wilderness_disk = 1;
constexpr std::uint8_t load_files = 0x21, load_pieces = 0x37;

// What Curse's scripts load, read from the scripts themselves: a script's
// number is the same on every disk, its disk is the ECLn.DAX that holds it,
// and its LOAD FILES and LOAD PIECES name maps and wall banks of that disk.
struct ScriptLoads
{
    unsigned disk{};
    std::vector<unsigned> maps;
    std::vector<std::array<unsigned, 3>> walls;
};

std::map<unsigned, ScriptLoads> script_loads(const por::EclCatalog &catalog)
{
    std::map<unsigned, ScriptLoads> loads;
    for (const auto &[id, program] : catalog.all())
    {
        auto &script = loads[id.record];
        script.disk = static_cast<unsigned>(std::stoul(id.archive.substr(3)));
        for (const auto &i : por::walk_reachable(*program).instructions)
        {
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
                script.walls.push_back(values);
            }
        }
    }
    return loads;
}

// Curse's ending, told when Tyranthraxus falls (PROGRAM 8; its WinGame):
// GAME.OVR's Pascal strings at a fixed place in the release this engine reads,
// six screens of four lines, its "Press any key" prompt among them. It is read
// from the player's copy, never stored here; another release yields none.
std::vector<std::string> read_curse_ending(std::span<const std::uint8_t> overlay)
{
    constexpr std::size_t release_size = 272137, ending_offset = 0x265c6, strings = 25,
                          lines_a_screen = 4;
    if (overlay.size() != release_size)
    {
        return {};
    }
    std::vector<std::string> screens(1);
    std::size_t lines = 0;
    auto at = ending_offset;
    for (std::size_t n = 0; n < strings; ++n)
    {
        const std::size_t length = overlay[at];
        if (length == 0 || at + 1 + length > overlay.size())
        {
            return {};
        }
        const std::string line(overlay.begin() + static_cast<std::ptrdiff_t>(at) + 1,
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
        if (lines == lines_a_screen)
        {
            screens.emplace_back();
            lines = 0;
        }
        // Its lines end with the space that joins them.
        screens.back() +=
            (screens.back().empty() || screens.back().ends_with(' ') ? "" : " ") + line;
        ++lines;
    }
    return screens;
}

por::RolfTourSession load_curse_campaign(const std::filesystem::path &folder)
{
    constexpr auto game = GoldBoxGame::curse_of_the_azure_bonds;
    const auto catalog = por::EclCatalog::load(folder, game);
    const auto disk = std::to_string(curse_opening_disk);
    const auto opening = catalog.find({"ECL" + disk + ".DAX", curse_opening_script});
    if (!opening)
    {
        throw EclError("Curse of the Azure Bonds needs ECL" + disk + ".DAX record " +
                       std::to_string(curse_opening_script));
    }
    const auto templates = por::decode_item_templates(
        por::detail::read_archive(por::detail::resolve_archive(folder, "ITEMS")));
    if (!templates)
    {
        throw EclError("Invalid item templates");
    }
    const auto start_image = por::unpack_sound_executable(
        por::detail::read_archive(por::detail::resolve_archive(folder, "START.EXE")));
    auto words = por::decode_item_words(start_image);
    if (!words)
    {
        throw EclError("START.EXE has no item name table");
    }
    const por::detail::AreaFiles files(folder, *templates, std::move(*words),
                                       por::detail::AreaFiles::StoredNames::dropped);
    const auto maps = por::MapCatalog::load(folder);
    const auto creatures =
        std::make_shared<const por::CreatureCatalog>(por::CreatureCatalog::load(folder, game));
    // Every creature record of a disk, which its scripts' LOAD MONSTER and ADD
    // NPC name by number; Curse fights them as their records (the original
    // rules), with no conversions.
    const auto disk_creatures = [&creatures](unsigned disk)
    {
        std::map<unsigned, por::Creature> found;
        for (const auto &[id, creature] : creatures->all())
        {
            if (id.bank == disk)
            {
                found.emplace(id.record, creature);
            }
        }
        return found;
    };
    // The root is the wilderness: its scripts play there, with their disk's
    // creatures, encounter sprites, combat icons and treasure.
    auto root = std::make_shared<por::PhlanResources>();
    const auto wilderness = std::to_string(curse_wilderness_disk);
    root->creatures = creatures;
    root->item_templates = *templates;
    root->bank = curse_wilderness_disk;
    root->opening_script = curse_opening_script;
    root->encounter_creatures = disk_creatures(curse_wilderness_disk);
    root->map = por::GeoMap{};
    root->treasure = files.item_lists("ITEM" + wilderness + ".DAX");
    root->sprite_archive = por::detail::read_archive(
        por::detail::resolve_archive(folder, "SPRIT" + wilderness + ".DAX"));
    root->combat_archive = por::detail::read_archive(
        por::detail::resolve_archive(folder, "CPIC" + wilderness + ".DAX"));
    root->closing_story = read_curse_ending(
        por::detail::read_archive(por::detail::resolve_archive(folder, "GAME.OVR")));
    // Fights out of doors (the script's 3D-area cell clear) are on open
    // ground drawn with WILDCOM's 34 tiles, then RANDCOM's 6.
    root->curse_wilderness = por::curse_wilderness(start_image);
    if (!root->curse_wilderness)
    {
        throw EclError("Curse's START.EXE has no wilderness battlefield tables");
    }
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
            root->wilderness_art.push_back(std::move(art.image));
        }
    }
    const auto loads = script_loads(catalog);
    for (const auto &[id, program] : catalog.all())
    {
        root->programs.emplace(id.record, program);
        root->script_banks.emplace(id.record, loads.at(id.record).disk);
    }
    for (const auto &[script, load] : loads)
    {
        for (const auto &walls : load.walls)
        {
            const auto key = std::make_pair(load.disk, walls);
            if (!root->wall_sets.contains(key))
            {
                root->wall_sets.emplace(key,
                                        por::detail::load_wall_art(files, load.disk, walls, {}));
            }
        }
    }
    // Every disk's pictures and animations, for every map.
    std::map<unsigned, Image> pictures;
    std::map<unsigned, std::vector<AnimationFrame>> animations;
    for (unsigned picture_disk = 1; picture_disk <= 6; ++picture_disk)
    {
        for (const auto *family : {"PIC", "BIGPIC"})
        {
            const auto name = family + std::to_string(picture_disk) + ".DAX";
            if (!std::filesystem::exists(folder / name))
            {
                continue;
            }
            files.pictures(name, pictures);
            for (const auto &record : files.archive(name).records)
            {
                if (auto animation = decode_ega_animation(record.bytes))
                {
                    animations.emplace(record.id, std::move(animation.frames));
                }
            }
        }
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
        return load_curse_campaign(folder);
    default:
        throw std::invalid_argument(std::string(game_title(game)) + " cannot be played yet");
    }
}

} // namespace opengold
