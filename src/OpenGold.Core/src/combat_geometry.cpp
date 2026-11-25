#include "combat_geometry.h"
#include <algorithm>
#include <limits>

namespace opengold::combat_geometry
{
using namespace rules;

std::vector<Cell> squares_of(Cell start, Footprint footprint)
{
    std::vector<Cell> squares;
    for (int dy = 0; dy < footprint.height; ++dy)
    {
        for (int dx = 0; dx < footprint.width; ++dx)
        {
            squares.push_back({start.x + dx, start.y + dy});
        }
    }
    return squares;
}

std::vector<Cell> squares_of(const CombatantView &a)
{
    return squares_of(a.cell, a.footprint);
}

// Squares from `cell` to the nearest square `a` fills: 1 when beside it.
int squares_from(Cell cell, const CombatantView &a)
{
    const int across =
        std::max({0, a.cell.x - cell.x, cell.x - (a.cell.x + a.footprint.width - 1)});
    const int down = std::max({0, a.cell.y - cell.y, cell.y - (a.cell.y + a.footprint.height - 1)});
    return std::max(across, down);
}

// Squares between the nearest squares two combatants fill: 1 when beside.
int squares_apart(const CombatantView &a, const CombatantView &b)
{
    int nearest = std::numeric_limits<int>::max();
    for (const auto square : squares_of(a))
    {
        nearest = std::min(nearest, squares_from(square, b));
    }
    return nearest;
}

// Whether any square `a` fills is one of `cells`.
bool in_cells(const std::vector<Cell> &cells, const CombatantView &a)
{
    const auto squares = squares_of(a);
    return std::any_of(squares.begin(), squares.end(),
                       [&](Cell square)
                       {
                           return std::find(cells.begin(), cells.end(), square) != cells.end();
                       });
}

// Whether any of `viewer`'s squares has a line of sight to any of `seen`'s.
bool sees(const Battlefield &board, const CombatantView &viewer, const CombatantView &seen)
{
    for (const auto from : squares_of(viewer))
    {
        for (const auto to : squares_of(seen))
        {
            if (rules::has_line_of_sight(board, from, to))
            {
                return true;
            }
        }
    }
    return false;
}
} // namespace opengold::combat_geometry
