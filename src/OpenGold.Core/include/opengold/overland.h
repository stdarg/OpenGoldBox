#ifndef OPENGOLD_OVERLAND_H
#define OPENGOLD_OVERLAND_H
#include "opengold/dungeon_battlefield.h"
#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/wall_art.h"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace opengold::por
{
// The wilderness around Phlan: a 44 x 36 grid of SQRPACI tile numbers that
// START.EXE keeps in its data, seen through three 16-column windows (west,
// central, east) that overlap by three columns. Landmarks drawn over the grid
// change with the story (a site found, the Pyramid's valley healed).
struct OverlandWorld
{
    static constexpr unsigned columns = 44, rows = 36, window_columns = 16;

    struct Landmark
    {
        unsigned x{}, y{}, tile{};
    };

    std::vector<std::uint8_t> squares;
    std::array<Landmark, 9> landmarks{};
    // The world column of each window's first column, by travel mode: 2 west,
    // 3 central, 4 east.
    std::array<unsigned, 3> window_start{};
    // The original's combat terrain table: per battlefield tile its movement
    // cost (255 impassable) and its art frame.
    struct CombatTile
    {
        std::uint8_t cost{}, frame{};
    };
    std::vector<CombatTile> combat_tiles;

    // The valley's healing once the Pyramid's pollution is stopped (the
    // original's Yarash ending): in each of the tiles listed (128 and up in
    // the second set), every EGA color `from` holds becomes `to`'s.
    struct Recolor
    {
        std::vector<std::uint8_t> tiles;
        std::array<std::uint8_t, 16> from{}, to{};
    };

    std::vector<Recolor> healing;

    [[nodiscard]] std::uint8_t at(unsigned x, unsigned y) const
    {
        return squares.at(y * columns + x);
    }
};

// Reads the wilderness from START.EXE's unpacked image; nothing when the image
// lacks it.
[[nodiscard]] std::optional<OverlandWorld> read_overland_world(std::span<const std::uint8_t> image);

// The wilderness tiles as the healed valley shows them, by the world's
// healing; tiles it does not name stay as they are.
[[nodiscard]] std::vector<Image> healed_overland_tiles(const OverlandWorld &world,
                                                       std::vector<Image> tiles);

// What a square of the grid allows a party stepping onto it.
enum class OverlandPassage
{
    open,
    // A river of the poisoned valley: closed until the Pyramid's sorcerer is
    // beaten.
    river,
    closed
};

[[nodiscard]] OverlandPassage overland_passage(std::uint8_t tile) noexcept;

// The squares eight directions step by: north, northeast, east, and on
// clockwise.
inline constexpr std::array<int, 8> overland_dx{0, 1, 1, 1, 0, -1, -1, -1},
    overland_dy{-1, -1, 0, 1, 1, 1, 0, -1};

// The 5 x 5 squares around the party, 24 pixels each, from SQRPACI's tiles
// (tile n is art n - 1), with the party's icon on its square. `landmark` says
// which of the nine landmarks shows its own tile now.
struct OverlandView
{
    unsigned world_x{}, world_y{};
    unsigned direction{};
    std::function<bool(unsigned landmark)> landmark_shown;
};

// Gateway to the Savage Frontier's overland as its DrawOverlandView draws it
// in the view's 88 by 88 pixels: the 11 by 11 squares around the party (kept
// on the map), each square's north-and-east wall byte the number of a symbol
// of the three sets its LOAD PIECES loaded (`symbols`, 70 a set; from 210 the
// marks of `marks`), and the party's arrow over it, the marks' first four by
// facing, their light magenta clear.
[[nodiscard]] Image render_symbol_map(const GeoMap &map, std::span<const WallTile> symbols,
                                      std::span<const WallTile> marks, unsigned x, unsigned y,
                                      MapDirection facing);

[[nodiscard]] Image render_overland_window(const OverlandWorld &world,
                                           std::span<const Image> tiles, const Image &party_icon,
                                           const OverlandView &view);

// The original's battlefield for a fight on the overland square of tile
// `tile`: open ground, with trees, ground cover, a road or ridge with its
// boulders and rare debris as the terrain decides. `roll(count, sides)` rolls
// `count` dice of `sides` and gives their sum, as the original's dice routine
// does in one call. Its tiles are art frames of the wilderness set: WILDCOM's
// 34 frames, then RANDCOM's from 34.
[[nodiscard]] DungeonBattlefield
wilderness_battlefield(const OverlandWorld &world, std::uint8_t tile,
                       const std::function<int(int count, int sides)> &roll);
// The same field from a table of battlefield tiles and the terrain's flags
// (road or ridge, trees, ground cover), as Curse builds it for its places.
[[nodiscard]] DungeonBattlefield
wilderness_battlefield(std::span<const OverlandWorld::CombatTile> tiles, unsigned flags,
                       const std::function<int(int count, int sides)> &roll);

// Curse of the Azure Bonds's wilderness fights: its battlefield tiles and the
// terrain flags of each place (the area's current place, script cell 0x4E42),
// read from its unpacked START.EXE.
struct CurseWilderness
{
    std::vector<OverlandWorld::CombatTile> combat_tiles;
    std::vector<std::uint8_t> place_flags;
};

[[nodiscard]] std::optional<CurseWilderness> curse_wilderness(std::span<const std::uint8_t> image);

// Gateway to the Savage Frontier's fights out of doors: its terrains (each
// one's movement cost and WILDCOM tile, numbered from 1; 0 is none) and the
// class bits of each kind of ground, read from its unpacked GAME.EXE.
struct GatewayWilderness
{
    std::vector<OverlandWorld::CombatTile> combat_tiles;
    std::vector<std::uint8_t> terrain_classes;
};

[[nodiscard]] std::optional<GatewayWilderness>
gateway_wilderness(std::span<const std::uint8_t> image);

// Gateway's field out of doors (its BuildWildernessArena): grass, a river by
// chance on rough ground, trees and scattered features, as the ground's class
// bits decide; `tiles` is GatewayWilderness's terrains.
[[nodiscard]] DungeonBattlefield
gateway_wilderness_battlefield(std::span<const OverlandWorld::CombatTile> tiles,
                               unsigned terrain_class,
                               const std::function<int(int count, int sides)> &roll);
} // namespace opengold::por
#endif
