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
                        return tile > Terrain::overgrown;
                    }) ||
        (!board.plants.empty() && board.plants.size() != board.terrain.size()))
    {
        throw std::runtime_error("Invalid battlefield");
    }
}

MovementGrid::MovementGrid(const Battlefield &board, Cell origin,
                           std::span<const Occupant> occupants, bool crawling, bool flying,
                           bool incorporeal, Footprint footprint)
    : board_(board), origin_(origin), crawling_(crawling), flying_(flying),
      incorporeal_(incorporeal), footprint_(footprint)
{
    validate_battlefield(board_);
    if (footprint_.width < 1 || footprint_.height < 1 || board_.at(origin_) == Terrain::obstacle)
    {
        throw std::runtime_error("Invalid movement origin");
    }
    occupancy_.resize(board_.terrain.size(), Occupancy::empty);
    held_.resize(board_.terrain.size());
    for (const auto &occupant : occupants)
    {
        for (int dy = 0; dy < occupant.footprint.height; ++dy)
        {
            for (int dx = 0; dx < occupant.footprint.width; ++dx)
            {
                const Cell square{occupant.cell.x + dx, occupant.cell.y + dy};
                if (!board_.contains(square))
                {
                    throw std::runtime_error("Invalid occupant cell");
                }
                if (!occupant.downed)
                {
                    held_[index(square)] = true;
                }
                auto &entry = occupancy_[index(square)];
                // Apply the strongest restriction when several creatures overlap:
                // a conscious enemy blocks; an incapacitated enemy adds difficult terrain.
                const auto candidate = occupant.hostile ? (occupant.incapacitated || incorporeal_
                                                               ? Occupancy::incapacitated_enemy
                                                               : Occupancy::enemy)
                                                        : Occupancy::ally;
                entry = std::max(entry, candidate);
            }
        }
    }
}

int MovementGrid::index(Cell cell) const
{
    return cell.y * board_.width + cell.x;
}

bool MovementGrid::can_stop_at(Cell destination) const
{
    if (destination == origin_)
    {
        return false;
    }
    for (int dy = 0; dy < footprint_.height; ++dy)
    {
        for (int dx = 0; dx < footprint_.width; ++dx)
        {
            const Cell square{destination.x + dx, destination.y + dy};
            if (board_.at(square) == Terrain::obstacle || held_[index(square)])
            {
                return false;
            }
        }
    }
    return true;
}

// The dearest square sets the step's cost: a large creature is slowed when any
// of its squares is.
std::optional<int> MovementGrid::step_cost(Cell from, Cell to) const
{
    int cost = 0;
    for (int dy = 0; dy < footprint_.height; ++dy)
    {
        for (int dx = 0; dx < footprint_.width; ++dx)
        {
            const auto square =
                square_step_cost({from.x + dx, from.y + dy}, {to.x + dx, to.y + dy});
            if (!square)
            {
                return std::nullopt;
            }
            cost = std::max(cost, *square);
        }
    }
    return cost;
}

std::optional<int> MovementGrid::square_step_cost(Cell from, Cell to) const
{
    if (!board_.contains(from) || !board_.contains(to))
    {
        return std::nullopt;
    }
    if (!incorporeal_ &&
        (board_.at(from) == Terrain::obstacle || board_.at(to) == Terrain::obstacle))
    {
        return std::nullopt;
    }
    const int dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
    if (std::max(dx, dy) != 1)
    {
        return std::nullopt;
    }
    if (dx && dy && !incorporeal_ &&
        (board_.at({from.x, to.y}) == Terrain::obstacle ||
         board_.at({to.x, from.y}) == Terrain::obstacle))
    {
        return std::nullopt;
    }
    const auto occupant = occupancy_[index(to)];
    if (occupant == Occupancy::enemy)
    {
        return std::nullopt;
    }
    const auto ground = board_.at(to);
    const bool difficult = (!flying_ && ground == Terrain::difficult) ||
                           occupant == Occupancy::incapacitated_enemy ||
                           ground == Terrain::obstacle;
    return (!flying_ && ground == Terrain::overgrown ? 20 : difficult ? 10 : 5) +
           (crawling_ ? 5 : 0);
}

ReachableCells MovementGrid::reachable(int budget) const
{
    if (budget < 0)
    {
        throw std::runtime_error("Negative movement budget");
    }
    ReachableCells result;
    result.width_ = board_.width;
    result.height_ = board_.height;
    result.origin_ = index(origin_);
    result.costs_.assign(board_.terrain.size(), unreachable);
    result.previous_.assign(board_.terrain.size(), -1);
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            result.destinations_.push_back(can_stop_at({x, y}));
        }
    }

    // Dijkstra: the cheapest queued cell is final when removed. Every edge
    // costs at least 5, so a route through a more expensive cell cannot improve it.
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
        {
            continue; // Superseded route.
        }
        const Cell current{current_index % board_.width, current_index / board_.width};
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const Cell next{current.x + dx, current.y + dy};
                const auto step = step_cost(current, next);
                if (!step || *step > budget - spent)
                {
                    continue;
                }
                const int next_index = index(next), total = spent + *step;
                if (total >= result.costs_[next_index])
                {
                    continue;
                }
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
    {
        return std::nullopt;
    }
    const auto index = destination.y * width_ + destination.x;
    if (!destinations_[index] || costs_[index] == unreachable)
    {
        return std::nullopt;
    }
    return costs_[index];
}

std::vector<Cell> ReachableCells::path_to(Cell destination) const
{
    if (!cost_to(destination))
    {
        return {};
    }
    std::vector<Cell> path;
    // Each predecessor has strictly lower cost, so this chain cannot cycle.
    for (int at = destination.y * width_ + destination.x; at != origin_; at = previous_[at])
    {
        path.push_back({at % width_, at / width_});
    }
    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace opengold::srd5::detail
