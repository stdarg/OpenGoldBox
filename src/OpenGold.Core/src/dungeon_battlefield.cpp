#include "opengold/dungeon_battlefield.h"
#include <array>
#include <stdexcept>

namespace opengold::por
{
namespace
{
enum Edge : unsigned
{
    open = 0,
    wall = 1,
    doorway = 3
};

constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};

Edge side(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    if (x < 0 || x >= 16 || y < 0 || y >= 16)
        return y == static_cast<int>(party_y) && (direction == 1 || direction == 3) ? open : wall;
    const auto &cell = map.at(x, y);
    if (!cell.walls[direction])
        return open;
    return cell.doors[direction] ? doorway : wall;
}

Edge edge(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    return static_cast<Edge>(
        side(map, x, y, direction, party_y) |
        side(map, x + dx[direction], y + dy[direction], (direction + 2) % 4, party_y));
}

// Functional tile patterns observed with synthetic inputs; see combat-geometry.md.
// Zero means untouched. Values are the original arena's one-based tile indices.
std::array<std::uint8_t, 35> cell_tiles(Edge west, Edge north, Edge east, Edge upper_west,
                                        Edge left_north, Edge upper_east, Edge right_north)
{
    std::array<std::uint8_t, 35> tiles{};
    const auto put = [&](int x, int y, std::uint8_t tile)
    {
        tiles[y * 7 + x] = tile;
    };
    for (int y = 0; y < 5; ++y)
        for (int x = (y < 2 ? 1 : 0); x < (y < 2 ? 7 : 6); ++x)
            put(x, y, 23);
    if (west == wall)
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
    else if (west == doorway)
    {
        put(1, 0, 14);
        put(1, 1, 21);
        put(1, 2, 9);
        put(5, 4, 1);
    }
    if (north != open)
    {
        put(1, 0, west == open ? 16 : 19);
        put(1, 1, west == open ? 17 : west == wall ? 2 : 21);
        put(6, 0, east == open ? 18 : 10);
        put(6, 1, east == open ? 24 : 15);
        if (north == wall)
        {
            for (int x = 2; x < 6; ++x)
            {
                put(x, 0, 6);
                put(x, 1, 11);
            }
            if (west == wall)
                put(2, 1, 7);
        }
        else
        {
            put(2, 0, 18);
            put(2, 1, west == wall ? 22 : 24);
            put(5, 0, 16);
            put(5, 1, 17);
        }
    }
    if (upper_west != open || left_north != open)
    {
        if (west == open)
        {
            if (north != open)
            {
                put(1, 0, 6);
                put(1, 1, 11);
            }
        }
        else
        {
            put(1, 0, north == open ? 14 : 3);
            put(1, 1, west == wall ? 4 : 8);
        }
    }
    if (north != open && east == open)
    {
        put(6, 0, right_north != open ? 6 : upper_east != open ? 20 : 18);
        put(6, 1, right_north != open ? 11 : 24);
    }
    else if (north == open && upper_east != open)
    {
        put(5, 0, upper_east == wall ? 5 : 23);
        put(6, 0, upper_east == wall ? 4 : 2);
        put(6, 1, east != open ? 5 : right_north != open ? 13 : 9);
        if (east == open && right_north != open)
            put(6, 0, upper_east == wall ? 12 : 25);
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
} // namespace

DungeonBattlefield dungeon_battlefield(const GeoMap &map, unsigned x, unsigned y)
{
    if (x >= 16 || y >= 16)
        throw std::out_of_range("Dungeon battlefield requires a valid exploration position");
    DungeonBattlefield result{{50, 25, std::vector<std::uint8_t>(1250, 0)},
                              std::vector<std::uint8_t>(1250, 22)};
    for (int row = -2; row <= 2; ++row)
        for (int column = -6; column <= 6; ++column)
        {
            const int gx = static_cast<int>(x) + column, gy = static_cast<int>(y) + row;
            const auto stamp = cell_tiles(edge(map, gx, gy, 3, y), edge(map, gx, gy, 0, y),
                                          edge(map, gx, gy, 1, y), edge(map, gx, gy - 1, 3, y),
                                          edge(map, gx - 1, gy, 0, y), edge(map, gx, gy - 1, 1, y),
                                          edge(map, gx + 1, gy, 0, y));
            for (int sy = 0; sy < 5; ++sy)
                for (int sx = 0; sx < 7; ++sx)
                {
                    const auto tile = stamp[sy * 7 + sx];
                    if (!tile)
                        continue;
                    const rules::Cell cell{21 + 6 * column + 5 * row + sx, 10 + 5 * row + sy};
                    if (!result.geometry.contains(cell))
                        continue;
                    const auto index = cell.y * 50 + cell.x;
                    result.tiles[index] = tile - 1;
                    result.geometry.terrain[index] = blocked(tile) ? 1 : 0;
                }
        }
    return result;
}
} // namespace opengold::por
