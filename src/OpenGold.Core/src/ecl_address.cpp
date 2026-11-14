#include "opengold/ecl_address.h"
#include "opengold/ecl.h"

#include <array>
#include <string>

namespace opengold::por
{
namespace
{

struct Zone
{
    std::uint16_t first{}, last{}; // Pool of Radiance's.
};

// Pool of Radiance's zones 0 to 2 (its GbvmZone), in the order of ZoneStarts.
constexpr std::array<Zone, 3> pool_of_radiance_zones{
    {{0x4900, 0x4CFF}, {0x6B00, 0x6EFF}, {0x9700, 0x98FF}}};

using ZoneStarts = std::array<std::uint16_t, 3>;

ZoneStarts zone_starts(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return {0x4900, 0x6B00, 0x9700};
    case GoldBoxGame::curse_of_the_azure_bonds:
    case GoldBoxGame::secret_of_the_silver_blades:
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return {0x4B00, 0x7C00, 0x7A00};
    default:
        throw EclError("The script memory of " + std::string(game_title(game)) +
                       " is not mapped yet");
    }
}

} // namespace

EclAddress game_address(EclAddress cell, GoldBoxGame game)
{
    const auto starts = zone_starts(game);
    for (std::size_t zone = 0; zone < pool_of_radiance_zones.size(); ++zone)
    {
        const auto [first, last] = pool_of_radiance_zones[zone];
        if (cell.location >= first && cell.location <= last)
        {
            return EclAddress(static_cast<std::uint16_t>(starts[zone] + (cell.location - first)));
        }
    }
    return cell;
}

EclAddress pool_of_radiance_address(EclAddress cell, GoldBoxGame game)
{
    const auto starts = zone_starts(game);
    for (std::size_t zone = 0; zone < pool_of_radiance_zones.size(); ++zone)
    {
        const auto [first, last] = pool_of_radiance_zones[zone];
        if (cell.location >= starts[zone] && cell.location <= starts[zone] + (last - first))
        {
            return EclAddress(static_cast<std::uint16_t>(first + (cell.location - starts[zone])));
        }
    }
    return cell;
}

} // namespace opengold::por
