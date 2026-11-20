// The Buck Rogers games' solar map under their original rules
// (docs/games/countdown.md, "The solar map"): the bodies' places by phase,
// the rocket's steps and the days they take. Fuel and arrival at a body are
// the games' scripts', checked on the game's files by
// countdown_campaign_tests.cpp.
#include "opengold/buck_rogers_solar_map.h"
#include "test_support.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>

namespace
{
using namespace opengold::original;
using opengold::test::check;

std::string cell_text(OrbitCell cell)
{
    return std::to_string(cell.x) + "," + std::to_string(cell.y);
}

// A new game's rocket heads north on Earth's cell, where Earth stands at
// phase 0 (gameinit.pas InitShipAndOrbit).
void a_voyage_starts_at_earth()
{
    const auto start = new_voyage();
    check(start.phase == 0 && start.days == 0 && start.heading == RocketHeading::north &&
              start.rocket == OrbitCell{9, 8},
          "A voyage starts on Earth's cell, heading north: " + cell_text(start.rocket));
    check(body_at(0, start.rocket) == 2, "Earth is body 2, planet 3 to the scripts");
    check(rocket_tile(start.heading) == 87 && rocket_tile(RocketHeading::northeast) == 94,
          "The rocket's tile turns with its heading");
}

// At phase 0 each body stands at its starting place; the others on the
// asteroids' orbit stand their places round from Ceres, drawn with the tiles
// 28 before Ceres's at the same place (orbitmap.pas PaintOrbitMap).
void the_bodies_stand_on_their_orbits()
{
    const auto bodies = place_bodies(0);
    check(bodies[0].cell == OrbitCell{12, 10} && bodies[0].tile == 1, "Mercury at phase 0");
    check(bodies[1].cell == OrbitCell{12, 9} && bodies[1].tile == 3, "Venus at phase 0");
    check(bodies[3].cell == OrbitCell{9, 15} && bodies[3].tile == 26, "Mars at phase 0");
    check(bodies[4].cell == OrbitCell{21, 11} && bodies[4].tile == 59, "Ceres at phase 0");
    // Vesta is three places round from Ceres; Thule twenty-six.
    check(bodies[5].cell == OrbitCell{18, 17} && bodies[5].tile == 84 - 28, "Vesta at phase 0");
    check(bodies[12].cell == OrbitCell{20, 7} && bodies[12].tile == 61 - 28, "Thule at phase 0");
    for (std::size_t body = 0; body < bodies.size(); ++body)
    {
        const auto orbit = std::min(body, solar_orbits - 1);
        const auto cells = orbit_cells(orbit);
        check(std::ranges::find(cells, bodies[body].cell) != cells.end(),
              "Body " + std::to_string(body) + " stands on its orbit");
        check(bodies[body].cell != sun_cell, "No body stands on the Sun");
    }
    std::set<std::pair<unsigned, unsigned>> taken;
    for (const auto &body : bodies)
    {
        taken.insert({body.cell.x, body.cell.y});
    }
    check(taken.size() == solar_bodies, "No two bodies share a cell at phase 0");
}

// Every 32 days the bodies move a place back round their orbits, which is
// counterclockwise on the map; after 224 phases they stand where they began.
void the_bodies_move_as_days_pass()
{
    auto state = new_voyage();
    for (int step = 0; step < 10; ++step)
    {
        state = step_rocket(state, RocketHeading::north);
    }
    check(state.phase == 0 && state.days == 30, "Ten steps are 30 days, still phase 0");
    state = step_rocket(state, RocketHeading::north);
    check(state.phase == phase_cycle - 1 && state.days == 1,
          "The eleventh step passes 32 days: the phase counts down from 0 to 223");
    const auto moved = place_bodies(state.phase);
    check(moved[2].cell == OrbitCell{8, 10} && moved[2].tile == 11,
          "Earth moves a place back round its orbit: " + cell_text(moved[2].cell));
    check(moved[0].cell == OrbitCell{10, 10}, "Mercury goes round in two phases");
    const auto start = place_bodies(0);
    const auto again = place_bodies(phase_cycle);
    for (std::size_t body = 0; body < solar_bodies; ++body)
    {
        check(again[body].cell == start[body].cell,
              "After 224 phases body " + std::to_string(body) + " is back");
    }
}

// The rocket turns and steps a cell each way, stays on the map, and never
// enters the Sun's cell; a step that cannot move still takes its three days
// (explore.pas MoveRocket).
void the_rocket_flies_a_cell_a_step()
{
    const SolarMapState middle{0, 0, RocketHeading::north, {5, 5}};
    const std::array<std::pair<RocketHeading, OrbitCell>, 8> ways{{
        {RocketHeading::north, {5, 4}},
        {RocketHeading::northwest, {4, 4}},
        {RocketHeading::west, {4, 5}},
        {RocketHeading::southwest, {4, 6}},
        {RocketHeading::south, {5, 6}},
        {RocketHeading::southeast, {6, 6}},
        {RocketHeading::east, {6, 5}},
        {RocketHeading::northeast, {6, 4}},
    }};
    for (const auto &[heading, to] : ways)
    {
        const auto stepped = step_rocket(middle, heading);
        check(stepped.rocket == to && stepped.heading == heading && stepped.days == 3,
              "A step " + std::to_string(static_cast<int>(heading)) + " reaches " +
                  cell_text(stepped.rocket));
    }
    const auto corner = step_rocket({0, 0, RocketHeading::north, {0, 0}}, RocketHeading::northwest);
    check(corner.rocket == OrbitCell{0, 0} && corner.heading == RocketHeading::northwest &&
              corner.days == 3,
          "The rocket stays on the map at its corner, and the days pass");
    const auto edge = step_rocket({0, 0, RocketHeading::north, {21, 21}}, RocketHeading::southeast);
    check(edge.rocket == OrbitCell{21, 21}, "The far corner too");
    const auto sun = step_rocket({0, 0, RocketHeading::north, {11, 11}}, RocketHeading::north);
    check(sun.rocket == OrbitCell{11, 11} && sun.heading == RocketHeading::north && sun.days == 3,
          "A step onto the Sun's cell leaves the rocket where it was");
}

// The scripts take the last body in their order whose cell the rocket is on.
void a_body_is_found_by_its_cell()
{
    for (unsigned phase = 0; phase < phase_cycle; ++phase)
    {
        const auto bodies = place_bodies(phase);
        for (std::size_t body = 0; body < bodies.size(); ++body)
        {
            const auto found = body_at(phase, bodies[body].cell);
            check(found && *found >= body, "Each body is found on its cell");
        }
    }
    check(!body_at(0, OrbitCell{0, 0}), "No body in empty space");
}
} // namespace

int main()
{
    try
    {
        a_voyage_starts_at_earth();
        the_bodies_stand_on_their_orbits();
        the_bodies_move_as_days_pass();
        the_rocket_flies_a_cell_a_step();
        a_body_is_found_by_its_cell();
        std::cout << "Buck Rogers solar map passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
