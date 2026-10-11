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
// which stop a troll's Regeneration.
namespace authored_item
{
inline constexpr std::uint8_t torch = 200, acid = 201, alchemists_fire = 202;
}
} // namespace opengold
#endif
