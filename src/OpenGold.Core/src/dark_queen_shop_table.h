#ifndef OPENGOLD_DARK_QUEEN_SHOP_TABLE_H
#define OPENGOLD_DARK_QUEEN_SHOP_TABLE_H

#include <array>

// Numeric shop facts verified byte for byte against installed DQK.EXE.
namespace opengold::por::dark_queen_shops
{
inline constexpr std::array<unsigned, 9> shop_first{{
    1,
    32,
    38,
    46,
    54,
    62,
    67,
    72,
    77,
}};
inline constexpr std::array<unsigned, 9> shop_last{{
    31,
    37,
    45,
    53,
    61,
    66,
    71,
    76,
    121,
}};
inline constexpr std::array<unsigned, 121> shop_items{{
    1,   2,   3,   4,   5,   6,  7,  8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,
    19,  20,  21,  22,  23,  24, 25, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  49,
    223, 36,  37,  38,  39,  40, 41, 42,  43,  5,   9,   1,   11,  15,  20,  61,  86,  22,
    25,  1,   11,  15,  20,  59, 80, 243, 244, 245, 246, 247, 250, 250, 255, 255, 255, 47,
    208, 238, 240, 241, 1,   2,  3,  4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20, 21, 22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
    33,  34,  35,  49,  223, 36, 37, 38,  39,  40,  41,  42,  43,
}};
} // namespace opengold::por::dark_queen_shops

#endif
