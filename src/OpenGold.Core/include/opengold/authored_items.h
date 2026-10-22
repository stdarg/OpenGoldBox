#ifndef OPENGOLD_AUTHORED_ITEMS_H
#define OPENGOLD_AUTHORED_ITEMS_H
#include <cstdint>

namespace opengold
{
// Original Pool of Radiance item types the game converts specially.
namespace original_item
{
inline constexpr std::uint8_t flask_of_oil = 86;
}

// SRD 5.2.1 adventuring gear OpenGoldBox adds to the original game, numbered
// above the original's 128 item types. They give every party Fire and Acid,
// which stop a troll's Regeneration, and Magic Ink, which writes a scroll or
// copies a scroll's spell into a Wizard's spellbook (MAGIC-6). Magic Ink of
// level 1 is magic_ink; levels 2 to 9 follow it. A Poisoner's Kit lets a Rogue
// poison with Cunning Strike (#233).
namespace authored_item
{
inline constexpr std::uint8_t torch = 200, acid = 201, alchemists_fire = 202, magic_ink = 203,
                              poisoners_kit = 212;

// The level of Magic Ink an item type is, if it is Magic Ink.
[[nodiscard]] inline constexpr unsigned magic_ink_level(std::uint8_t type) noexcept
{
    return type >= magic_ink && type < magic_ink + 9 ? type - magic_ink + 1u : 0u;
}
} // namespace authored_item
} // namespace opengold
#endif
