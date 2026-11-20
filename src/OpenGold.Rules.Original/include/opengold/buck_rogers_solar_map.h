#ifndef OPENGOLD_BUCK_ROGERS_SOLAR_MAP_H
#define OPENGOLD_BUCK_ROGERS_SOLAR_MAP_H

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The solar map of the two Buck Rogers games, Countdown to Doomsday and
// Matrix Cubed (docs/games/countdown.md, "The solar map"): a grid of 22 by 22
// cells round the Sun, five orbits and thirteen bodies on them, and the
// team's rocket flying a cell a step. Both games' engines draw and move it
// the same way (orbitmap.pas PaintOrbitMap, explore.pas MoveRocket), from
// the same tables (gdconsts.pas), so these rules serve both. Fuel, arrival
// and what a body offers are the games' scripts' (script 34 in Countdown,
// 19 in Matrix Cubed), which compare the rocket's cell with the bodies'.
namespace opengold::original
{
// A cell of the solar map: its column and row, 0 to 21, north at the top.
struct OrbitCell
{
    unsigned x{}, y{};

    friend constexpr auto operator<=>(const OrbitCell &, const OrbitCell &) = default;
};

inline constexpr unsigned solar_map_cells = 22;
// The Sun's cell, which the rocket never enters.
inline constexpr OrbitCell sun_cell{11, 10};
// The orbits, from the Sun out, and the bodies on them: one on each of the
// first five, Mercury to Ceres, and eight more on the fifth, Vesta to Thule
// (the scripts' order).
inline constexpr std::size_t solar_orbits = 5;
inline constexpr std::size_t solar_bodies = 13;
inline constexpr std::array<unsigned, solar_orbits> orbit_lengths{2, 4, 8, 16, 28};
// A step of the rocket is three days of time; each 32 of them the bodies move
// on a phase, of 224 (explore.pas MoveRocket).
inline constexpr unsigned rocket_step_days = 3;
inline constexpr unsigned days_per_phase = 32;
inline constexpr unsigned phase_cycle = 224;

// The way the rocket heads, in the original's order: from north
// counterclockwise (gdconsts.pas mapStepX, mapStepY).
enum class RocketHeading : std::uint8_t
{
    north,
    northwest,
    west,
    southwest,
    south,
    southeast,
    east,
    northeast
};

// The solar map's state as the games keep it in their zone 0 (types.pas
// TOrbitView): the phase the bodies stand at, the days into it, and the
// rocket's heading and cell.
struct SolarMapState
{
    unsigned phase{}, days{};
    RocketHeading heading{RocketHeading::north};
    OrbitCell rocket{};

    friend constexpr bool operator==(const SolarMapState &, const SolarMapState &) = default;
};

// A new game's: phase 0, no days, the rocket heading north on Earth's cell
// (gameinit.pas InitShipAndOrbit).
[[nodiscard]] SolarMapState new_voyage() noexcept;

// A body where the phase puts it, and the tile it is drawn with: its own
// tile in the solar map's tile set (8X8D0 record 1) for that place on its
// orbit, so the side lit by the Sun turns as it goes round.
struct PlacedBody
{
    OrbitCell cell;
    unsigned tile{};
};

// The thirteen bodies at `phase` (orbitmap.pas PaintOrbitMap), in the
// scripts' order: body n is the planet number n + 1 the scripts write to
// zone 0 when the rocket is on its cell.
[[nodiscard]] std::array<PlacedBody, solar_bodies> place_bodies(unsigned phase) noexcept;
// Every cell of orbit `orbit` (0 to 4), in its order.
[[nodiscard]] std::span<const OrbitCell> orbit_cells(std::size_t orbit);

// The Sun's tile and the rocket's, which turns with its heading.
inline constexpr unsigned sun_tile = 0;
[[nodiscard]] unsigned rocket_tile(RocketHeading heading) noexcept;

// One step of the rocket (explore.pas MoveRocket): it turns to `heading` and
// moves a cell that way, staying on the map; a step onto the Sun's cell
// leaves it where it was. Either way three days pass, and every 32 days the
// bodies move on a phase. The team's healing in those days and the clock
// are the caller's.
[[nodiscard]] SolarMapState step_rocket(SolarMapState state, RocketHeading heading) noexcept;

// The body on `cell` at `phase`, as the scripts find it: the last in their
// order whose cell it is.
[[nodiscard]] std::optional<std::size_t> body_at(unsigned phase, OrbitCell cell) noexcept;
} // namespace opengold::original
#endif
