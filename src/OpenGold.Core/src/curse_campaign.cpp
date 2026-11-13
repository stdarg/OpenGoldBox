#include "opengold/campaign_session.h"

#include "area_loading.h"
#include "opengold/creature_catalog.h"
#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/por_sound.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace opengold
{
namespace
{

using por::EclError;

constexpr unsigned curse_opening_script = 1, curse_opening_disk = 2;
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
    auto words = por::decode_item_words(por::unpack_sound_executable(
        por::detail::read_archive(por::detail::resolve_archive(folder, "START.EXE"))));
    if (!words)
    {
        throw EclError("START.EXE has no item name table");
    }
    const por::detail::AreaFiles files(folder, *templates, std::move(*words));
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
    auto root = std::make_shared<por::PhlanResources>();
    root->creatures = creatures;
    root->item_templates = *templates;
    root->bank = curse_opening_disk;
    root->opening_script = curse_opening_script;
    root->encounter_creatures = disk_creatures(curse_opening_disk);
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
        root->districts.emplace(map_id.record, std::move(district));
    }
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
