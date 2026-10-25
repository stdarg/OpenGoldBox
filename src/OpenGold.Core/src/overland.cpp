#include "opengold/overland.h"
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace opengold::por
{
namespace
{
// START.EXE's data segment holds the wilderness at fixed offsets; its start
// is found from the landmark table, whose first entry is the Silver Dragon's
// pass at (10,9) and the Outpost of Zhentil Keep at (3,32).
constexpr std::size_t landmark_offset = 0x0C36, squares_offset = 0x3648,
                      window_offset = 0x3C78, combat_tile_offset = 0x2744;
constexpr std::size_t combat_tile_count = 0x42;

std::uint16_t word_at(std::span<const std::uint8_t> image, std::size_t at)
{
    return static_cast<std::uint16_t>(image[at] | image[at + 1] << 8);
}

bool in(std::uint8_t tile, std::initializer_list<std::pair<int, int>> ranges)
{
    return std::any_of(ranges.begin(), ranges.end(),
                       [&](const auto &range)
                       {
                           return tile >= range.first && tile <= range.second;
                       });
}

// The original's classification of the overland tile under a fight, as bits
// its battlefield generator reads: 2, 4, 40h and 80h thin or thicken the ground
// cover, 4, 8, 40h and 80h the trees, 10h and 20h bring a road or ridge.
unsigned terrain_flags(std::uint8_t t)
{
    if (in(t, {{0x00, 0x02}, {0x28, 0x29}, {0x4F, 0x56}, {0x5B, 0x5C}, {0x2B, 0x2B}, {0xF1, 0xF1}}))
    {
        return 0x01;
    }
    if (in(t, {{0xA8, 0xB1}}))
    {
        return 0x21;
    }
    if (in(t, {{0xB9, 0xB9}, {0xBC, 0xC0}}))
    {
        return 0x11;
    }
    if (in(t, {{0x03, 0x04}, {0x06, 0x09}, {0x13, 0x15}, {0x17, 0x19}}))
    {
        return 0x03;
    }
    if (in(t, {{0x07, 0x07}, {0x0A, 0x12}, {0x16, 0x16}, {0x1A, 0x24}, {0xC9, 0xD0}}))
    {
        return 0x02;
    }
    if (in(t, {{0xD1, 0xD5}}))
    {
        return 0x12;
    }
    if (in(t, {{0x25, 0x25},
               {0x39, 0x39},
               {0x40, 0x40},
               {0x9A, 0x9E},
               {0xA4, 0xA5},
               {0xB4, 0xB4},
               {0xF3, 0xF3},
               {0xEF, 0xF0},
               {0xFA, 0xFA},
               {0xFC, 0xFC}}))
    {
        return 0x04;
    }
    if (in(t, {{0x27, 0x2C}, {0x99, 0x99}, {0x9F, 0x9F}}))
    {
        return 0x05;
    }
    if (in(t, {{0x2D, 0x2E}, {0x33, 0x38}, {0x43, 0x4D}, {0x5D, 0x61}}))
    {
        return 0x44;
    }
    if (in(t, {{0xB5, 0xB8}, {0xBA, 0xBA}, {0xC1, 0xC8}}))
    {
        return 0x14;
    }
    if (in(t, {{0x2F, 0x32}, {0x62, 0x65}, {0xF2, 0xF2}}))
    {
        return 0x40;
    }
    if (t == 0x26)
    {
        return 0x41;
    }
    if (in(t, {{0xB2, 0xB3}}))
    {
        return 0x60;
    }
    if (in(t, {{0x3A, 0x3C}, {0x41, 0x42}}))
    {
        return 0x48;
    }
    if (in(t, {{0x3D, 0x3F},
               {0x79, 0x7C},
               {0x87, 0x89},
               {0x8B, 0x8B},
               {0x8D, 0x8D},
               {0x90, 0x93},
               {0x95, 0x95},
               {0x97, 0x98},
               {0xA0, 0xA2},
               {0xA6, 0xA7},
               {0xD6, 0xD8},
               {0xEE, 0xEE}}))
    {
        return 0x08;
    }
    if (in(t, {{0x57, 0x5A}, {0xA3, 0xA3}, {0xE0, 0xE0}}))
    {
        return 0x88;
    }
    if (in(t, {{0x7C, 0x7D}, {0x8A, 0x8A}, {0x8C, 0x8C}, {0x8E, 0x8F}, {0x94, 0x94}, {0x96, 0x96}}))
    {
        return 0x09;
    }
    if (in(t, {{0x7E, 0x85}, {0xE5, 0xE7}}))
    {
        return 0x28;
    }
    if (in(t, {{0xD9, 0xDF}}))
    {
        return 0x18;
    }
    if (in(t, {{0x4E, 0x4E}, {0xF5, 0xF5}, {0xE8, 0xE9}}))
    {
        return 0x20;
    }
    if (in(t, {{0x6D, 0x6D}, {0xF4, 0xF6}}))
    {
        return 0x80;
    }
    if (in(t, {{0x6E, 0x71}, {0x73, 0x78}}))
    {
        return 0x81;
    }
    if (in(t, {{0x66, 0x6C}, {0x72, 0x72}}))
    {
        return 0x90;
    }
    if (in(t, {{0xE1, 0xE4}, {0xEA, 0xED}}))
    {
        return 0xA0;
    }
    return 0;
}

constexpr int field_columns = 50, field_rows = 25;
constexpr std::uint8_t ground = 0x17, road = 0x32, ridge = 0x33, boulder = 0x34;
constexpr std::uint8_t floor_frame = 0x16;

class FieldBuilder
{
  public:
    FieldBuilder(const OverlandWorld &world, const std::function<int(int)> &roll)
        : world_(world), roll_(roll), grid_(field_columns * field_rows, ground)
    {
    }

    std::uint8_t &at(int row, int column)
    {
        return grid_.at(static_cast<std::size_t>(row * field_columns + column));
    }

    [[nodiscard]] bool floor(int row, int column) const
    {
        const auto tile = grid_.at(static_cast<std::size_t>(row * field_columns + column));
        return tile < world_.combat_tiles.size() && world_.combat_tiles[tile].frame == floor_frame;
    }

    int roll(int count, int sides) const
    {
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll_(sides);
        }
        return total;
    }

    // A road across the field, a ridge on rougher ground, with boulders.
    void road_or_ridge(unsigned flags)
    {
        const int chance = flags & 0x10 ? 75 : flags & 0x20 ? 35 : 0;
        if (roll(1, 100) > chance)
        {
            return;
        }
        int column = 34 - roll(5, 4);
        while ((column + 2) % 7 > 0)
        {
            --column;
        }
        const int start = column;
        for (int row = 0; row < field_rows && column < field_columns - 1; ++row, ++column)
        {
            at(row, column) = road;
            at(row, column + 1) = ridge;
            if (roll(1, 20) == 1)
            {
                boulders(row, column);
            }
        }
        const int row = 16 - roll(1, 9);
        if (start + row + 2 < field_columns)
        {
            boulders(row, start + row);
            boulders(row + 1, start + row + 1);
        }
    }

    void boulders(int row, int column)
    {
        at(row, column) = boulder;
        at(row, column + 1) = boulder + 1;
    }

    // Trees, thicker in forest, none on bare ground; a small tree's crown fills
    // the square above it.
    void trees(unsigned flags)
    {
        int density = 1;
        density = flags & 0x04 ? 3 : density;
        density = flags & 0x40 ? 4 : density;
        density = flags & 0x08 ? 7 : density;
        density = flags & 0x80 ? 0 : density;
        for (int row = 1; row < field_rows; ++row)
        {
            for (int column = 0; column < field_columns; ++column)
            {
                if (!floor(row, column) || !floor(row - 1, column) || roll(1, 100) > density)
                {
                    continue;
                }
                const int sized = roll(1, 10);
                int size = sized < 9 ? (sized + 1) / 2 : roll(1, 2) + 4;
                if (flags & 0x40)
                {
                    size = roll(1, 3) + 3;
                }
                at(row, column) = static_cast<std::uint8_t>(size + 0x23);
                if (size < 5)
                {
                    at(row - 1, column) = static_cast<std::uint8_t>(size + 0x1F);
                }
            }
        }
    }

    // Bushes, rocks and grass, by thresholds the terrain shifts.
    void ground_cover(unsigned flags)
    {
        int a = 0, b = 0x0F, c = 0x28, e = 0;
        if (flags & 0x02)
        {
            b = 0x2D;
            c = 0x0A;
        }
        if (flags & 0x04)
        {
            b = 0x19;
        }
        if (flags & 0x40)
        {
            c = 5;
            e = 0x1E;
        }
        if (flags & 0x80)
        {
            a = 0x0F;
            c = 0x19;
            b = 0x0A;
        }
        for (int row = 0; row < field_rows; ++row)
        {
            for (int column = 0; column < field_columns; ++column)
            {
                if (!floor(row, column))
                {
                    continue;
                }
                const int r = roll(1, 255);
                if (r <= a)
                {
                    const int small = roll(1, 4);
                    if (small == 4 && row > 0 && floor(row - 1, column))
                    {
                        at(row - 1, column) = 0x41;
                        at(row, column) = 0x40;
                    }
                    else if (small < 4)
                    {
                        at(row, column) = static_cast<std::uint8_t>(0x3C + small);
                    }
                }
                else if (r <= a + 6)
                {
                    at(row, column) = static_cast<std::uint8_t>(0x36 + roll(1, 3));
                }
                else if (r <= a + b + 6)
                {
                    at(row, column) = static_cast<std::uint8_t>(0x29 + roll(1, 4));
                }
                else if (r <= a + b + e + 6)
                {
                    at(row, column) = static_cast<std::uint8_t>(0x39 + (roll(1, 10) - 1) / 3);
                }
                else if (r <= a + b + e + c + 6)
                {
                    int kind = roll(1, 4);
                    if (kind == 4 && !(flags & 0x40))
                    {
                        kind = roll(1, 3);
                    }
                    at(row, column) = static_cast<std::uint8_t>(0x2D + kind);
                }
            }
        }
    }

    // Rare floor variants.
    void debris()
    {
        for (int row = 0; row < field_rows; ++row)
        {
            for (int column = 0; column < field_columns; ++column)
            {
                if (!floor(row, column))
                {
                    continue;
                }
                const int r = roll(1, 100);
                if (r == 98)
                {
                    at(row, column) = 0x1A;
                }
                else if (r == 99)
                {
                    at(row, column) = 0x1B;
                }
                else if (r == 100 && roll(1, 100) == 1)
                {
                    at(row, column) = roll(1, 10) <= 6 ? 0x1C : 0x1D;
                }
            }
        }
    }

    DungeonBattlefield field() const
    {
        DungeonBattlefield result{{field_columns, field_rows,
                                   std::vector<rules::Terrain>(grid_.size()),
                                   std::vector<bool>(grid_.size(), true)},
                                  std::vector<std::uint8_t>(grid_.size())};
        for (std::size_t n = 0; n < grid_.size(); ++n)
        {
            const auto &tile = world_.combat_tiles.at(grid_[n]);
            result.tiles[n] = tile.frame;
            result.geometry.terrain[n] =
                tile.cost == 255 ? rules::Terrain::obstacle : rules::Terrain::open;
        }
        return result;
    }

  private:
    const OverlandWorld &world_;
    const std::function<int(int)> &roll_;
    std::vector<std::uint8_t> grid_;
};
} // namespace

std::optional<OverlandWorld> read_overland_world(std::span<const std::uint8_t> image)
{
    // The landmark table's first two entries, as little-endian words.
    constexpr std::array<std::uint8_t, 12> signature{10, 0, 9, 0, 240, 0, 3, 0, 32, 0, 2, 0};
    const auto found = std::search(image.begin(), image.end(), signature.begin(), signature.end());
    const auto at = static_cast<std::size_t>(found - image.begin());
    if (found == image.end() || at < landmark_offset ||
        at - landmark_offset + squares_offset + OverlandWorld::columns * OverlandWorld::rows >
            image.size())
    {
        return std::nullopt;
    }
    const auto data = at - landmark_offset;
    OverlandWorld world;
    world.squares.assign(image.begin() + static_cast<std::ptrdiff_t>(data + squares_offset),
                         image.begin() + static_cast<std::ptrdiff_t>(
                                             data + squares_offset +
                                             OverlandWorld::columns * OverlandWorld::rows));
    for (std::size_t k = 0; k < world.landmarks.size(); ++k)
    {
        const auto entry = data + landmark_offset + k * 6;
        world.landmarks[k] = {word_at(image, entry), word_at(image, entry + 2),
                              word_at(image, entry + 4)};
    }
    for (std::size_t mode = 0; mode < world.window_start.size(); ++mode)
    {
        world.window_start[mode] = image[data + window_offset + mode];
    }
    for (std::size_t tile = 0; tile < combat_tile_count; ++tile)
    {
        const auto entry = data + combat_tile_offset + tile * 4;
        world.combat_tiles.push_back({image[entry], image[entry + 3]});
    }
    if (world.window_start != std::array<unsigned, 3>{0, 13, 26})
    {
        return std::nullopt;
    }
    return world;
}

OverlandPassage overland_passage(std::uint8_t t) noexcept
{
    if (in(t, {{0x4E, 0x4E},
               {0x7E, 0x80},
               {0x82, 0x84},
               {0xA8, 0xAC},
               {0xAE, 0xB3},
               {0xBC, 0xBC},
               {0xDD, 0xDE},
               {0xE1, 0xE9},
               {0xF5, 0xF5}}))
    {
        return OverlandPassage::closed;
    }
    return in(t, {{0x66, 0x6C}, {0x72, 0x72}}) ? OverlandPassage::river : OverlandPassage::open;
}

Image render_overland_window(const OverlandWorld &world, std::span<const Image> tiles,
                             const Image &party_icon, const OverlandView &view)
{
    constexpr unsigned span = 5, size = 24;
    const unsigned left = std::clamp<int>(static_cast<int>(view.world_x) - 2, 0,
                                          OverlandWorld::columns - span),
                   top = std::clamp<int>(static_cast<int>(view.world_y) - 2, 0,
                                         OverlandWorld::rows - span);
    Image result;
    result.width = result.height = span * size;
    result.rgba.assign(static_cast<std::size_t>(result.width) * result.height * 4, 0);
    const auto blit = [&](const Image &art, unsigned column, unsigned row, bool mirrored)
    {
        for (unsigned y = 0; y < std::min<unsigned>(art.height, size); ++y)
        {
            for (unsigned x = 0; x < std::min<unsigned>(art.width, size); ++x)
            {
                const auto source = (y * art.width + (mirrored ? art.width - 1 - x : x)) * 4;
                if (art.rgba[source + 3] == 0)
                {
                    continue;
                }
                const auto target = ((row * size + y) * result.width + column * size + x) * 4;
                std::copy_n(art.rgba.begin() + source, 4, result.rgba.begin() + target);
            }
        }
    };
    for (unsigned row = 0; row < span; ++row)
    {
        for (unsigned column = 0; column < span; ++column)
        {
            const unsigned x = left + column, y = top + row;
            unsigned tile = world.at(x, y);
            for (unsigned k = 0; k < world.landmarks.size(); ++k)
            {
                const auto &mark = world.landmarks[k];
                if (mark.x == x && mark.y == y && view.landmark_shown && view.landmark_shown(k + 1))
                {
                    tile = mark.tile;
                }
            }
            if (tile >= 1 && tile - 1 < tiles.size())
            {
                blit(tiles[tile - 1], column, row, false);
            }
        }
    }
    // The party faces left when heading west.
    blit(party_icon, view.world_x - left, view.world_y - top,
         view.direction >= 5 && view.direction <= 7);
    return result;
}

DungeonBattlefield wilderness_battlefield(const OverlandWorld &world, std::uint8_t tile,
                                          const std::function<int(int)> &roll)
{
    if (world.combat_tiles.size() <= 0x41)
    {
        throw std::invalid_argument("The wilderness has no combat terrain table");
    }
    const auto flags = terrain_flags(tile);
    FieldBuilder builder(world, roll);
    builder.road_or_ridge(flags);
    builder.trees(flags);
    builder.ground_cover(flags);
    builder.debris();
    return builder.field();
}
} // namespace opengold::por
