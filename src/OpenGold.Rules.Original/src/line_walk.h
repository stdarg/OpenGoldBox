#ifndef OPENGOLD_ORIGINAL_LINE_WALK_H
#define OPENGOLD_ORIGINAL_LINE_WALK_H
#include "opengold/rules.h"

#include <algorithm>
#include <array>
#include <cstdlib>

// The original's lines across the battlefield (pathing.pas): a line walked a
// square at a time, whether it is clear, and its eight directions.
namespace opengold::original::detail
{
// A straight line walked a square at a time, its doubled distance growing by
// 2 a straight step and 3 a diagonal one (pathing.pas, PathFind and NextPathStep).
struct LineWalk
{
    rules::Cell start, end, at;
    int across{}, down{}, step_x{}, step_y{}, error{}, diagonal_step{}, straight_step{}, distance{};

    LineWalk(rules::Cell from, rules::Cell to)
        : start(from), end(to), at(from), across(std::abs(to.x - from.x)),
          down(std::abs(to.y - from.y)), step_x((to.x > from.x) - (to.x < from.x)),
          step_y((to.y > from.y) - (to.y < from.y)), error(2 * minor() - major()),
          diagonal_step(2 * (minor() - major())), straight_step(2 * minor())
    {
    }

    [[nodiscard]] int major() const noexcept
    {
        return std::max(across, down);
    }

    [[nodiscard]] int minor() const noexcept
    {
        return std::min(across, down);
    }

    // One step on; false at the line's end.
    bool next()
    {
        const bool x_major = across > down;
        if ((x_major && at.x == end.x) || (!x_major && at.y == end.y))
        {
            return false;
        }
        if (error >= 0)
        {
            (x_major ? at.y : at.x) += x_major ? step_y : step_x;
            error += diagonal_step;
            distance += 3;
        }
        else
        {
            error += straight_step;
            distance += 2;
        }
        (x_major ? at.x : at.y) += x_major ? step_x : step_y;
        return true;
    }
};

// Whether the line from one square to another is clear of the ground's
// obstacles, both ends included.
[[nodiscard]] bool clear_line(const rules::Battlefield &board, rules::Cell from, rules::Cell to);

// The original's eight directions: 0 north and clockwise to 7 north-west.
inline constexpr std::array<int, 8> step_x{0, 1, 1, 1, 0, -1, -1, -1};
inline constexpr std::array<int, 8> step_y{-1, -1, 0, 1, 1, 1, 0, -1};

// Whether a direction leads from one cell toward another.
[[nodiscard]] bool leads_toward(rules::Cell from, rules::Cell to, int direction);
// The first direction that leads from one cell toward another.
[[nodiscard]] int direction_toward(rules::Cell from, rules::Cell to);
} // namespace opengold::original::detail
#endif
