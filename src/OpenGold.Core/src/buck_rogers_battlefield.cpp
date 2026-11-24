#include "opengold/buck_rogers_battlefield.h"

#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace opengold::por
{
namespace
{
constexpr int columns = 50, rows = 25;
using Cells = std::array<std::array<std::uint8_t, columns>, rows>;

// The kinds of combat terrain, from 1 (gdconsts.pas combatTerrain): cost,
// line class and tile slot.
constexpr std::array<CombatTerrain, combat_terrain_kinds> terrains{{
    {1, 0, 0},    {1, 0, 1},    {1, 0, 2},    {1, 0, 3},    {1, 0, 4},    {1, 0, 5},
    {1, 0, 0},    {255, 1, 1},  {1, 0, 2},    {255, 1, 3},  {255, 1, 4},  {255, 1, 5},
    {1, 0, 6},    {255, 1, 7},  {255, 1, 8},  {255, 1, 9},  {255, 1, 10}, {255, 1, 11},
    {1, 0, 12},   {255, 1, 13}, {255, 1, 14}, {255, 1, 15}, {255, 1, 16}, {1, 0, 17},
    {255, 1, 18}, {1, 0, 19},   {255, 1, 20}, {1, 0, 21},   {255, 1, 22}, {255, 1, 23},
    {255, 1, 24}, {255, 1, 25}, {1, 0, 26},   {1, 0, 27},   {255, 1, 28}, {1, 0, 29},
    {255, 1, 30}, {1, 0, 31},   {1, 0, 32},   {1, 0, 33},   {255, 1, 49}, {255, 1, 50},
    {255, 1, 51}, {255, 1, 52}, {1, 0, 34},   {2, 2, 35},   {2, 2, 36},   {2, 2, 37},
    {2, 2, 38},   {2, 2, 39},   {2, 2, 40},   {2, 2, 41},   {2, 2, 42},   {2, 2, 43},
    {2, 2, 44},   {1, 0, 45},   {1, 0, 46},   {1, 0, 47},   {1, 0, 48},   {2, 0, 34},
    {1, 0, 35},   {2, 2, 36},   {2, 2, 37},   {2, 2, 38},   {2, 2, 39},   {2, 2, 40},
    {2, 2, 41},   {2, 2, 42},   {255, 1, 1},  {255, 1, 2},  {255, 1, 3},  {255, 1, 4},
    {255, 1, 5},  {255, 1, 6},  {255, 1, 7},  {255, 1, 8},  {255, 1, 9},  {255, 1, 10},
    {255, 2, 11}, {255, 2, 12}, {255, 2, 13}, {4, 2, 14},   {4, 2, 15},   {4, 2, 16},
    {4, 2, 17},   {4, 2, 18},   {4, 2, 19},   {4, 2, 20},   {1, 0, 21},   {1, 0, 22},
    {2, 0, 23},   {255, 0, 24}, {255, 0, 25}, {3, 2, 26},   {3, 2, 27},   {3, 2, 28},
    {3, 2, 29},   {1, 2, 30},   {1, 2, 31},   {255, 2, 32}, {255, 2, 33}, {3, 2, 1},
    {3, 2, 2},    {3, 2, 3},    {3, 2, 4},    {3, 2, 5},    {3, 2, 6},    {1, 0, 7},
    {1, 0, 8},    {255, 0, 9},  {255, 0, 10}, {2, 0, 11},   {2, 0, 12},   {2, 0, 13},
    {3, 2, 14},   {3, 2, 15},   {3, 0, 16},   {255, 2, 17}, {255, 2, 18}, {255, 2, 19},
    {255, 2, 20}, {255, 2, 21}, {255, 2, 22}, {255, 2, 23}, {255, 2, 24}, {255, 2, 25},
    {1, 2, 26},   {1, 2, 27},   {1, 2, 28},   {1, 2, 29},   {2, 0, 34},   {2, 0, 35},
    {2, 0, 36},   {2, 0, 37},   {1, 2, 38},   {1, 2, 39},   {255, 2, 40}, {255, 2, 41},
    {1, 0, 42},   {2, 0, 30},   {2, 0, 31},   {2, 2, 42},   {2, 0, 33},   {2, 0, 34},
    {2, 0, 35},   {1, 0, 36},   {2, 2, 37},   {2, 0, 38},   {255, 0, 39}, {255, 2, 40},
    {1, 2, 41},   {255, 0, 32}, {2, 0, 43},   {2, 0, 44},
}};

// The tiles of a square's left and right corners, rows 0 and 1, by the walls
// meeting there as a state of 1 to 15 (gdconsts.pas leftWallTiles,
// rightWallTiles); 255 leaves the cell.
using CornerTiles = std::array<std::array<std::array<std::uint8_t, 2>, 2>, 15>;
constexpr CornerTiles left_corner_tiles{{
    {{{24, 24}, {25, 25}}},
    {{{36, 255}, {28, 26}}},
    {{{7, 24}, {9, 10}}},
    {{{31, 255}, {32, 255}}},
    {{{24, 24}, {25, 25}}},
    {{{14, 255}, {13, 26}}},
    {{{11, 24}, {13, 10}}},
    {{{34, 255}, {38, 255}}},
    {{{17, 24}, {18, 25}}},
    {{{26, 255}, {28, 26}}},
    {{{15, 24}, {28, 10}}},
    {{{22, 255}, {23, 255}}},
    {{{21, 24}, {25, 25}}},
    {{{16, 255}, {13, 26}}},
    {{{40, 24}, {13, 10}}},
}};
constexpr CornerTiles right_corner_tiles{{
    {{{255, 31}, {255, 33}}},
    {{{255, 35}, {255, 39}}},
    {{{255, 255}, {255, 8}}},
    {{{24, 24}, {25, 25}}},
    {{{24, 24}, {25, 25}}},
    {{{24, 24}, {25, 12}}},
    {{{24, 24}, {25, 12}}},
    {{{27, 29}, {255, 37}}},
    {{{27, 28}, {255, 27}}},
    {{{27, 28}, {255, 27}}},
    {{{27, 28}, {255, 27}}},
    {{{19, 20}, {25, 25}}},
    {{{19, 20}, {25, 25}}},
    {{{19, 20}, {25, 12}}},
    {{{19, 20}, {25, 12}}},
}};

// The original's dice: Turbo Pascal's generator (its System.Random), which
// the indoor arena seeds with the script's number (combatst.pas ReseedRandom).
class PascalRandom
{
  public:
    explicit PascalRandom(std::uint32_t seed) noexcept : seed_(seed)
    {
    }

    // 0 to count - 1.
    [[nodiscard]] unsigned below(unsigned count) noexcept
    {
        step();
        return static_cast<unsigned>((static_cast<std::uint64_t>(seed_) * count) >> 32);
    }

    // 0 to just under 1.
    [[nodiscard]] double unit() noexcept
    {
        step();
        return static_cast<double>(seed_) / 4294967296.0;
    }

    // count dice of `sides`, as a byte (charhlp.pas RollDice).
    [[nodiscard]] unsigned dice(unsigned count, unsigned sides) noexcept
    {
        unsigned total = 0;
        for (unsigned n = 0; n < count; ++n)
        {
            total += below(sides) + 1;
        }
        return total & 0xFF;
    }

  private:
    void step() noexcept
    {
        seed_ = seed_ * 134775813U + 1;
    }

    std::uint32_t seed_;
};

// The eight directions from north, clockwise (gdconsts.pas dirStepX, dirStepY).
constexpr unsigned north = 0, east = 2, west = 6;
constexpr std::array<int, 8> step_x{0, 1, 1, 1, 0, -1, -1, -1}, step_y{-1, -1, 0, 1, 1, 1, 0, -1};

// The wall kind on a square's side (combatst.pas WallBetween's GetWallKind):
// 0 open, 1 a wall no one passes, 3 a wall with a way through. Off the map
// a side is a wall, but east and west along the team's row.
unsigned side_kind(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    if (x < 0 || y < 0 || x >= 16 || y >= 16)
    {
        return y == static_cast<int>(party_y) && (direction == east || direction == west) ? 0 : 1;
    }
    const auto &cell = map.at(static_cast<unsigned>(x), static_cast<unsigned>(y));
    const auto side = direction / 2;
    if (cell.walls[side] == 0)
    {
        return 0;
    }
    return cell.doors[side] == 0 ? 1 : 3;
}

unsigned wall_between(const GeoMap &map, int x, int y, unsigned direction, unsigned party_y)
{
    const auto opposite = (direction + 4) % 8;
    return side_kind(map, x, y, direction, party_y) |
           side_kind(map, x + step_x[direction], y + step_y[direction], opposite, party_y);
}

// The indoor arena (combatst.pas BuildDungeonArena): floor, the decoration
// mode bit 40h scatters, and each map square within 6 columns and 2 rows of
// the team's laid from its walls.
Cells dungeon_arena(const GeoMap &map, unsigned x, unsigned y, unsigned kind, unsigned script)
{
    PascalRandom random(script);
    Cells cells{};
    const bool low_tech = kind == 0x88 || kind == 0x82;
    for (auto &row : cells)
    {
        row.fill(low_tech ? 7 : 45);
    }
    int ox = 0, oy = 0;
    const auto place = [&](int px, int py) -> std::uint8_t *
    {
        const int column = ox * 6 + oy * 5 + 21 + px, row = oy * 5 + 10 + py;
        return column >= 0 && column < columns && row >= 0 && row < rows ? &cells[row][column]
                                                                         : nullptr;
    };
    const auto plot = [&](int px, int py, unsigned value)
    {
        if (auto *cell = place(px, py))
        {
            *cell = static_cast<std::uint8_t>(value + 1);
        }
    };
    // Onto floor only (ScatterWallTiles' PlaceTileOn).
    const auto plot_on_floor = [&](int px, int py, unsigned value)
    {
        if (auto *cell = place(px, py); cell && (*cell == 7 || *cell == 45))
        {
            plot(px, py, value);
        }
    };
    if ((kind & 0x40) != 0)
    {
        for (oy = -2; oy <= 2; ++oy)
        {
            for (ox = -6; ox <= 6; ++ox)
            {
                if (random.below(2) == 0)
                {
                    plot(4, 2, 55);
                    plot(5, 2, 56);
                    plot(5, 3, 57);
                    plot(6, 3, 58);
                }
            }
        }
    }
    const int party_x = static_cast<int>(x), party_y = static_cast<int>(y);
    const auto between = [&](int mx, int my, unsigned direction)
    {
        return wall_between(map, mx, my, direction, y);
    };
    for (oy = -2; oy <= 2; ++oy)
    {
        for (ox = -6; ox <= 6; ++ox)
        {
            const int mx = party_x + ox, my = party_y + oy;
            const auto wall_west = between(mx, my, west), wall_north = between(mx, my, north),
                       wall_east = between(mx, my, east);
            if (wall_west == 1)
            {
                for (int i = 2; i <= 4; ++i)
                {
                    plot(i - 1, i, 27);
                    plot(i, i, 28);
                    plot(i + 1, i, 26);
                }
            }
            else if (wall_west == 3)
            {
                plot(1, 2, 27);
                plot(2, 2, 29);
                plot(3, 2, 34);
                plot(2, 3, 37);
                plot(3, 3, 38);
                plot(4, 4, 35);
                plot(5, 4, 36);
            }
            if (wall_north == 1)
            {
                plot(3, 0, 24);
                plot(4, 0, 24);
                plot(3, 1, 25);
                plot(4, 1, 25);
            }
            unsigned state = (wall_north != 0 ? 1U : 0U) + (wall_west != 0 ? 2U : 0U) +
                             (between(mx - 1, my, north) != 0 ? 4U : 0U) +
                             (between(mx, my - 1, west) != 0 ? 8U : 0U);
            for (unsigned row = 0; state > 0 && row < 2; ++row)
            {
                for (unsigned column = 0; column < 2; ++column)
                {
                    if (const auto tile = left_corner_tiles[state - 1][row][column]; tile < 255)
                    {
                        plot(static_cast<int>(column) + 1, static_cast<int>(row), tile);
                    }
                }
            }
            const auto north_east = between(mx, my - 1, east);
            state = (between(mx + 1, my, north) != 0 ? 1U : 0U) + (wall_east != 0 ? 2U : 0U) +
                    (wall_north != 0 ? 4U : 0U) + (north_east != 0 ? 8U : 0U);
            for (unsigned row = 0; state > 0 && row < 2; ++row)
            {
                for (unsigned column = 0; column < 2; ++column)
                {
                    if (const auto tile = right_corner_tiles[state - 1][row][column]; tile < 255)
                    {
                        plot(static_cast<int>(column) + 5, static_cast<int>(row), tile);
                    }
                }
            }
            if (wall_north == 3)
            {
                plot(2, 0, 30);
                plot(2, 1, wall_west == 0 ? 32 : 42);
                plot(5, 0, north_east == 0 ? 31 : 43);
                plot(5, 1, 33);
            }
            if (north_east == 3)
            {
                plot(5, 0, wall_north == 0 ? 39 : 41);
            }
            // The square's event bit 40h chooses the other decoration
            // (View3D's GetMapEvent, which wraps round the map).
            const auto &square = map.at(static_cast<unsigned>((mx % 16 + 16) % 16),
                                        static_cast<unsigned>((my % 16 + 16) % 16));
            const bool variant = (square.event_raw & 0x40) != 0;
            // The walls' decoration (ScatterWallTiles).
            const auto roll = [&](unsigned below)
            {
                return random.below(below);
            };
            if ((kind & 8) == 0)
            {
                if (wall_north == 1)
                {
                    for (int i = 4; i <= 6; ++i)
                    {
                        if (roll(10) < 3)
                        {
                            plot_on_floor(i, 2, roll(2) + 51);
                        }
                    }
                }
                if (wall_east == 1 && variant)
                {
                    for (int j = 2; j <= 4; ++j)
                    {
                        if (roll(10) < 5)
                        {
                            static constexpr std::array<unsigned, 3> pieces{46, 49, 50};
                            const auto piece = roll(3);
                            if (roll(10) < 8)
                            {
                                plot_on_floor(j + 4, j, pieces[piece]);
                                if (roll(10) < 5)
                                {
                                    plot_on_floor(j + 3, j, 47);
                                }
                            }
                        }
                        else
                        {
                            static constexpr std::array<unsigned, 6> pieces{45, 46, 48, 49, 53, 54};
                            const auto piece = roll(6);
                            if (roll(10) < 8)
                            {
                                plot_on_floor(j + 4, j, pieces[piece]);
                            }
                        }
                    }
                }
                else if (wall_east == 1)
                {
                    for (int j = 2; j <= 4; ++j)
                    {
                        if (roll(10) < 8)
                        {
                            plot_on_floor(j + 4, j, roll(2) == 0 ? 45 : 54);
                        }
                    }
                }
            }
            else
            {
                if (wall_north == 1)
                {
                    for (int i = 4; i <= 6; ++i)
                    {
                        if (roll(10) < 8)
                        {
                            static constexpr std::array<unsigned, 6> varied{59, 60, 62, 63, 66, 68};
                            static constexpr std::array<unsigned, 3> plain{62, 66, 67};
                            plot_on_floor(i, 2, variant ? varied[roll(6)] : plain[roll(3)]);
                        }
                    }
                }
                if (wall_east == 1)
                {
                    for (int j = 2; j <= 4; ++j)
                    {
                        if (roll(10) >= 8)
                        {
                            continue;
                        }
                        if (!variant)
                        {
                            // The original lays these on the north row, as
                            // its other branch for this wall does not.
                            static constexpr std::array<unsigned, 3> plain{62, 66, 67};
                            plot_on_floor(j + 2, 2, plain[roll(3)]);
                            continue;
                        }
                        static constexpr std::array<unsigned, 5> pieces{61, 62, 65, 66, 67};
                        const auto piece = roll(6);
                        if (piece < pieces.size())
                        {
                            plot_on_floor(j + 4, j, pieces[piece]);
                        }
                        else
                        {
                            plot_on_floor(j + 4, j, 63);
                            if (roll(10) < 5)
                            {
                                plot_on_floor(j + 3, j, 64);
                            }
                        }
                    }
                }
            }
        }
    }
    return cells;
}

// The wilderness scatter rules (gdconsts.pas terrainProfiles).
struct TerrainProfile
{
    std::uint8_t chance, dice_count, dice_sides, clearance, scatter, scatter_bonus, flags, tile,
        pair_tile, tile_variance, pair_variance;
};

constexpr std::uint8_t unset_terrain = 6;
constexpr std::uint8_t pair_below = 0x01, one_roll = 0x02, pair_follows = 0x04,
                       double_variance = 0x08, keep_center = 0x10, fixed_scatter = 0x20;

constexpr std::array<TerrainProfile, 22> profiles{{
    {50, 6, 12, 0, 12, 12, 0, 112, unset_terrain, 3, 0},
    {50, 4, 10, 0, 30, 0, 0, 112, unset_terrain, 3, 0},
    {80, 4, 10, 0, 10, 10, 0, 115, unset_terrain, 3, 0},
    {90, 3, 8, 0, 30, 0, 0, 115, unset_terrain, 3, 0},
    {80, 3, 8, 0, 12, 0, pair_below, 118, 130, 0, 0},
    {80, 4, 10, 0, 40, 0, pair_below, 119, 127, 4, 3},
    {80, 4, 20, 0, 10, 10, pair_below | one_roll, 119, 127, 8, 3},
    {80, 2, 12, 0, 20, 0, 0, 104, unset_terrain, 0, 0},
    {90, 4, 10, 0, 10, 5, one_roll, 102, unset_terrain, 6, 0},
    {80, 2, 5, 0, 8, 10, 0, 110, 111, 0, 0},
    {80, 2, 10, 1, 40, 0, pair_follows | double_variance, 69, 70, 5, 0},
    {80, 3, 8, 0, 15, 20, 0, 79, unset_terrain, 3, 0},
    {80, 4, 12, 0, 15, 20, 0, 82, unset_terrain, 7, 0},
    {100, 10, 20, 0, 9, 4, 0, 91, unset_terrain, 0, 0},
    {80, 5, 10, 0, 10, 10, pair_below | one_roll, 100, 98, 2, 2},
    {100, 1, 6, 0, 5, 0, keep_center, 92, 93, 0, 0},
    {80, 1, 10, 1, 5, 10, 0, 92, 93, 0, 0},
    {80, 5, 10, 0, 15, 15, one_roll, 94, unset_terrain, 4, 0},
    {100, 10, 8, 0, 0, 0, keep_center | fixed_scatter, 79, unset_terrain, 10, 0},
    {80, 3, 10, 0, 10, 10, pair_below | one_roll, 137, 135, 2, 2},
    {80, 3, 10, 0, 15, 15, one_roll, 131, unset_terrain, 4, 0},
    {80, 10, 10, 0, 15, 10, 0, 154, unset_terrain, 0, 0},
}};

// The ground of Mars and Venus (combatst.pas BuildWildernessArena,
// BuildWilderness, FillUnsetTerrain).
class WildernessArena
{
  public:
    explicit WildernessArena(std::uint32_t seed) : random_(seed)
    {
        for (auto &row : cells_)
        {
            row.fill(unset_terrain);
        }
    }

    // A profile's runs (BuildWilderness).
    void scatter(unsigned profile, unsigned count)
    {
        const auto &p = profiles.at(profile - 1);
        for (unsigned run = 0; run < count; ++run)
        {
            if (!(p.chance > random_.below(100)))
            {
                continue;
            }
            auto remaining = random_.dice(p.dice_count, p.dice_sides);
            bool first = true;
            unsigned spread = p.scatter;
            clearance_ = p.clearance;
            if ((p.flags & keep_center) == 0)
            {
                center_x_ = random_.dice(1, columns) - 1;
                center_y_ = random_.dice(1, rows) - 1;
            }
            if (p.scatter_bonus > 0)
            {
                spread += random_.dice(1, p.scatter_bonus);
            }
            if (scatter_override_ > 0)
            {
                spread = static_cast<unsigned>(scatter_override_);
            }
            unsigned tile = 0;
            pair_ = unset_terrain;
            while (remaining > 0)
            {
                if (first || (p.flags & one_roll) == 0)
                {
                    unsigned variance =
                        p.tile_variance > 0 ? random_.dice(1, p.tile_variance) - 1 : 0;
                    if ((p.flags & double_variance) != 0)
                    {
                        variance *= 2;
                    }
                    tile = (p.tile + variance) & 0xFF;
                    pair_ = p.pair_tile;
                    if (p.pair_tile != unset_terrain)
                    {
                        if ((p.flags & pair_follows) == 0)
                        {
                            variance =
                                p.pair_variance > 0 ? random_.dice(1, p.pair_variance) - 1 : 0;
                            if ((p.flags & double_variance) != 0)
                            {
                                variance *= 2;
                            }
                        }
                        pair_ = (pair_ + variance) & 0xFF;
                    }
                    first = false;
                }
                // The center is a byte: a plot west or north of it may wrap
                // round to a large number, which no cell is.
                unsigned x = center_x_, y = center_y_;
                if (spread > 0)
                {
                    const double distance =
                        (p.flags & fixed_scatter) != 0 ? spread : random_.dice(1, spread + 1) - 1;
                    const double angle = random_.unit() * 2 * std::numbers::pi;
                    x = (static_cast<unsigned>(
                             static_cast<int>(std::trunc(std::cos(angle) * distance))) +
                         center_x_) &
                        0xFF;
                    y = (static_cast<unsigned>(
                             static_cast<int>(std::trunc(std::sin(angle) * distance))) +
                         center_y_) &
                        0xFF;
                }
                if (placement_open(p, x, y))
                {
                    cells_[y][x] = static_cast<std::uint8_t>(tile);
                    if (pair_ != unset_terrain)
                    {
                        ((p.flags & pair_below) != 0 ? y : x) += 1;
                        if (y < rows && x < columns)
                        {
                            cells_[y][x] = static_cast<std::uint8_t>(pair_);
                        }
                    }
                }
                --remaining;
            }
        }
    }

    // Every cell still unset gets its ground (FillUnsetTerrain), column by column.
    void fill(unsigned base, unsigned spread)
    {
        for (int column = 0; column < columns; ++column)
        {
            for (int row = 0; row < rows; ++row)
            {
                if (cells_[row][column] == unset_terrain)
                {
                    cells_[row][column] = static_cast<std::uint8_t>(
                        base + (spread > 0 ? random_.dice(1, spread) - 1 : 0));
                }
            }
        }
    }

    void override_scatter(int value) noexcept
    {
        scatter_override_ = value;
    }

    [[nodiscard]] unsigned roll(unsigned sides) noexcept
    {
        return random_.dice(1, sides);
    }

    [[nodiscard]] const Cells &cells() const noexcept
    {
        return cells_;
    }

  private:
    // Unset, or the open ground 89 to 91, 108 and 109 (IsTerrainOpen); the
    // coordinates are read as the original's signed bytes.
    [[nodiscard]] bool open(int x, int y) const
    {
        const auto sx = static_cast<std::int8_t>(x & 0xFF), sy = static_cast<std::int8_t>(y & 0xFF);
        if (sx < 0 || sy < 0 || sx >= columns || sy >= rows)
        {
            return false;
        }
        const auto cell = cells_[sy][sx];
        return cell == unset_terrain || cell == 89 || cell == 90 || cell == 91 || cell == 108 ||
               cell == 109;
    }

    [[nodiscard]] bool placement_open(const TerrainProfile &p, unsigned x, unsigned y) const
    {
        const int cx = static_cast<int>(x), cy = static_cast<int>(y);
        if (clearance_ > 0)
        {
            const int c = static_cast<int>(clearance_);
            for (int dx = -c; dx <= c; ++dx)
            {
                for (int dy = -c; dy <= c; ++dy)
                {
                    if (!open(cx + dx, cy + dy))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
        if (!open(cx, cy))
        {
            return false;
        }
        if (pair_ != unset_terrain)
        {
            return (p.flags & pair_below) != 0 ? open(cx, cy + 1) : open(cx + 1, cy);
        }
        return true;
    }

    PascalRandom random_;
    Cells cells_{};
    unsigned center_x_{}, center_y_{}, clearance_{}, pair_{unset_terrain};
    int scatter_override_{-1};
};

Cells wilderness_arena(unsigned kind, unsigned ground, std::uint32_t seed)
{
    WildernessArena arena(seed);
    if ((kind & 0xF) == 8)
    {
        ground += 10;
    }
    using Runs = std::vector<std::pair<unsigned, unsigned>>;
    const auto run = [&arena](const Runs &runs)
    {
        for (const auto &[profile, count] : runs)
        {
            arena.scatter(profile, count);
        }
    };
    switch (ground)
    {
    case 1:
    case 8:
    case 9:
        run({{6, 6}, {1, 8}, {3, 1}, {4, 2}, {5, 2}, {8, 2}});
        break;
    case 2:
        run({{1, 6}, {3, 1}, {4, 1}, {5, 1}, {7, 5}, {8, 1}});
        break;
    case 3:
        run({{2, 1}, {4, 3}, {7, 10}, {10, 1}, {9, 5}, {22, 1}});
        break;
    case 4:
        run({{10, 10}, {2, 1}, {3, 1}, {4, 1}, {7, 3}, {9, 10}, {22, 2}});
        break;
    case 5:
        run({{10, 10}, {2, 1}, {3, 1}, {4, 1}, {7, 10}, {9, 15}, {22, 3}});
        break;
    case 6:
    case 7:
        run({{1, 1}, {2, 2}, {3, 1}, {4, 1}, {7, 2}, {8, 1}, {9, 1}});
        break;
    case 11:
    case 20:
        run({{13, 2}, {14, 1}, {16, 1}, {14, 2}, {18, 3}, {12, 2}});
        break;
    case 12:
    case 18:
        run({{11, 4}, {12, 4}, {13, 2}, {18, 2}, {15, 12}});
        break;
    case 13:
    case 19:
        run({{11, 3}, {12, 3}, {13, 2}, {14, 2}, {18, 4}, {15, 8}});
        break;
    case 14:
    case 21:
        for (int n = 0; n < 6; ++n)
        {
            arena.override_scatter(static_cast<int>(arena.roll(4)) + 2);
            run({{14, 1}, {16, 1}, {19, 1}});
        }
        arena.override_scatter(-1);
        run({{20, 4}, {21, 4}, {12, 1}, {13, 1}});
        break;
    case 15:
        run({{12, 2}, {13, 4}, {17, 3}, {18, 7}, {11, 1}});
        break;
    case 16:
        run({{13, 1}, {17, 1}, {18, 8}});
        break;
    case 17:
        run({{11, 2}, {12, 4}, {13, 2}, {18, 4}, {15, 6}});
        break;
    default:
        // The original says it cannot tell the ground and lays the open
        // ground alone.
        break;
    }
    if (ground < 10)
    {
        arena.fill(108, 2);
    }
    else if (ground == 15 || ground == 16)
    {
        arena.fill(91, 0);
    }
    else
    {
        arena.fill(89, 2);
    }
    return arena.cells();
}
} // namespace

BuckRogersTileSet buck_rogers_tile_set(unsigned area_kind) noexcept
{
    switch (area_kind)
    {
    case mars_wilderness:
        return {"MARSCOM.DAX", 1, 52};
    case venus_wilderness:
        return {"VENUSCOM.DAX", 1, 52};
    case 0x88:
    case 0x82:
        return {"LOTEK.DAX", 0, 53};
    default:
        return {"HITEK.DAX", 0, 53};
    }
}

CombatTerrain combat_terrain(unsigned kind)
{
    if (kind < 1 || kind > combat_terrain_kinds)
    {
        throw std::out_of_range("No such kind of combat terrain");
    }
    return terrains[kind - 1];
}

rules::Terrain combat_ground(unsigned kind)
{
    const auto terrain = combat_terrain(kind);
    if (terrain.line_class == 1)
    {
        return rules::Terrain::obstacle;
    }
    if (terrain.cost == 255)
    {
        return terrain.line_class == 2 ? rules::Terrain::impassable_cover
                                       : rules::Terrain::impassable;
    }
    return terrain.line_class == 2 ? rules::Terrain::cover : rules::Terrain::open;
}

BuckRogersBattlefield buck_rogers_battlefield(const GeoMap &map, unsigned x, unsigned y,
                                              unsigned area_kind, unsigned script,
                                              unsigned wilderness_terrain, std::uint32_t seed,
                                              unsigned tiles_in_file)
{
    if (x >= 16 || y >= 16)
    {
        throw std::out_of_range("A Buck Rogers arena needs the team's square");
    }
    const bool wilderness = area_kind == mars_wilderness || area_kind == venus_wilderness;
    const auto cells = wilderness ? wilderness_arena(area_kind, wilderness_terrain, seed)
                                  : dungeon_arena(map, x, y, area_kind, script);
    const auto set = buck_rogers_tile_set(area_kind);
    BuckRogersBattlefield result;
    auto &field = result.field;
    field.geometry = {columns, rows, std::vector<rules::Terrain>(columns * rows), {}};
    field.geometry.move_cost.assign(columns * rows, 1);
    field.tiles.assign(columns * rows, 0);
    result.terrain.assign(columns * rows, 0);
    // A terrain whose tile the set's file lacks (LOTEK holds 52 of the 53 the
    // game copies): the original draws from past the picture's pixels;
    // here it shows the file's last tile, which is of the same walls
    // (docs/games/countdown.md, Bugs).
    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const auto index = static_cast<std::size_t>(row * columns + column);
            // The original draws a cell past the last kind as kind 21 (CbtMap).
            unsigned kind = cells[row][column];
            if (kind < 1 || kind > combat_terrain_kinds)
            {
                kind = 21;
            }
            const auto terrain = combat_terrain(kind);
            result.terrain[index] = static_cast<std::uint8_t>(kind);
            field.geometry.terrain[index] = combat_ground(kind);
            field.geometry.move_cost[index] = terrain.cost;
            unsigned tile = terrain.tile;
            if (tile < set.first_slot || tile - set.first_slot >= tiles_in_file)
            {
                ++result.missing_tiles;
                tile = set.first_slot + tiles_in_file - 1;
            }
            field.tiles[index] = static_cast<std::uint8_t>(tile - set.first_slot);
        }
    }
    for (int row = -2; row <= 2; ++row)
    {
        for (int column = -6; column <= 6; ++column)
        {
            for (unsigned direction = 0; direction < 4; ++direction)
            {
                field.walled[row + 2][column + 6][direction] =
                    !wilderness && wall_between(map, static_cast<int>(x) + column,
                                                static_cast<int>(y) + row, direction * 2, y) == 1;
            }
        }
    }
    return result;
}
} // namespace opengold::por
