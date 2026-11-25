#include "line_walk.h"

#include <algorithm>
#include <cstdlib>

namespace opengold::original::detail
{
// Whether the line from one square to another is clear (pathing.pas,
// WalkLine): a square on it, both ends too, blocks it when the square's
// blocking height is above the clearance of the square looked from. Without
// heights an obstacle blocks and nothing else does.
bool clear_line(const rules::Battlefield &board, rules::Cell from, rules::Cell to)
{
    const auto at = [&board](rules::Cell cell)
    {
        return static_cast<std::size_t>(cell.y * board.width + cell.x);
    };
    const auto blocking = [&](rules::Cell cell) -> int
    {
        if (!board.contains(cell))
        {
            return 255;
        }
        if (board.blocking.empty())
        {
            return board.at(cell) == rules::Terrain::obstacle ? 2 : 0;
        }
        return board.blocking[at(cell)];
    };
    const int eye =
        board.clearance.empty() || !board.contains(from) ? 0 : board.clearance[at(from)];
    LineWalk line(from, to);
    do
    {
        if (blocking(line.at) > eye)
        {
            return false;
        }
    } while (line.next());
    return true;
}

// Whether a direction leads from one cell toward another: the other cell lies
// in the 90-degree cone around the cell one step that way (pathing.pas, IsDirToward).
bool leads_toward(rules::Cell from, rules::Cell to, int direction)
{
    const int nx = from.x + step_x[static_cast<std::size_t>(direction)];
    const int ny = from.y + step_y[static_cast<std::size_t>(direction)];
    const int tx = to.x, ty = to.y;
    if (from == to || (nx == tx && ny == ty))
    {
        return true;
    }
    switch (direction)
    {
    case 0:
        return (tx >= nx && ty <= nx - tx + ny) || (tx <= nx && ty <= tx - nx + ny);
    case 1:
        return (tx >= nx && ty <= nx - tx + ny) || (tx >= nx + ny - ty && ty <= ny);
    case 2:
        return (tx >= nx + ny - ty && ty <= ny) || (tx >= nx + ty - ny && ty >= ny);
    case 3:
        return (tx >= nx + ty - ny && ty >= ny) || (tx >= nx && ty >= tx - nx + ny);
    case 4:
        return (tx >= nx && ty >= tx - nx + ny) || (tx <= nx && ty >= nx - tx + ny);
    case 5:
        return (tx <= nx && ty >= nx - tx + ny) || (tx <= nx + ny - ty && ty >= ny);
    case 6:
        return (tx <= nx + ny - ty && ty >= ny) || (tx <= nx + ty - ny && ty <= ny);
    default:
        return (tx <= nx + ty - ny && ty <= ny) || (tx <= nx && ty <= tx - nx + ny);
    }
}

// The first direction that leads from one cell toward another (cbtacts.pas, DirTowardTarget).
int direction_toward(rules::Cell from, rules::Cell to)
{
    for (int direction = 0; direction < 8; ++direction)
    {
        if (leads_toward(from, to, direction))
        {
            return direction;
        }
    }
    return 0;
}
} // namespace opengold::original::detail
