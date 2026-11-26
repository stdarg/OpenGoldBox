#ifndef OPENGOLD_DIRECTIONS_H
#define OPENGOLD_DIRECTIONS_H
#include <array>

// The original games' eight directions (gdconsts.pas dirStepX, dirStepY): 0
// north and clockwise to 7 north-west, north towards smaller y. The original
// rules' line walk, the Buck Rogers ground arena, Pool of Radiance's overland
// map and the 256-color games' area map number by this one table.
namespace opengold::por::directions
{
inline constexpr std::array<int, 8> step_x{0, 1, 1, 1, 0, -1, -1, -1};
inline constexpr std::array<int, 8> step_y{-1, -1, 0, 1, 1, 1, 0, -1};
} // namespace opengold::por::directions
#endif
