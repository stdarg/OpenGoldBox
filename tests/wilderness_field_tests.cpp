// The wilderness battlefield grown as the original's START.EXE grows it, with
// set dice on a made-up terrain table, so no game files are needed. The DOS
// probe's wilderness_field experiment compares whole fields with the running
// original; these pin the details it found: trees and ground cover walk the
// field column by column, a boulder on the road brings one in the next row,
// the pair beside the road comes only when the road has none, and a road that
// reaches column 49 puts its ridge on the next row's first square.
#include "opengold/overland.h"
#include <cstdlib>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace opengold::por;

namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// The heights for sight a tile of this frame has in the made-up table.
std::uint8_t clearance_of(int frame)
{
    return static_cast<std::uint8_t>(frame % 3);
}

std::uint8_t blocking_of(int frame)
{
    return static_cast<std::uint8_t>(frame % 4);
}

// Tiles 0x17 and 0x36 are floor (frame 0x16), as in the original; every other
// tile is its own frame and open.
OverlandWorld world()
{
    OverlandWorld result;
    for (unsigned tile = 0; tile < 66; ++tile)
    {
        const bool floor = tile == 0x17 || tile == 0x36;
        const auto frame = static_cast<std::uint8_t>(floor ? 0x16 : tile);
        result.combat_tiles.push_back({1, frame, clearance_of(frame), blocking_of(frame)});
    }
    return result;
}

// The set dice first, then a number that places nothing: 255 on a d255 for
// ground cover, 50 on a d100 for trees and debris, the highest on any other.
class Dice
{
  public:
    explicit Dice(std::deque<int> set) : set_(std::move(set))
    {
    }

    int operator()(int count, int sides)
    {
        if (!set_.empty())
        {
            const int value = set_.front();
            set_.pop_front();
            return value;
        }
        return sides == 100 ? 50 : count * sides;
    }

  private:
    std::deque<int> set_;
};

DungeonBattlefield grow(std::uint8_t tile, std::deque<int> set)
{
    Dice dice(std::move(set));
    return wilderness_battlefield(world(), tile,
                                  [&](int count, int sides)
                                  {
                                      return dice(count, sides);
                                  });
}

int frame(const DungeonBattlefield &field, int row, int column)
{
    return field.tiles.at(static_cast<std::size_t>(row * 50 + column));
}

// Light woods (tile 0x25, a d100 of 3 or less makes a tree): the first tree
// stands on row 1 of column 0; the square below its trunk takes no roll, so
// the next roll falls on row 3 of the same column, not on column 1.
void trees_column_by_column()
{
    const auto field = grow(0x25, {100, 1, 1, 1, 1});
    check(frame(field, 1, 0) == 0x24 && frame(field, 0, 0) == 0x20,
          "The first tree's trunk and crown");
    check(frame(field, 3, 0) == 0x24, "The second tree stands further down the first column");
    check(frame(field, 1, 1) == 0x16, "Not beside the first, as a row-by-row walk put it");
}

// A road (tile 0xC0, 75 in 100): 5d4 of 12 starts it at column 19. A d20 of 1
// on row 0 places a boulder, and the odd count one on row 1 too; row 2's
// d20 of 20 places none, and no pair stands beside the road.
void boulders_in_pairs()
{
    std::deque<int> set{1, 12, 1};
    for (int row = 1; row < 25; ++row)
    {
        set.push_back(20);
    }
    set.push_back(5); // a d9 the road must not roll
    const auto field = grow(0xC0, set);
    check(frame(field, 0, 19) == 0x34 && frame(field, 0, 20) == 0x35, "Row 0's boulder");
    check(frame(field, 1, 20) == 0x34 && frame(field, 1, 21) == 0x35, "Row 1's boulder follows");
    check(frame(field, 2, 21) == 0x32 && frame(field, 2, 22) == 0x33, "Row 2 has road and ridge");
    check(frame(field, 11, 30) != 0x34, "No pair beside a road with boulders");
}

// With no boulder on the road, a pair stands 16 - d9 rows down, beside it.
void pair_beside_a_bare_road()
{
    std::deque<int> set{1, 12};
    for (int row = 0; row < 25; ++row)
    {
        set.push_back(20);
    }
    set.push_back(5);
    const auto field = grow(0xC0, set);
    check(frame(field, 11, 30) == 0x34 && frame(field, 12, 31) == 0x34,
          "A bare road's pair of boulders on rows 11 and 12");
}

// 5d4 of 5 starts the road at column 26; on row 23 it reaches column 49, and
// its ridge falls on row 24's first square, as in the original. It has 24
// rows, then the pair beside it.
void road_to_the_last_column()
{
    std::deque<int> set{1, 5};
    for (int row = 0; row < 24; ++row)
    {
        set.push_back(20);
    }
    set.push_back(5);
    const auto field = grow(0xC0, set);
    check(frame(field, 23, 49) == 0x32, "The road reaches column 49");
    check(frame(field, 24, 0) == 0x33, "Its ridge falls on the next row's first square");
}
} // namespace

// Each square keeps its tile's heights for sight (rules::Battlefield).
void squares_keep_their_heights()
{
    const auto field = grow(0x25, {3});
    bool kept = field.geometry.clearance.size() == field.tiles.size() &&
                field.geometry.blocking.size() == field.tiles.size();
    for (std::size_t n = 0; kept && n < field.tiles.size(); ++n)
    {
        kept = field.geometry.clearance[n] == clearance_of(field.tiles[n]) &&
               field.geometry.blocking[n] == blocking_of(field.tiles[n]);
    }
    check(kept, "Every square has its tile's clearance and blocking");
}

// Each square costs what its tile's entry in the table costs, as the games
// after Pool of Radiance charge it (Curse's and Gateway's cbtacts.pas
// CombatMoveCreature): here the floor costs 3 and a tree's trunk 1.
void squares_cost_their_tiles()
{
    auto costly = world();
    for (auto &tile : costly.combat_tiles)
    {
        tile.cost = static_cast<std::uint8_t>(1 + tile.frame % 4);
    }
    Dice dice({100, 1, 1, 1, 1});
    const auto field = wilderness_battlefield(costly, 0x25,
                                              [&](int count, int sides)
                                              {
                                                  return dice(count, sides);
                                              });
    const auto &costs = field.geometry.move_cost;
    check(costs.size() == field.tiles.size(), "Every square has its cost");
    check(costs[1 * 50] == 1 && costs[1 * 50 + 1] == 3,
          "A tree and the floor cost what their tiles cost");
    for (std::size_t n = 0; n < costs.size(); ++n)
    {
        check(costs[n] == 1 + field.tiles[n] % 4, "Each square costs what its tile costs");
    }
}

int main()
{
    try
    {
        trees_column_by_column();
        boulders_in_pairs();
        pair_beside_a_bare_road();
        road_to_the_last_column();
        squares_keep_their_heights();
        squares_cost_their_tiles();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Wilderness field tests passed\n";
    return EXIT_SUCCESS;
}
