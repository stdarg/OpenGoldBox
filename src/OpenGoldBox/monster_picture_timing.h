#ifndef OPENGOLDBOX_MONSTER_PICTURE_TIMING_H
#define OPENGOLDBOX_MONSTER_PICTURE_TIMING_H

#include <cstddef>
#include <cstdint>
#include <span>

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
        total_ticks += delay;
    if (!total_ticks)
        return 0;
    auto tick = static_cast<std::uint64_t>(seconds / tick_seconds) % total_ticks;
    for (std::size_t n = 0; n < delays.size(); ++n)
    {
        if (tick < delays[n])
            return n;
        tick -= delays[n];
    }
    return 0;
}
} // namespace presentation

#endif
