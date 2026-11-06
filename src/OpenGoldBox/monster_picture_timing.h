#ifndef OPENGOLDBOX_MONSTER_PICTURE_TIMING_H
#define OPENGOLDBOX_MONSTER_PICTURE_TIMING_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace presentation
{
// The frame of a looping monster close-up on screen `seconds` after it
// appeared. Frame delays count ticks of the PC's 18.2 Hz timer. Zero-tick
// frames are passed over, so records like the orc alternate between two held
// poses.
[[nodiscard]] inline std::size_t monster_frame_at(std::span<const std::uint32_t> delays,
                                                  double seconds)
{
    constexpr double tick_seconds = 1.0 / 18.2;
    std::uint64_t total_ticks = 0;
    for (const auto delay : delays)
    {
        total_ticks += delay;
    }
    if (!total_ticks)
    {
        return 0;
    }
    auto tick = static_cast<std::uint64_t>(seconds / tick_seconds) % total_ticks;
    for (std::size_t n = 0; n < delays.size(); ++n)
    {
        if (tick < delays[n])
        {
            return n;
        }
        tick -= delays[n];
    }
    return 0;
}

// How long a close-up plays before the fight opens by itself: one full loop, and
// long enough to see one that barely has any.
[[nodiscard]] inline double monster_picture_lasts(std::span<const std::uint32_t> delays)
{
    std::uint64_t total_ticks = 0;
    for (const auto delay : delays)
    {
        total_ticks += delay;
    }
    return std::max(1.0, static_cast<double>(total_ticks) / 18.2);
}

// The frames a close-up holds on screen, in order: those with a delay. New art
// draws only these.
[[nodiscard]] inline std::vector<std::size_t> held_frames(std::span<const std::uint32_t> delays)
{
    std::vector<std::size_t> held;
    for (std::size_t n = 0; n < delays.size(); ++n)
    {
        if (delays[n] > 0)
        {
            held.push_back(n);
        }
    }
    return held;
}
} // namespace presentation

#endif
