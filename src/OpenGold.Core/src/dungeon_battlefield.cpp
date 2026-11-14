#include "opengold/dungeon_battlefield.h"
#include <array>
#include <stdexcept>

namespace opengold::por
{
namespace
{
using Edge = FieldEdge;

// Both squares' sides of one edge: a wall on either side makes a wall, a
// doorway on either a doorway (their bits 1 and 3).
Edge combined(Edge first, Edge second)
{
    return static_cast<Edge>(static_cast<unsigned>(first) | static_cast<unsigned>(second));
}

constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};

Edge side(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    if (x < 0 || x >= 16 || y < 0 || y >= 16)
    {
        return y == static_cast<int>(party_y) && (direction == 1 || direction == 3) ? Edge::open
                                                                                    : Edge::wall;
    }
    const auto &cell = map.at(x, y);
    if (!cell.walls[direction])
    {
        return Edge::open;
    }
    return cell.doors[direction] ? Edge::doorway : Edge::wall;
}

Edge edge(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    return combined(side(map, x, y, direction, party_y),
                    side(map, x + dx[direction], y + dy[direction], (direction + 2) % 4, party_y));
}

// Functional tile patterns observed with synthetic inputs; see combat-geometry.md.
// Zero means untouched. Values are the original arena's one-based tile indices.
std::array<std::uint8_t, 35> cell_tiles(FieldLayout layout, Edge west, Edge north, Edge east,
                                        Edge upper_west, Edge left_north, Edge upper_east,
                                        Edge right_north)
{
    std::array<std::uint8_t, 35> tiles{};
    const auto put = [&](int x, int y, std::uint8_t tile)
    {
        tiles[y * 7 + x] = tile;
    };
    for (int y = 0; y < 5; ++y)
    {
        for (int x = (y < 2 ? 1 : 0); x < (y < 2 ? 7 : 6); ++x)
        {
            put(x, y, 23);
        }
    }
    if (west == Edge::wall)
    {
        put(1, 0, 1);
        put(1, 1, 2);
        put(2, 1, 14);
        for (int y = 2; y < 5; ++y)
        {
            put(y - 1, y, 5);
            put(y, y, 4);
            put(y + 1, y, 14);
        }
    }
    else if (west == Edge::doorway)
    {
        put(1, 0, 14);
        put(1, 1, 21);
        put(1, 2, 9);
        put(5, 4, 1);
    }
    if (north != Edge::open)
    {
        put(1, 0, west == Edge::open ? 16 : 19);
        put(1, 1, west == Edge::open ? 17 : west == Edge::wall ? 2 : 21);
        put(6, 0, east == Edge::open ? 18 : 10);
        put(6, 1, east == Edge::open ? 24 : 15);
        if (north == Edge::wall)
        {
            for (int x = 2; x < 6; ++x)
            {
                put(x, 0, 6);
                put(x, 1, 11);
            }
            if (west == Edge::wall)
            {
                put(2, 1, 7);
            }
        }
        else
        {
            put(2, 0, 18);
            put(2, 1, west == Edge::wall ? 22 : 24);
            put(5, 0, 16);
            put(5, 1, 17);
        }
    }
    if (upper_west != Edge::open || left_north != Edge::open)
    {
        if (west == Edge::open)
        {
            if (north != Edge::open)
            {
                put(1, 0, 6);
                put(1, 1, 11);
            }
        }
        else
        {
            put(1, 0, north == Edge::open ? 14 : 3);
            put(1, 1, west == Edge::wall ? 4 : 8);
        }
    }
    if (north != Edge::open && east == Edge::open)
    {
        put(6, 0, right_north != Edge::open ? 6 : upper_east != Edge::open ? 20 : 18);
        put(6, 1, right_north != Edge::open ? 11 : 24);
    }
    else if (north == Edge::open && upper_east != Edge::open)
    {
        put(5, 0, upper_east == Edge::wall ? 5 : 23);
        put(6, 0, upper_east == Edge::wall ? 4 : 2);
        put(6, 1, east != Edge::open ? 5 : right_north != Edge::open ? 13 : 9);
        if (east == Edge::open && right_north != Edge::open)
        {
            put(6, 0, upper_east == Edge::wall ? 12 : 25);
        }
        // Where the wall from the north ends at the corner, the kit draws its
        // end piece (cmbsetup.c, block_ne).
        if (layout == FieldLayout::kit && east == Edge::open && right_north == Edge::open)
        {
            put(6, 0, 8);
        }
    }
    return tiles;
}

bool blocked(unsigned tile)
{
    switch (tile)
    {
    case 5:
    case 9:
    case 11:
    case 13:
    case 15:
    case 17:
    case 23:
    case 24:
        return false;
    default:
        return true;
    }
}

// The field around the party's square from the sides `edge_of` gives, by
// column, row and direction from that square; `sky_of` says which squares are
// open to the sky.
template <typename EdgeOf, typename SkyOf>
DungeonBattlefield laid_out_field(FieldLayout layout, const EdgeOf &edge_of, const SkyOf &sky_of)
{
    DungeonBattlefield result{{50, 25, std::vector<rules::Terrain>(1250), std::vector<bool>(1250)},
                              std::vector<std::uint8_t>(1250, 22)};
    for (int row = -2; row <= 2; ++row)
    {
        for (int column = -6; column <= 6; ++column)
        {
            for (unsigned direction = 0; direction < 4; ++direction)
            {
                result.walled[row + 2][column + 6][direction] =
                    edge_of(column, row, direction) == Edge::wall;
            }
            const bool sky = sky_of(column, row);
            const auto stamp = cell_tiles(layout, edge_of(column, row, 3), edge_of(column, row, 0),
                                          edge_of(column, row, 1), edge_of(column, row - 1, 3),
                                          edge_of(column - 1, row, 0), edge_of(column, row - 1, 1),
                                          edge_of(column + 1, row, 0));
            for (int sy = 0; sy < 5; ++sy)
            {
                for (int sx = 0; sx < 7; ++sx)
                {
                    const auto tile = stamp[sy * 7 + sx];
                    if (!tile)
                    {
                        continue;
                    }
                    const rules::Cell cell{21 + 6 * column + 5 * row + sx, 10 + 5 * row + sy};
                    if (!result.geometry.contains(cell))
                    {
                        continue;
                    }
                    const auto index = cell.y * 50 + cell.x;
                    result.tiles[index] = tile - 1;
                    result.geometry.plants[index] = sky;
                    result.geometry.terrain[index] =
                        blocked(tile) ? rules::Terrain::obstacle : rules::Terrain::open;
                }
            }
        }
    }
    return result;
}
} // namespace

DungeonBattlefield dungeon_battlefield(const GeoMap &map, unsigned x, unsigned y)
{
    if (x >= 16 || y >= 16)
    {
        throw std::out_of_range("Dungeon battlefield requires a valid exploration position");
    }
    const int party_x = static_cast<int>(x), party_y = static_cast<int>(y);
    return laid_out_field(
        FieldLayout::dax,
        [&](int column, int row, unsigned direction)
        {
            return edge(map, party_x + column, party_y + row, direction, y);
        },
        [&](int column, int row)
        {
            // A square open to the sky has ordinary plants (Plant Growth); the
            // original marks a roofed square with its event byte's high bit.
            const int gx = party_x + column, gy = party_y + row;
            return gx >= 0 && gy >= 0 && gx < 16 && gy < 16 &&
                   !map.at(static_cast<unsigned>(gx), static_cast<unsigned>(gy)).event_high_bit();
        });
}

DungeonBattlefield kit_dungeon_battlefield(const FieldEdges &edges)
{
    return laid_out_field(FieldLayout::kit, edges,
                          [](int, int)
                          {
                              return false;
                          });
}
} // namespace opengold::por
