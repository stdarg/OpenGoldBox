#ifndef OPENGOLD_UA_VIEW_H
#define OPENGOLD_UA_VIEW_H

#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/ua_design.h"

#include <array>
#include <map>
#include <optional>
#include <vector>

// Unlimited Adventures' first-person view (docs/games/ua.md, The 3D view): an
// 88 x 88 window, three squares deep, the Gold Box games' geometry drawn from
// the area's three wall sets over the backdrop of the party's square.
namespace opengold::ua
{

// A set's 48 glyphs (0 its colors, 1 and 11 the far filler, 2 to 10 and 12 to
// 20 two walls by slot, 21 to 47 three overlays) and the colors they use.
struct WallSet
{
    std::vector<std::optional<Glyph>> glyphs;
    std::array<VgaColor, 256> palette{};
};

// A backdrop of BACK.TLB: an 88 x 88 picture and its colors.
struct Backdrop
{
    Glyph picture;
    std::array<VgaColor, 256> palette{};
};

struct ViewArt
{
    // By the area's wall set slots; empty for a slot with no set.
    std::array<std::optional<WallSet>, 3> sets;
    // By backdrop number.
    std::map<unsigned, Backdrop> backdrops;
};

// The wall sets and backdrops the area names, from the design's files. Throws
// when a set or backdrop it names is missing or does not decode.
[[nodiscard]] ViewArt load_view_art(const DesignFiles &files, const Area &area);

// What a wall byte says: its low four bits the graphic (0 none; 1 to 15 the
// set slot (v - 1) / 5 and its variant (v - 1) % 5 + 1), its high four bits
// what it does to a step.
enum class WallKind : std::uint8_t
{
    open = 0,
    open_secret = 1,
    locked = 2,
    locked_secret = 3,
    locked_wizard = 4,
    locked_wizard_secret = 5,
    first_key = 6, // 6 to 13: opens with key 1 to 8.
    last_key = 13,
    blocked = 14,
    false_door = 15,
};

[[nodiscard]] constexpr WallKind wall_kind(std::uint8_t wall) noexcept
{
    return WallKind{static_cast<std::uint8_t>(wall >> 4)};
}

// The view from square (x, y) facing `facing`; `night` picks a backdrop's
// night picture where it has one (numbers from 20h).
[[nodiscard]] Image render_view(const Area &area, const ViewArt &art, unsigned x, unsigned y,
                                por::MapDirection facing, bool night);

} // namespace opengold::ua

#endif
