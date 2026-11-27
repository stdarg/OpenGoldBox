// The Buck Rogers games' solar map in the session (docs/games/countdown.md,
// "The solar map"): the rocket's state in zone 0, its steps between the
// script's step and search entries, and the map drawn as the original draws
// it. The rules are original::step_rocket and place_bodies
// (buck_rogers_solar_map.h); the fuel, the arrival and what a body offers
// are the solar map's script's.
#include "opengold/rolf_tour.h"
#include "area_loading.h"

#include <algorithm>
#include <chrono>

namespace opengold::por
{
namespace
{
// Zone 0's solar map (types.pas TOrbitView): each body's cell, two bytes a
// body from 4B85h; the body the script found the rocket on; the days into
// the phase, the phase, the rocket's tile and its cell.
constexpr std::uint16_t first_body_cell = 0x4B85;
constexpr EclAddress planet_cell{0x4BA5}, days_cell{0x4BB9}, phase_cell{0x4BBB},
    rocket_tile_cell{0x4BBD}, rocket_x_cell{0x4BBE}, rocket_y_cell{0x4BBF};
// Zone 1's moveBlocked: FFh holds the rocket where it is for a step
// (explore.pas ResolveMovement).
constexpr EclAddress move_blocked_cell{0x7EC9};
constexpr std::uint16_t move_blocked = 0xFF;
// The team's ship in zone 0 (types.pas TShip): its name, a length byte and
// fifteen characters, then words: each system as it is now from 16h, and
// whole from 22h.
constexpr std::uint16_t ship_cell = 0x4D00, ship_name_length = 15, systems_now = 0x16,
                        systems_whole = 0x22;
// A cell of the map is 8 pixels of its backdrop.
constexpr unsigned cell_pixels = 8;

// The rocket's heading for a step the explore() command names, which run
// from north clockwise.
[[nodiscard]] original::RocketHeading heading_of(ExplorationCommand command)
{
    using original::RocketHeading;
    static constexpr std::array clockwise{RocketHeading::north, RocketHeading::northeast,
                                          RocketHeading::east,  RocketHeading::southeast,
                                          RocketHeading::south, RocketHeading::southwest,
                                          RocketHeading::west,  RocketHeading::northwest};
    return clockwise.at(static_cast<std::size_t>(command) -
                        static_cast<std::size_t>(ExplorationCommand::north));
}
} // namespace

original::SolarMapState BuckRogersHost::solar_map_state(const TourHost &tour) const
{
    const auto tile = tour.script_machine().variable(rocket_tile_cell);
    const auto first = original::rocket_tile(original::RocketHeading::north);
    const auto heading = tile >= first && tile < first + 8
                             ? static_cast<original::RocketHeading>(tile - first)
                             : original::RocketHeading::north;
    return {tour.script_machine().variable(phase_cell),
            tour.script_machine().variable(days_cell),
            heading,
            {tour.script_machine().variable(rocket_x_cell),
             tour.script_machine().variable(rocket_y_cell)}};
}

std::optional<std::size_t> BuckRogersHost::orbited_body(const TourHost &tour) const
{
    const auto planet = tour.script_machine().variable(planet_cell);
    if (planet == 0 || planet > original::solar_bodies)
    {
        return std::nullopt;
    }
    return planet - 1U;
}

TeamShip BuckRogersHost::team_ship(const TourHost &tour) const
{
    const auto at = [&tour](std::uint16_t offset)
    {
        return tour.script_machine().variable(
            EclAddress{static_cast<std::uint16_t>(ship_cell + offset)});
    };
    TeamShip ship;
    const auto length = std::min<unsigned>(at(0) & 0xFF, ship_name_length);
    for (std::uint16_t n = 1; n <= length; ++n)
    {
        ship.name += static_cast<char>(at(n) & 0xFF);
    }
    for (std::uint16_t n = 0; n < ship.systems.size(); ++n)
    {
        ship.systems[n] = {at(static_cast<std::uint16_t>(systems_now + 2 * n)),
                           at(static_cast<std::uint16_t>(systems_whole + 2 * n))};
    }
    return ship;
}

void BuckRogersHost::keep_solar_map(TourHost &tour, const original::SolarMapState &state)
{
    tour.script_machine().bind_variable(phase_cell, static_cast<std::uint16_t>(state.phase));
    tour.script_machine().bind_variable(days_cell, static_cast<std::uint16_t>(state.days));
    tour.script_machine().bind_variable(
        rocket_tile_cell, static_cast<std::uint16_t>(original::rocket_tile(state.heading)));
    tour.script_machine().bind_variable(rocket_x_cell, static_cast<std::uint16_t>(state.rocket.x));
    tour.script_machine().bind_variable(rocket_y_cell, static_cast<std::uint16_t>(state.rocket.y));
    const auto bodies = original::place_bodies(state.phase);
    for (std::size_t body = 0; body < bodies.size(); ++body)
    {
        const auto at = static_cast<std::uint16_t>(first_body_cell + 2 * body);
        tour.script_machine().bind_variable(EclAddress{at},
                                            static_cast<std::uint16_t>(bodies[body].cell.x));
        tour.script_machine().bind_variable(EclAddress{static_cast<std::uint16_t>(at + 1)},
                                            static_cast<std::uint16_t>(bodies[body].cell.y));
    }
}

void BuckRogersHost::solar_map_step(TourHost &tour)
{
    tour.read_character();
    if (tour.script_machine().variable(move_blocked_cell) == move_blocked)
    {
        tour.script_machine().bind_variable(move_blocked_cell, 0);
        ++tour.screen().revision;
        return;
    }
    keep_solar_map(
        tour, original::step_rocket(solar_map_state(tour), heading_of(*tour.pending_movement())));
    if (tour.campaign())
    {
        BuckRogersMenuService::rest_buck_rogers_team(*tour.campaign(), original::rocket_step_days);
        for (const auto &w : tour.character_reply(tour.selected_character()).writes)
        {
            tour.script_machine().bind_variable(w.address, w.value);
        }
    }
    tour.synchronize_clock();
    ++tour.screen().revision;
}

Image BuckRogersHost::render_solar_map(const TourHost &tour) const
{
    if (!tour.resources() || tour.town_resources().solar_map_tiles.empty() ||
        !consistent(tour.town_resources().solar_map_backdrop))
    {
        return {};
    }
    auto map = tour.town_resources().solar_map_backdrop;
    const auto &tiles = tour.town_resources().solar_map_tiles;
    const auto draw = [&](unsigned tile, original::OrbitCell cell)
    {
        if (tile < tiles.size())
        {
            detail::draw_over(map, tiles[tile], cell.x * cell_pixels, cell.y * cell_pixels);
        }
    };
    draw(original::sun_tile, original::sun_cell);
    const auto state = solar_map_state(tour);
    for (const auto &body : original::place_bodies(state.phase))
    {
        draw(body.tile, body.cell);
    }
    draw(original::rocket_tile(state.heading), state.rocket);
    return map;
}
} // namespace opengold::por
