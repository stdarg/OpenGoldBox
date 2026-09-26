#include "opengold/dungeon_battlefield.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace opengold::por;
namespace
{
void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
std::uint64_t digest(const DungeonBattlefield &field)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto tile : field.tiles)
    {
        hash ^= tile;
        hash *= 1099511628211ULL;
    }
    return hash;
}
void oracle_tests()
{
    // Full-arena results from the original PC executable with synthetic maps,
    // stopping before passable floor decoration. These do not derive from the
    // native generator. No original game map or artwork is embedded here.
    constexpr std::array<std::uint64_t, 27> expected{
        0x76095efe1401490dULL, 0x93a018362def56aaULL, 0x81ce03fe18f27f81ULL, 0xe1a33502a82cf3a4ULL,
        0xf13e172d8fe979abULL, 0x26e52a0c352761f0ULL, 0x2d48b518a4e11321ULL, 0x6611d9764862564aULL,
        0x32496495cf4fc771ULL, 0xbe77260999daae26ULL, 0x768021bd6a47dd1dULL, 0xe0bf093c09d634beULL,
        0x2387cf1131958fbfULL, 0xc19d544945934348ULL, 0x2ba9a3ef6d4eabb3ULL, 0xc728db1bfd805668ULL,
        0x36c67dfe1a8b45fbULL, 0x59bffda09e908a60ULL, 0x314f65398c7d3e61ULL, 0x8a8bb0d6ca018f7aULL,
        0xb0e9854960c86a2dULL, 0x23c29d50ad9797b4ULL, 0x47aafe814babe923ULL, 0x40f8636c767500f4ULL,
        0xdfb4c31930267055ULL, 0xe471b601cea26066ULL, 0x8352ae6e9396c1f9ULL};
    unsigned index = 0;
    for (unsigned west : {0, 1, 3})
        for (unsigned north : {0, 1, 3})
            for (unsigned east : {0, 1, 3})
            {
                GeoMap map;
                auto &c = map.cells[8 * 16 + 8];
                for (auto [d, type] : std::array<std::pair<unsigned, unsigned>, 3>{
                         {{3, west}, {0, north}, {1, east}}})
                {
                    c.walls[d] = type != 0;
                    c.doors[d] = type == 3;
                }
                const auto field = dungeon_battlefield(map, 8, 8);
                if (digest(field) != expected[index])
                    throw std::runtime_error("Original wall/door oracle mismatch at combination " +
                                             std::to_string(index));
                check(field.geometry.width == 50 && field.geometry.height == 25 &&
                          field.geometry.terrain.size() == 1250,
                      "Original arena dimensions");
                ++index;
            }
    GeoMap mixed;
    for (unsigned i = 0; i < 256; ++i)
        for (unsigned d = 0; d < 4; ++d)
        {
            mixed.cells[i].walls[d] = (i * 7 + d * 3) % 5 == 0;
            mixed.cells[i].doors[d] = (i / 3 + d) % 4;
        }
    constexpr std::array<std::pair<unsigned, unsigned>, 8> poses{
        {{0, 0}, {15, 0}, {0, 15}, {15, 15}, {8, 8}, {1, 8}, {14, 8}, {8, 1}}};
    constexpr std::array<std::uint64_t, 8> borders{
        0x4358d2f3eecbc509ULL, 0x771dc10d93b9c6adULL, 0x99448cdd35baef37ULL, 0x85328af967096776ULL,
        0xcff2298921ad58d5ULL, 0xef17af3460689192ULL, 0xe14a0977291617f5ULL, 0x1e2cfb5a50cebbc5ULL};
    for (unsigned i = 0; i < poses.size(); ++i)
        check(digest(dungeon_battlefield(mixed, poses[i].first, poses[i].second)) == borders[i],
              "Original mixed-boundary oracle mismatch");
}
void collision_tests()
{
    GeoMap map;
    auto &c = map.cells[8 * 16 + 8];
    c.walls[0] = 1;
    auto solid = dungeon_battlefield(map, 8, 8);
    check(solid.geometry.at({24, 10}) == 1 && solid.geometry.at({24, 11}) == 0,
          "Wall face blocks but wall base remains passable");
    for (unsigned door = 1; door <= 3; ++door)
    {
        c.doors[0] = door;
        auto field = dungeon_battlefield(map, 8, 8);
        check(field.geometry.at({24, 10}) == 0 && field.geometry.at({25, 10}) == 0,
              "All original door codes preserve two-tile opening");
        check(field.geometry.at({23, 10}) == 1 && field.geometry.at({26, 10}) == 1,
              "Door posts remain blocking");
    }
    bool rejected = false;
    try
    {
        (void)dungeon_battlefield(map, 16, 0);
    }
    catch (const std::out_of_range &)
    {
        rejected = true;
    }
    check(rejected, "Reject out-of-map party position");
}
} // namespace
int main()
{
    try
    {
        oracle_tests();
        collision_tests();
        std::cout << "Dungeon geometry: 35 original-routine oracles and collision checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
