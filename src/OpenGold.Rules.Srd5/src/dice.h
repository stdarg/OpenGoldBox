#ifndef OPENGOLD_SRD5_DICE_H
#define OPENGOLD_SRD5_DICE_H

#include <cstdint>

namespace opengold::srd5
{
// SplitMix64 with rejection sampling. Preserve the sequence for existing saves
// and seeded encounters across all SRD dice consumers.
inline int roll_die(std::uint64_t &state, int sides)
{
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
} // namespace opengold::srd5
#endif
