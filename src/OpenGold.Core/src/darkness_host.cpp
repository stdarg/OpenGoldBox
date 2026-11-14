// Pools of Darkness's script commands the session answers where its numbers
// mean other commands in the other games (docs/games/darkness.md, Scripts).
#include "opengold/rolf_tour.h"
#include "area_loading.h"

namespace opengold::por
{
namespace
{

// The map number from which LOAD FILES loads no map, and the wall set that
// keeps the current one.
constexpr unsigned first_non_map = 0x7F, keep_walls = 255;
// PLAY SOUND's footstep, and its music from 80h up.
constexpr unsigned footstep_sound = 10;

} // namespace

EclHostReply RolfTourSession::darkness_load_files(unsigned map, unsigned walls)
{
    EclHostReply reply;
    // Indoors only: the area map keeps its own picture (its EclLoadFiles).
    if (map < first_non_map && machine_.variable(game_cell(ecl_in_3d_area)) != 0)
    {
        if (!town_->districts.contains(map))
        {
            throw EclError("Resources outside the supported areas (map " + std::to_string(map) +
                           ")");
        }
        change_area(map);
        const auto &here = map_.at(machine_.variable(game_cell(ecl_party_x)),
                                   machine_.variable(game_cell(ecl_party_y)));
        const auto facing = machine_.variable(game_cell(ecl_party_facing));
        reply.writes = {{game_cell(ecl_wall_ahead), here.walls[facing % 4]},
                        {game_cell(ecl_cell_event), here.event_raw},
                        {game_cell(ecl_current_map), static_cast<std::uint16_t>(map)}};
    }
    if (walls != keep_walls)
    {
        const auto set = town_->wall_sets.find(
            {town_->bank, {walls, detail::no_wall_bank, detail::no_wall_bank}});
        if (set == town_->wall_sets.end())
        {
            throw EclError("Unknown wall set " + std::to_string(walls));
        }
        wall_art_ = set->second;
        ++snapshot_.revision;
    }
    return reply;
}

void RolfTourSession::darkness_play_sound(unsigned sound)
{
    // The footstep plays as the other games' does; the other sound effects
    // and the music are not played yet.
    if (sound == footstep_sound)
    {
        ++snapshot_.footsteps;
    }
}

} // namespace opengold::por
