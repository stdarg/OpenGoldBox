#include "combat_grid.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

namespace
{
constexpr int unreachable = std::numeric_limits<int>::max();
}

void validate_battlefield(const Battlefield &board)
{
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64 ||
        board.terrain.size() != static_cast<std::size_t>(board.width * board.height) ||
        std::any_of(board.terrain.begin(), board.terrain.end(),
                    [](auto tile)
                    {
                        return tile > 2;
                    }))
        throw std::runtime_error("Invalid battlefield");
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

MovementGrid::MovementGrid(const Battlefield &board, Cell origin,
                           std::span<const Occupant> occupants, bool crawling)
    : board_(board), origin_(origin), crawling_(crawling)
{
    validate_battlefield(board_);
    if (board_.at(origin_) == 1)
        throw std::runtime_error("Invalid movement origin");
    occupancy_.resize(board_.terrain.size(), Occupancy::empty);
    for (const auto &occupant : occupants)
    {
        if (!board_.contains(occupant.cell))
            throw std::runtime_error("Invalid occupant cell");
        auto &entry = occupancy_[index(occupant.cell)];
        // Apply the strongest restriction when several creatures overlap:
        // a conscious enemy blocks; an incapacitated enemy adds difficult terrain.
        const auto candidate =
            occupant.hostile
                ? (occupant.incapacitated ? Occupancy::incapacitated_enemy : Occupancy::enemy)
                : Occupancy::ally;
        entry = std::max(entry, candidate);
    }
}

int MovementGrid::index(Cell cell) const
{
    return cell.y * board_.width + cell.x;
}

bool MovementGrid::can_stop_at(Cell destination) const
{
    return board_.at(destination) != 1 && destination != origin_ &&
           occupancy_[index(destination)] == Occupancy::empty;
}

std::optional<int> MovementGrid::step_cost(Cell from, Cell to) const
{
    if (board_.at(from) == 1 || board_.at(to) == 1)
        return std::nullopt;
    const int dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
    if (std::max(dx, dy) != 1)
        return std::nullopt;
    if (dx && dy && (board_.at({from.x, to.y}) == 1 || board_.at({to.x, from.y}) == 1))
        return std::nullopt;
    const auto occupant = occupancy_[index(to)];
    if (occupant == Occupancy::enemy)
        return std::nullopt;
    return (board_.at(to) == 2 || occupant == Occupancy::incapacitated_enemy ? 10 : 5) +
           (crawling_ ? 5 : 0);
}

ReachableCells MovementGrid::reachable(int budget) const
{
    if (budget < 0)
        throw std::runtime_error("Negative movement budget");
    ReachableCells result;
    result.width_ = board_.width;
    result.height_ = board_.height;
    result.origin_ = index(origin_);
    result.costs_.assign(board_.terrain.size(), unreachable);
    result.previous_.assign(board_.terrain.size(), -1);
    for (int y = 0; y < board_.height; ++y)
        for (int x = 0; x < board_.width; ++x)
            result.destinations_.push_back(can_stop_at({x, y}));

    // Dijkstra: the cheapest queued cell is final when removed. Every edge
    // costs 5 or 10, so a route through a more expensive cell cannot improve it.
    // Equal costs use the row-major cell index, preserving deterministic paths
    // (and therefore which opportunity attacks occur along those paths).
    using QueueItem = std::pair<int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
    result.costs_[result.origin_] = 0;
    queue.push({0, result.origin_});
    while (!queue.empty())
    {
        const auto [spent, current_index] = queue.top();
        queue.pop();
        if (spent != result.costs_[current_index])
            continue; // Superseded route.
        const Cell current{current_index % board_.width, current_index / board_.width};
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const Cell next{current.x + dx, current.y + dy};
                const auto step = step_cost(current, next);
                if (!step || *step > budget - spent)
                    continue;
                const int next_index = index(next), total = spent + *step;
                if (total >= result.costs_[next_index])
                    continue;
                result.costs_[next_index] = total;
                result.previous_[next_index] = current_index;
                queue.push({total, next_index});
            }
        }
    }
    return result;
}

std::optional<int> ReachableCells::cost_to(Cell destination) const
{
    if (destination.x < 0 || destination.y < 0 || destination.x >= width_ ||
        destination.y >= height_)
        return std::nullopt;
    const auto index = destination.y * width_ + destination.x;
    if (!destinations_[index] || costs_[index] == unreachable)
        return std::nullopt;
    return costs_[index];
}

std::vector<Cell> ReachableCells::path_to(Cell destination) const
{
    if (!cost_to(destination))
        return {};
    std::vector<Cell> path;
    // Each predecessor has strictly lower cost, so this chain cannot cycle.
    for (int at = destination.y * width_ + destination.x; at != origin_; at = previous_[at])
        path.push_back({at % width_, at / width_});
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace opengold::srd5::detail
