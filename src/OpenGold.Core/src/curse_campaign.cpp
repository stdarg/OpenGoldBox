#include "opengold/campaign_session.h"

#include "area_loading.h"
#include "opengold/ecl_machine.h"
#include "opengold/map_catalog.h"
#include "opengold/por_sound.h"

#include <stdexcept>

namespace opengold
{
namespace
{

using por::EclError;

// Curse of the Azure Bonds' areas, as its scripts load them: script, disk
// and maps with their LOAD PIECES wall banks (docs/games/curse.md). The game
// starts with script 1 of disk 2, in the room the party wakes in.
const std::vector<por::detail::AreaSource> &curse_areas()
{
    static const std::vector<por::detail::AreaSource> areas{{1, 2, {{1, {1, 2, 3}}}}};
    return areas;
}

constexpr unsigned curse_opening_script = 1, curse_opening_disk = 2;

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
    auto root = std::make_shared<por::PhlanResources>();
    root->item_templates = *templates;
    root->bank = curse_opening_disk;
    root->opening_script = curse_opening_script;
    for (const auto &area : curse_areas())
    {
        const auto bank = std::to_string(area.bank);
        const auto program =
            catalog.find({"ECL" + bank + ".DAX", static_cast<std::uint8_t>(area.script)});
        if (!program)
        {
            throw EclError("Missing area script ECL" + bank + ":" + std::to_string(area.script));
        }
        root->programs.emplace(area.script, program);
        root->script_banks.emplace(area.script, area.bank);
        for (const auto &geo : area.maps)
        {
            root->districts.emplace(geo.id,
                                    por::detail::load_district(files, maps, area, geo, {}, {}));
        }
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
