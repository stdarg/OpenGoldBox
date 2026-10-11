#ifndef OPENGOLD_DICE_H
#define OPENGOLD_DICE_H

#include "opengold/random_state.h"
#include <cstdint>
#include <stdexcept>

namespace opengold
{
// SplitMix64 with rejection sampling. Preserve the sequence for existing saves
// and seeded encounters across all SRD dice consumers.
inline int roll_die(std::uint64_t &state, int sides)
{
    // A die needs a side; zero would divide by zero below. Original game
    // scripts supply some dice, so bad data must be refused, not rolled.
    if (sides < 1)
        throw std::invalid_argument("A die needs at least one side");
    const auto count = static_cast<std::uint64_t>(sides);
    const auto threshold = (-count) % count;
    std::uint64_t value;
    do
    {
        value = (state += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;
    }
    while (value < threshold);
    return static_cast<int>(value % count) + 1;
}

inline int roll_die(RandomState &state, int sides)
{
    return roll_die(state.value, sides);
}
} // namespace opengold
#endif
