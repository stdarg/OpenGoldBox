#ifndef OPENGOLD_OVERLAND_H
#define OPENGOLD_OVERLAND_H
#include "opengold/dungeon_battlefield.h"
#include "opengold/formats.h"
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

    [[nodiscard]] std::uint8_t at(unsigned x, unsigned y) const
    {
        return squares.at(y * columns + x);
    }
};

// Reads the wilderness from START.EXE's unpacked image; nothing when the image
// lacks it.
[[nodiscard]] std::optional<OverlandWorld> read_overland_world(std::span<const std::uint8_t> image);

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

[[nodiscard]] Image render_overland_window(const OverlandWorld &world,
                                           std::span<const Image> tiles, const Image &party_icon,
                                           const OverlandView &view);

// The original's battlefield for a fight on the overland square of tile
// `tile`: open ground, with trees, ground cover, a road or ridge with its
// boulders and rare debris as the terrain decides. `roll(sides)` rolls one die.
// Its tiles are art frames of the wilderness set: WILDCOM's 34 frames, then
// RANDCOM's from 34.
[[nodiscard]] DungeonBattlefield wilderness_battlefield(const OverlandWorld &world,
                                                        std::uint8_t tile,
                                                        const std::function<int(int)> &roll);
} // namespace opengold::por
#endif
