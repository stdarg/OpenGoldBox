#ifndef OPENGOLD_SCRIPT_SECRET_DOORS_H
#define OPENGOLD_SCRIPT_SECRET_DOORS_H
#include "opengold/geo_map.h"
#include <array>
#include <cstdint>

namespace opengold::por
{
// A secret door an original script keeps shut until the party finds it, which
// the Wand of Secrets can point out (SECRET-1). It is on the `side` of square
// (x, y) of `map` (the GEO record its area uses, as for Teleport) and is found
// once the script variable `flag` holds the `found` bits.
struct ScriptSecretDoor
{
    unsigned map{}, x{}, y{};
    MapDirection side{};
    std::uint16_t flag{}, found{};
};

// The Pyramid's doors, from the tables its scripts search with (ECL7:22's at
// 0x9C95 and 0x9CA0, ECL7:23's at 0x9C40 and 0x9C4A), and the door in Kuto's
// Well's catacombs (ECL8:29). The kobold caves' secret door is left out: once
// found, the script still keeps it shut.
inline constexpr std::array script_secret_doors{
    // The prison of souls; the doors at (15,0) and (15,15) lead out.
    ScriptSecretDoor{22, 15, 0, MapDirection::north, 0x4A4B, 1},
    ScriptSecretDoor{22, 15, 15, MapDirection::south, 0x4A4B, 2},
    ScriptSecretDoor{22, 1, 12, MapDirection::south, 0x4A4B, 4},
    ScriptSecretDoor{22, 4, 10, MapDirection::east, 0x4A4B, 8},
    ScriptSecretDoor{22, 12, 8, MapDirection::east, 0x4A4B, 16},
    ScriptSecretDoor{22, 11, 9, MapDirection::east, 0x4A4B, 32},
    ScriptSecretDoor{22, 13, 14, MapDirection::east, 0x4A4B, 64},
    // The laboratories.
    ScriptSecretDoor{23, 0, 10, MapDirection::east, 0x4A4F, 1},
    ScriptSecretDoor{23, 2, 11, MapDirection::east, 0x4A4F, 2},
    ScriptSecretDoor{23, 3, 12, MapDirection::south, 0x4A4F, 4},
    ScriptSecretDoor{23, 9, 12, MapDirection::south, 0x4A4F, 8},
    ScriptSecretDoor{23, 12, 11, MapDirection::east, 0x4A4F, 16},
    // Kuto's Well's catacombs: the script sets 0x4A1F to 255 when found.
    ScriptSecretDoor{32, 5, 7, MapDirection::south, 0x4A1F, 255}};
} // namespace opengold::por
#endif
