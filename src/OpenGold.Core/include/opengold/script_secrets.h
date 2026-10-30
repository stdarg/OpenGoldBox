#ifndef OPENGOLD_SCRIPT_SECRETS_H
#define OPENGOLD_SCRIPT_SECRETS_H
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

// A trap an original script springs on square (x, y) of `map`, which the Wand
// of Secrets can point out (SECRET-1). It is armed until the script variable
// `flag` holds `sprung` in its `mask` bits: sprung, disarmed, or its treasure
// taken.
struct ScriptTrap
{
    unsigned map{}, x{}, y{};
    std::uint16_t flag{}, mask{0xFFFF}, sprung{1};
};

// Kovel Mansion's traps (ECL3:14), which ask for Find Traps (0xB3A2), and the
// kobold caves' (ECL8:13, 0xADB4).
inline constexpr std::array script_traps{
    // Kovel Mansion's trapped doorways, events 3 to 8: each springs once, its
    // slot of the table at 0x4900 then 1.
    ScriptTrap{14, 11, 10, 0x4900},
    ScriptTrap{14, 13, 11, 0x4901},
    ScriptTrap{14, 12, 13, 0x4902},
    ScriptTrap{14, 3, 11, 0x4903},
    ScriptTrap{14, 4, 12, 0x4903},
    ScriptTrap{14, 5, 6, 0x4904},
    ScriptTrap{14, 4, 3, 0x4905},
    // Cabinet C (14,10) and the wooden cabinet (3,13), until opened.
    ScriptTrap{14, 14, 10, 0x4AEC},
    ScriptTrap{14, 3, 13, 0x4AED},
    // The four caskets (6,7), until each is emptied.
    ScriptTrap{14, 6, 7, 0x4A46, 15, 15},
    // The weapons' gas (3,4), until it is set off or disarmed.
    ScriptTrap{14, 3, 4, 0x4A15},
    // The kobold caves: the pit, the deadfall, the net and the spikes.
    ScriptTrap{13, 6, 13, 0x4A01},
    ScriptTrap{13, 8, 12, 0x4A03, 0xFFFF, 2},
    ScriptTrap{13, 10, 9, 0x4A04},
    ScriptTrap{13, 11, 9, 0x4A05}};
} // namespace opengold::por
#endif
