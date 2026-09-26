#include "combat_grid.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using namespace opengold::rules;
using namespace opengold::srd5::detail;
namespace
{
void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <class Function> void rejects(Function function)
{
    try
    {
        function();
    }
    catch (const std::runtime_error &)
    {
        return;
    }
    throw std::runtime_error("Invalid grid input was accepted");
}

// Independent, deliberately slow graph oracle: inspect every pair of cells,
// then relax all edges repeatedly (Bellman-Ford), without a priority queue.
int reference_step(const Battlefield &board, const std::vector<int> &occupants, Cell from, Cell to)
{
    if (board.at(from) == 1 || board.at(to) == 1 || from == to)
        return -1;
    if (std::abs(from.x - to.x) > 1 || std::abs(from.y - to.y) > 1)
        return -1;
    if (from.x != to.x && from.y != to.y &&
        (board.at({from.x, to.y}) == 1 || board.at({to.x, from.y}) == 1))
        return -1;
    const int occupant = occupants[to.y * board.width + to.x];
    if (occupant == 2)
        return -1;
    return board.at(to) == 2 || occupant == 3 ? 10 : 5;
}

std::vector<int> reference_costs(const Battlefield &board, const std::vector<int> &occupants,
                                 Cell origin)
{
    const int count = board.width * board.height;
    std::vector<int> costs(count, 100000);
    costs[origin.y * board.width + origin.x] = 0;
    for (int pass = 1; pass < count; ++pass)
    {
        for (int from = 0; from < count; ++from)
        {
            for (int to = 0; to < count; ++to)
            {
                const int edge =
                    reference_step(board, occupants, {from % board.width, from / board.width},
                                   {to % board.width, to / board.width});
                if (edge >= 0)
                    costs[to] = std::min(costs[to], costs[from] + edge);
            }
        }
    }
    return costs;
}

void compare_routes(const Battlefield &board, const std::vector<int> &occupants, Cell origin)
{
    std::vector<Occupant> actors;
    for (int i = 0; i < static_cast<int>(occupants.size()); ++i)
        if (occupants[i])
            actors.push_back(
                {{i % board.width, i / board.width}, occupants[i] >= 2, occupants[i] == 3});
    const MovementGrid grid(board, origin, actors);
    const auto expected = reference_costs(board, occupants, origin);
    for (const int budget : {0, 5, 9, 10, 15, 20, 30, std::numeric_limits<int>::max()})
    {
        const auto reachable = grid.reachable(budget);
        for (int i = 0; i < static_cast<int>(occupants.size()); ++i)
        {
            const Cell destination{i % board.width, i / board.width};
            const bool legal = destination != origin && !occupants[i] && expected[i] != 100000 &&
                               expected[i] <= budget;
            const auto cost = reachable.cost_to(destination);
            check(cost.has_value() == legal, "Reachability disagrees with reference graph");
            const auto path = reachable.path_to(destination);
            if (!legal)
            {
                check(path.empty(), "Illegal destination returned a path");
                continue;
            }
            check(*cost == expected[i], "Route is not minimum cost");
            check(!path.empty() && path.back() == destination, "Wrong path destination");
            Cell current = origin;
            int spent = 0;
            std::set<Cell> visited{origin};
            for (const auto next : path)
            {
                check(visited.insert(next).second, "Predecessor chain contains a cycle");
                const int step = reference_step(board, occupants, current, next);
                check(step > 0, "Path contains an illegal step");
                spent += step;
                current = next;
            }
            check(spent == *cost, "Path and advertised movement cost disagree");
        }
    }
}

void exhaustive_movement()
{
    // Every assignment of floor/wall/difficult/ally/enemy/unconscious enemy to the five cells
    // other than the origin: 6^5 = 7,776 boards, including disconnected maps.
    for (unsigned pattern = 0; pattern < 7776; ++pattern)
    {
        Battlefield board{3, 2, std::vector<std::uint8_t>(6)};
        std::vector<int> occupants(6);
        auto remaining = pattern;
        for (int cell = 1; cell < 6; ++cell)
        {
            const auto kind = remaining % 6;
            remaining /= 6;
            if (kind <= 2)
                board.terrain[cell] = kind;
            else
                occupants[cell] = kind - 2;
        }
        compare_routes(board, occupants, {0, 0});
    }
    // All 3^8 terrain patterns around a central origin exercise corners and
    // routes in all eight directions, independently of occupancy fixtures.
    for (unsigned pattern = 0; pattern < 6561; ++pattern)
    {
        Battlefield board{3, 3, std::vector<std::uint8_t>(9)};
        auto remaining = pattern;
        for (int cell = 0; cell < 9; ++cell)
        {
            if (cell == 4)
                continue;
            board.terrain[cell] = remaining % 3;
            remaining /= 3;
        }
        compare_routes(board, std::vector<int>(9), {1, 1});
    }
}

// Geometric oracle: intersect the center-to-center segment with each closed
// wall rectangle. This uses no cell traversal or boundary-crossing counters.
bool touches_wall(Cell from, Cell to, Cell wall)
{
    long double first = 0, last = 1;
    const int starts[]{2 * from.x + 1, 2 * from.y + 1};
    const int deltas[]{2 * (to.x - from.x), 2 * (to.y - from.y)};
    const int lows[]{2 * wall.x, 2 * wall.y};
    for (int axis = 0; axis < 2; ++axis)
    {
        if (deltas[axis] == 0)
        {
            if (starts[axis] < lows[axis] || starts[axis] > lows[axis] + 2)
                return false;
            continue;
        }
        const auto a = static_cast<long double>(lows[axis] - starts[axis]) / deltas[axis];
        const auto b = static_cast<long double>(lows[axis] + 2 - starts[axis]) / deltas[axis];
        first = std::max(first, std::min(a, b));
        last = std::min(last, std::max(a, b));
    }
    return first <= last + 1e-12L;
}

void exhaustive_sight()
{
    for (unsigned walls = 0; walls < 512; ++walls)
    {
        Battlefield board{3, 3, std::vector<std::uint8_t>(9)};
        for (int i = 0; i < 9; ++i)
            board.terrain[i] = (walls >> i) & 1;
        for (int a = 0; a < 9; ++a)
        {
            for (int b = 0; b < 9; ++b)
            {
                const Cell from{a % 3, a / 3}, to{b % 3, b / 3};
                bool clear = true;
                for (int wall = 0; wall < 9; ++wall)
                    if (board.terrain[wall] && touches_wall(from, to, {wall % 3, wall / 3}))
                        clear = false;
                check(has_line_of_sight(board, from, to) == clear,
                      "Sight disagrees with segment/rectangle oracle");
                check(has_line_of_sight(board, from, to) == has_line_of_sight(board, to, from),
                      "Sight is not symmetric");
            }
        }
    }
}

void allied_transit()
{
    Battlefield corridor{7, 2, std::vector<std::uint8_t>(14, 1)};
    for (int x = 0; x < 7; ++x)
        corridor.terrain[x] = 0;
    const std::vector<Occupant> allies{{{1, 0}, false}, {{2, 0}, false}};
    MovementGrid grid(corridor, {0, 0}, allies);
    check(grid.reachable(15).path_to({3, 0}) == std::vector<Cell>{{1, 0}, {2, 0}, {3, 0}},
          "A corridor route crosses successive allies at normal movement cost");
    check(grid.reachable(15).cost_to({3, 0}) == 15 && !grid.reachable(10).cost_to({3, 0}),
          "Only a fully affordable route to a free stopping point is offered");
    check(!grid.reachable(30).cost_to({1, 0}) && !grid.reachable(30).cost_to({2, 0}),
          "Allies remain transit cells, never destinations");
    corridor.terrain[1] = 2;
    grid = MovementGrid(corridor, {0, 0}, allies);
    check(grid.reachable(20).cost_to({3, 0}) == 20 && !grid.reachable(19).cost_to({3, 0}),
          "Difficult ground under an ally adds its normal five-foot surcharge once");
    auto hostile = allies;
    hostile[1].hostile = true;
    grid = MovementGrid(corridor, {0, 0}, hostile);
    check(!grid.reachable(60).cost_to({3, 0}), "A hostile creature still blocks the corridor");
    hostile[1].incapacitated = true;
    grid = MovementGrid(corridor, {0, 0}, hostile);
    check(grid.reachable(25).cost_to({3, 0}) == 25 && !grid.reachable(24).cost_to({3, 0}),
          "Unconscious hostile square adds difficult terrain to the route");
    corridor.terrain[2] = 2;
    grid = MovementGrid(corridor, {0, 0}, hostile);
    check(grid.reachable(25).cost_to({3, 0}) == 25,
          "Enemy occupancy and difficult ground do not stack");
    check(!grid.can_stop_at({2, 0}), "Unconscious enemy still occupies its square");
    auto overlapping = hostile;
    overlapping.push_back({{2, 0}, true, false});
    grid = MovementGrid(corridor, {0, 0}, overlapping);
    check(!grid.reachable(60).cost_to({3, 0}),
          "Conscious enemy wins over overlapping incapacitated enemy");
    corridor.terrain[1] = 1;
    grid = MovementGrid(corridor, {0, 0}, allies);
    check(!grid.reachable(60).cost_to({3, 0}), "Allied occupancy cannot bypass a wall");
}

void boundaries_and_ties()
{
    Battlefield board{3, 3, std::vector<std::uint8_t>(9)};
    board.terrain[4] = 1;
    MovementGrid grid(board, {0, 1}, {});
    check(grid.reachable(20).path_to({2, 1}) == std::vector<Cell>{{0, 0}, {1, 0}, {2, 0}, {2, 1}},
          "Equal-cost detours must keep their row-major tie order");
    check(!grid.can_stop_at({-1, 0}) && !grid.can_stop_at({3, 0}),
          "Outside destinations are rejected");
    check(!grid.reachable(20).cost_to({3, 0}), "Outside path query is rejected");
    check(!grid.step_cost({0, 1}, {0, 1}) && !grid.step_cost({0, 1}, {2, 1}),
          "Movement requires a single step");
    check(!has_line_of_sight(board, {std::numeric_limits<int>::min(), 0}, {0, 0}),
          "Outside sight endpoint rejected");
    rejects(
        [&]
        {
            (void)grid.reachable(-1);
        });
    rejects(
        [&]
        {
            (void)MovementGrid(board, {1, 1}, {});
        });
    auto invalid = board;
    invalid.terrain.pop_back();
    rejects(
        [&]
        {
            (void)MovementGrid(invalid, {0, 0}, {});
        });
    invalid = board;
    invalid.width = std::numeric_limits<int>::max();
    rejects(
        [&]
        {
            (void)MovementGrid(invalid, {0, 0}, {});
        });
    invalid = board;
    invalid.terrain[0] = 3;
    rejects(
        [&]
        {
            (void)MovementGrid(invalid, {0, 0}, {});
        });

    board.terrain[4] = 2;
    const std::vector<Occupant> ally{{{1, 1}, false}};
    grid = MovementGrid(board, {0, 1}, ally);
    check(grid.step_cost({0, 1}, {1, 1}) == 10,
          "Allied space retains difficult terrain cost without an occupancy surcharge");
    check(!grid.can_stop_at({1, 1}), "Ally cannot be a destination");
    const std::vector<Occupant> surrounded{{{0, 0}, false}, {{1, 0}, false}, {{2, 0}, false},
                                           {{0, 1}, false}, {{2, 1}, false}, {{0, 2}, false},
                                           {{1, 2}, false}, {{2, 2}, false}};
    const MovementGrid trapped(Battlefield{3, 3, std::vector<std::uint8_t>(9)}, {1, 1}, surrounded);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            check(!trapped.reachable(30).cost_to({x, y}),
                  "Surrounded character has no highlighted moves");
    // Search results and grids own their data; later caller mutations cannot
    // change either a previously computed path or a pending search's geometry.
    const auto reachable = grid.reachable(15);
    board.terrain.assign(9, 1);
    check(reachable.cost_to({2, 1}) == 10 && grid.reachable(15).cost_to({2, 1}) == 10,
          "Grid snapshots retain independent value ownership");
}
} // namespace

int main()
{
    try
    {
        allied_transit();
        boundaries_and_ties();
        exhaustive_movement();
        exhaustive_sight();
        std::cout
            << "Combat grid: 9,686 exhaustive movement layouts and 41,472 sight cases passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
