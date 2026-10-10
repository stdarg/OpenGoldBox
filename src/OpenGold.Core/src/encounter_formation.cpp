#include "opengold/encounter_formation.h"
#include <array>
#include <set>

// The rules below were read from the Gold Box engine as Simeon Pilgrim's Curse
// of the Azure Bonds reimplementation (coab) documents it; Pool of Radiance's
// START.EXE carries the same placement tables. No code is copied.
namespace opengold::por
{
namespace
{
using rules::Cell;

// Each side is laid out on an 11 by 6 grid over one exploration square. Grid
// (column, row) of the square (dx, dy) from the party's is arena square
// (22 + column + 6 dx + 5 dy, 10 + row + 5 dy): the arena's rows are sheared.
constexpr int grid_columns = 11, grid_rows = 6;

Cell arena_cell(Cell grid, Cell square)
{
    return {22 + grid.x + 6 * square.x + 5 * square.y, 10 + grid.y + 5 * square.y};
}

// One step on the grid towards each exploration direction (north, east,
// south, west). The shear makes north up and to the left.
constexpr std::array<Cell, 4> grid_step{{{-1, -1}, {1, 0}, {1, 1}, {-1, 0}}};

struct Columns
{
    int first, last; // Empty when first > last.
};

using GridShape = std::array<Columns, grid_rows>;

// The half of its own square at the back of a side facing north, east, south
// or west, then the whole square.
constexpr std::array<GridShape, 5> shapes{{
        {{{1, 0}, {1, 0}, {1, 0}, {2, 9}, {3, 10}, {4, 10}}},
        {{{0, 2}, {0, 3}, {1, 4}, {2, 5}, {3, 6}, {4, 7}}},
        {{{0, 6}, {0, 7}, {1, 8}, {1, 0}, {1, 0}, {1, 0}}},
        {{{3, 6}, {4, 7}, {5, 8}, {6, 9}, {7, 10}, {8, 10}}},
        {{{0, 6}, {0, 7}, {1, 8}, {2, 9}, {3, 10}, {4, 10}}},
    }
};
constexpr std::size_t whole_square = 4;

// Where a side's front rank is centred: in its own square, by its facing, and
// in a square it spills into, by the facing it fills that square with.
constexpr std::array<Cell, 4> own_front{{{5, 3}, {4, 2}, {5, 2}, {6, 3}}};
constexpr std::array<Cell, 4> spill_front{{{3, 0}, {8, 2}, {7, 5}, {2, 3}}};

// The squares a crowded side spills into, in order: behind it, then beside it.
constexpr std::array<std::array<MapDirection, 3>, 4> spill_order{{
        {MapDirection::south, MapDirection::west, MapDirection::east},
        {MapDirection::west, MapDirection::south, MapDirection::north},
        {MapDirection::north, MapDirection::west, MapDirection::east},
        {MapDirection::east, MapDirection::north, MapDirection::south},
    }
};

// A rank after the first is at most this long, counting squares tried.
constexpr int longest_rank = 12;

// One exploration square a side may stand in.
struct Area
{
    Cell square;        // From the party's square, in exploration squares.
    MapDirection fills; // The facing its ranks are formed with.
    Cell front;         // Grid square of the first rank's centre.
    std::size_t shape;  // Index into `shapes`.
};

struct Side
{
    Cell square;
    MapDirection facing;
    std::size_t size;
    bool is_party;
};

bool walled(const DungeonBattlefield &field, Cell square, MapDirection side)
{
    return field.walled[square.y + 2][square.x + 6][index(side)];
}

// The squares the side may stand in, in the order it fills them. A spill
// square fills facing back towards the side's own square, so its ranks start
// at the edge they share.
std::vector<Area> areas(const DungeonBattlefield &field, const Side &side)
{
    std::vector<Area> result{
        {side.square, side.facing, own_front[index(side.facing)], index(side.facing)}};
    for (const auto direction : spill_order[index(side.facing)])
    {
        if (walled(field, side.square, direction))
            continue;
        const Cell square{side.square.x + step_x(direction), side.square.y + step_y(direction)};
        const auto fills = reversed(direction);
        const bool behind = direction == reversed(side.facing);
        result.push_back({square, fills, spill_front[index(fills)],
                          behind ? whole_square : index(side.facing)});
    }
    return result;
}

bool usable(const Area &area, Cell grid)
{
    const auto columns = shapes[area.shape][static_cast<std::size_t>(grid.y)];
    return grid.x >= columns.first && grid.x <= columns.last;
}

// The n-th square tried in a rank (from 1): its centre, then alternately one
// more step to the side's right and to its left.
Cell rank_position(Cell centre, MapDirection fills, int n)
{
    const auto right = grid_step[index(turned_right(fills))];
    const auto left = grid_step[index(turned_left(fills))];
    const auto steps = n / 2;
    if (n == 1)
        return centre;
    if (n % 2 == 0)
        return {centre.x + right.x * steps, centre.y + right.y * steps};
    return {centre.x + left.x * steps, centre.y + left.y * steps};
}

// Every arena square the side may take, in the order the original tries them.
// A rank ends at the grid's edge or when long enough: the first rank holds
// half the side (rounded up). A rank that starts or runs beyond the grid's
// corner ends the square. A party facing east or west that has a square to
// spill into leaves one rank empty behind its first.
std::vector<Cell> slots(const DungeonBattlefield &field, const Side &side)
{
    const auto spills = areas(field, side);
    const int first_rank = static_cast<int>((side.size + 1) / 2);
    const bool facing_sideways = side.facing == MapDirection::east ||
                                 side.facing == MapDirection::west;
    bool first = true;
    std::vector<Cell> result;
    for (const auto &area : spills)
    {
        const bool own_square = &area == &spills.front();
        const auto back = grid_step[index(reversed(area.fills))];
        bool square_done = false;
        for (int rank = 0; !square_done;)
        {
            const Cell centre{area.front.x + back.x * rank, area.front.y + back.y * rank};
            for (int n = 1;; ++n)
            {
                const auto grid = rank_position(centre, area.fills, n);
                const bool column_inside = grid.x >= 0 && grid.x < grid_columns;
                const bool row_inside = grid.y >= 0 && grid.y < grid_rows;
                if (!column_inside && !row_inside)
                {
                    square_done = true;
                    break;
                }
                if (!column_inside || !row_inside)
                    break;
                if (usable(area, grid))
                    result.push_back(arena_cell(grid, area.square));
                if (n >= (first ? first_rank : longest_rank))
                    break;
            }
            ++rank;
            if (side.is_party && facing_sideways && own_square && rank == 1 && spills.size() > 1)
                ++rank;
            first = false;
        }
    }
    return result;
}

// Gives each member the first of the side's slots that is open and free.
std::vector<std::optional<Cell>> place(const DungeonBattlefield &field, const Side &side,
                                       std::set<Cell> &occupied)
{
    const auto candidates = slots(field, side);
    std::vector<std::optional<Cell>> result;
    for (std::size_t member = 0; member < side.size; ++member)
    {
        auto &cell = result.emplace_back();
        for (const auto candidate : candidates)
            if (field.geometry.at(candidate) != rules::Terrain::obstacle &&
                    !occupied.contains(candidate))
            {
                cell = candidate;
                occupied.insert(candidate);
                break;
            }
    }
    return result;
}
} // namespace

EncounterFormation encounter_formation(const DungeonBattlefield &field, MapDirection facing,
                                       unsigned distance, std::size_t party_size,
                                       std::size_t enemy_count)
{
    const int ahead = static_cast<int>(distance);
    const Side party{{0, 0}, facing, party_size, true};
    const Side enemies{{step_x(facing) * ahead, step_y(facing) * ahead}, reversed(facing),
                       enemy_count, false};
    std::set<Cell> occupied;
    auto placed_party = place(field, party, occupied);
    return {std::move(placed_party), place(field, enemies, occupied)};
}
} // namespace opengold::por
