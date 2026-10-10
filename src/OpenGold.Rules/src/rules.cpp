#include "opengold/rules.h"
#include <cstdlib>

namespace opengold::rules
{
bool Battlefield::contains(Cell p) const noexcept
{
    return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height;
}

unsigned Battlefield::at(Cell p) const noexcept
{
    if (!contains(p))
        return 1;
    const auto index = static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(p.x);
    return index < terrain.size() ? terrain[index] : 1;
}
bool has_line_of_sight(const Battlefield &board, Cell from, Cell to)
{
    if (board.at(from) == 1 || board.at(to) == 1)
        return false;
    const int columns = std::abs(to.x - from.x), rows = std::abs(to.y - from.y);
    const int step_x = to.x > from.x ? 1 : -1, step_y = to.y > from.y ? 1 : -1;
    int crossed_x = 0, crossed_y = 0;
    Cell current = from;
    while (crossed_x < columns || crossed_y < rows)
    {
        // Compare the next vertical and horizontal boundary crossings without
        // division. Cell centers are half a cell from their first boundary:
        // t_x = (1 + 2*crossed_x)/(2*columns), similarly for t_y.
        // Cross multiplication also handles horizontal/vertical rays (a zero
        // denominator means that boundary is never crossed).
        const int order = (1 + 2 * crossed_x) * rows - (1 + 2 * crossed_y) * columns;
        if (order == 0)
        {
            // Exact corner contact touches both side cells as well as the
            // diagonal cell. Checking all three prevents sight through walls.
            if (board.at({current.x + step_x, current.y}) == 1 ||
                    board.at({current.x, current.y + step_y}) == 1)
                return false;
            current.x += step_x;
            current.y += step_y;
            ++crossed_x;
            ++crossed_y;
        }
        else if (order < 0)
        {
            current.x += step_x;
            ++crossed_x;
        }
        else
        {
            current.y += step_y;
            ++crossed_y;
        }
        if (board.at(current) == 1)
            return false;
    }
    return true;
}

} // namespace opengold::rules
