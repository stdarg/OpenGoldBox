#include "opengold/ecl_address.h"
#include "opengold/ecl.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

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

// The 256-color engine (Pools of Darkness, Treasures of the Savage Frontier)
// keeps its script memory its own way (docs/games/darkness.md, "Script
// memory"): its cells 1h to 400h are the game's state, each named cell at its
// own place, and 401h to 800h are the current character's record, byte for
// byte. Each Pool of Radiance cell the engine names has its cell here.
using CellPair = std::pair<std::uint16_t, std::uint16_t>;
constexpr std::uint16_t vga_record = 0x401;
constexpr std::array<CellPair, 34> vga_cells{{
    {0x6DC9, 0x0003}, // FFh holds back the step a step's event began with.
    {0x49C5, 0x0013}, // The map LOAD FILES loaded.
    // The clock: minute ones and tens, hour, day, month and year.
    {0x49C7, 0x0006},
    {0x49C8, 0x0007},
    {0x49C9, 0x0008},
    {0x49CA, 0x0009},
    {0x49CB, 0x000A},
    {0x49CC, 0x000B},
    {0x49E6, 0x0022}, // In a dungeon (the 3D view), else on the area map.
    {0x49F0, 0x0017}, // The party's square before its last step.
    {0x49F1, 0x0018},
    {0x49F2, 0x0016}, // The area's script.
    // The current character: its name, its platinum, gems and jewelry (two
    // bytes each), its status, side and hit points.
    {0x6B00, vga_record},
    {0x6BC3, vga_record + 331},
    {0x6BC5, vga_record + 333},
    {0x6BC7, vga_record + 335},
    {0x6C00, vga_record + 493},
    {0x6C0C, vga_record + 495},
    {0x6C19, vga_record + 508},
    {0x6DA8, 0x0030}, // The classes a training hall trains, 127 for every class.
    {0x6DC8, 0x0015}, // Monsters defeated.
    {0x6DCA, 0x0019}, // Searching (bit 0) and looking (bit 1).
    {0x6DCB, 0x002E}, // Surprise: 1 the party, 2 the monsters.
    {0x6DD2, 0x002B}, // Camp's rest period and its chance of an interruption.
    {0x6DD3, 0x002C},
    {0x6DD5, 0x0021}, // 1 when the step goes off the map's edge.
    {0x6DE2, 0x0014}, // The temple opens at the next COMBAT.
    {0x6E33, 0x0036}, // A member attacked its own side.
    {0x6E3E, 0x0020}, // The party's size.
    {0xC04B, 0x0034}, // The party's square, facing, wall ahead and cell's event.
    {0xC04C, 0x0035},
    {0xC04D, 0x0011},
    {0xC04E, 0x0032},
    {0xC04F, 0x002D},
}};

// The engine's memory: its game cells, its character's record and its
// script, which runs to 9DFFh in Pools of Darkness and B6B0h in Treasures.
// A Pool of Radiance cell the engine has no place for keeps a place of its
// own that no script reads: its own address, or, where that is the engine's
// memory, the same cell moved up (low cells by F000h; script cells past the
// script's end, by 2000h in Darkness and 4000h in Treasures).
struct VgaMemory
{
    std::uint16_t script_last, script_spare;
};

constexpr std::uint16_t low_last = 0x0800, low_spare = 0xF000, script_first = 0x8000;

[[nodiscard]] constexpr VgaMemory vga_memory(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::treasures_of_the_savage_frontier ? VgaMemory{0xB6B0, 0x4000}
                                                                 : VgaMemory{0x9DFF, 0x2000};
}

[[nodiscard]] constexpr std::uint16_t vga_spare(std::uint16_t cell, VgaMemory memory) noexcept
{
    if (cell >= 0x0001 && cell <= low_last)
    {
        return static_cast<std::uint16_t>(cell + low_spare);
    }
    if (cell >= script_first && cell <= memory.script_last)
    {
        return static_cast<std::uint16_t>(cell + memory.script_spare);
    }
    return cell;
}

EclAddress vga_address(EclAddress cell, GoldBoxGame game)
{
    const auto named = std::ranges::find(vga_cells, cell.location, &CellPair::first);
    return EclAddress(named != vga_cells.end() ? named->second
                                               : vga_spare(cell.location, vga_memory(game)));
}

EclAddress vga_pool_of_radiance_address(EclAddress cell, GoldBoxGame game)
{
    const auto named = std::ranges::find(vga_cells, cell.location, &CellPair::second);
    if (named != vga_cells.end())
    {
        return EclAddress(named->first);
    }
    const auto memory = vga_memory(game);
    const auto moved_from = [&cell](std::uint16_t spare, std::uint16_t first, std::uint16_t last)
    {
        return cell.location >= first + spare && cell.location <= last + spare;
    };
    if (moved_from(low_spare, 0x0001, low_last))
    {
        return EclAddress(static_cast<std::uint16_t>(cell.location - low_spare));
    }
    if (moved_from(memory.script_spare, script_first, memory.script_last))
    {
        return EclAddress(static_cast<std::uint16_t>(cell.location - memory.script_spare));
    }
    return cell;
}

} // namespace

EclAddress game_address(EclAddress cell, GoldBoxGame game)
{
    if (vga_engine(game))
    {
        return vga_address(cell, game);
    }
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
    if (vga_engine(game))
    {
        return vga_pool_of_radiance_address(cell, game);
    }
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
