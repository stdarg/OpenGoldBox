#include "opengold/buck_rogers_solar_map.h"

#include <algorithm>
#include <stdexcept>

namespace opengold::original
{
namespace
{
// Each place on the five orbits, orbit by orbit, with the tile a body there
// is drawn with (gdconsts.pas orbitPositions); each orbit starts at its
// first place and runs clockwise.
struct OrbitPlace
{
    OrbitCell cell;
    unsigned tile{};
};

constexpr std::array<OrbitPlace, 58> orbit_places{{
    // Mercury's orbit.
    {{12, 10}, 1},
    {{10, 10}, 2},
    // Venus's.
    {{12, 9}, 3},
    {{12, 11}, 6},
    {{10, 11}, 5},
    {{10, 9}, 4},
    // Earth's.
    {{11, 7}, 9},
    {{13, 8}, 8},
    {{14, 10}, 7},
    {{13, 12}, 14},
    {{11, 13}, 13},
    {{9, 12}, 12},
    {{8, 10}, 11},
    {{9, 8}, 10},
    // Mars's.
    {{11, 4}, 19},
    {{13, 5}, 18},
    {{15, 6}, 17},
    {{16, 8}, 16},
    {{17, 10}, 15},
    {{16, 12}, 30},
    {{15, 14}, 29},
    {{13, 15}, 28},
    {{11, 16}, 27},
    {{9, 15}, 26},
    {{7, 14}, 25},
    {{6, 12}, 24},
    {{5, 10}, 23},
    {{6, 8}, 22},
    {{7, 6}, 21},
    {{9, 5}, 20},
    // The asteroids'.
    {{12, 0}, 66},
    {{14, 1}, 65},
    {{16, 2}, 64},
    {{18, 3}, 63},
    {{19, 5}, 62},
    {{20, 7}, 61},
    {{21, 9}, 60},
    {{21, 11}, 59},
    {{20, 13}, 86},
    {{19, 15}, 85},
    {{18, 17}, 84},
    {{16, 18}, 83},
    {{14, 19}, 82},
    {{12, 20}, 81},
    {{10, 20}, 80},
    {{8, 19}, 79},
    {{6, 18}, 78},
    {{4, 17}, 77},
    {{3, 15}, 76},
    {{2, 13}, 75},
    {{1, 11}, 74},
    {{1, 9}, 73},
    {{2, 7}, 72},
    {{3, 5}, 71},
    {{4, 3}, 70},
    {{6, 2}, 69},
    {{8, 1}, 68},
    {{10, 0}, 67},
}};

constexpr std::array<std::size_t, solar_orbits> first_places{0, 2, 6, 14, 30};
// Where each orbit's body stands at phase 0 (orbitStartPhases), and how far
// round the asteroids' orbit from Ceres the eight others stand
// (outerBodyPhases).
constexpr std::array<unsigned, solar_orbits> start_places{0, 0, 7, 9, 7};
constexpr std::array<unsigned, solar_bodies - solar_orbits> asteroid_offsets{3,  6,  9,  12,
                                                                             17, 19, 22, 26};
// The eight other asteroids are drawn with the tiles 28 before the ones
// Ceres takes at the same place (OUTER_BODY_TILES).
constexpr unsigned other_asteroid_tile_offset = 28;
// The rocket's tiles, one per heading from north (C_Orbit.FIRST_ROCKET_ICON).
constexpr unsigned first_rocket_tile = 87;
// The orbit a new game's rocket starts on: Earth's (gameinit.pas START_ORBIT).
constexpr std::size_t earth_orbit = 2;

// A cell's step for each heading, from north counterclockwise.
constexpr std::array<int, 8> step_columns{0, -1, -1, -1, 0, 1, 1, 1};
constexpr std::array<int, 8> step_rows{-1, -1, 0, 1, 1, 1, 0, -1};

[[nodiscard]] const OrbitPlace &place_on(std::size_t orbit, unsigned round) noexcept
{
    return orbit_places[first_places[orbit] + round % orbit_lengths[orbit]];
}

[[nodiscard]] unsigned kept_on_map(unsigned from, int step) noexcept
{
    const int moved = static_cast<int>(from) + step;
    return static_cast<unsigned>(std::clamp(moved, 0, static_cast<int>(solar_map_cells) - 1));
}
} // namespace

SolarMapState new_voyage() noexcept
{
    return {0, 0, RocketHeading::north, place_on(earth_orbit, start_places[earth_orbit]).cell};
}

std::array<PlacedBody, solar_bodies> place_bodies(unsigned phase) noexcept
{
    std::array<PlacedBody, solar_bodies> bodies{};
    for (std::size_t orbit = 0; orbit < solar_orbits; ++orbit)
    {
        const auto &place = place_on(orbit, start_places[orbit] + phase);
        bodies[orbit] = {place.cell, place.tile};
    }
    constexpr std::size_t asteroids = solar_orbits - 1;
    for (std::size_t other = 0; other < asteroid_offsets.size(); ++other)
    {
        const auto &place =
            place_on(asteroids, start_places[asteroids] + asteroid_offsets[other] + phase);
        bodies[solar_orbits + other] = {place.cell, place.tile - other_asteroid_tile_offset};
    }
    return bodies;
}

std::span<const OrbitCell> orbit_cells(std::size_t orbit)
{
    static const auto cells = []
    {
        std::array<OrbitCell, orbit_places.size()> all{};
        std::ranges::transform(orbit_places, all.begin(), &OrbitPlace::cell);
        return all;
    }();
    if (orbit >= solar_orbits)
    {
        throw std::out_of_range("No such orbit");
    }
    return std::span(cells).subspan(first_places[orbit], orbit_lengths[orbit]);
}

unsigned rocket_tile(RocketHeading heading) noexcept
{
    return first_rocket_tile + static_cast<unsigned>(heading);
}

SolarMapState step_rocket(SolarMapState state, RocketHeading heading) noexcept
{
    const auto way = static_cast<std::size_t>(heading);
    state.heading = heading;
    const OrbitCell next{kept_on_map(state.rocket.x, step_columns[way]),
                         kept_on_map(state.rocket.y, step_rows[way])};
    if (next != sun_cell)
    {
        state.rocket = next;
    }
    state.days += rocket_step_days;
    if (state.days >= days_per_phase)
    {
        state.days -= days_per_phase;
        state.phase = (state.phase + phase_cycle - 1) % phase_cycle;
    }
    return state;
}

std::optional<std::size_t> body_at(unsigned phase, OrbitCell cell) noexcept
{
    const auto bodies = place_bodies(phase);
    for (std::size_t body = bodies.size(); body-- > 0;)
    {
        if (bodies[body].cell == cell)
        {
            return body;
        }
    }
    return std::nullopt;
}
} // namespace opengold::original
