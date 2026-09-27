#include "opengold/speaker_audio.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace opengold::por
{
PcmBuffer::PcmBuffer(std::vector<std::int16_t> samples) : samples_(std::move(samples))
{
}

double PcmBuffer::duration() const noexcept
{
    return static_cast<double>(samples_.size()) / sample_rate();
}

std::vector<std::uint8_t> PcmBuffer::little_endian_bytes() const
{
    std::vector<std::uint8_t> bytes(samples_.size() * 2);
    for (std::size_t i = 0; i < samples_.size(); ++i)
    {
        const auto value = static_cast<std::uint16_t>(samples_[i]);
        bytes[i * 2] = value & 255;
        bytes[i * 2 + 1] = value >> 8;
    }
    return bytes;
}

namespace
{
constexpr double pit_clock = 14318180.0 / 12.0;

// Integral of a mode-3 square wave, including the odd-divisor duty cycle.
double integral(double x, double period)
{
    const auto high = std::ceil(period / 2);
    const auto cycles = std::floor(x / period);
    const auto remainder = x - cycles * period;
    return cycles * (2 * high - period) + 2 * std::min(remainder, high) - remainder;
}
} // namespace

std::vector<std::int16_t> render_speaker_audio(const SoundEffect &effect)
{
    if (!effect.tick_divisor || effect.ticks.size() > 4096)
        throw std::runtime_error("Invalid PC speaker timing");
    const double duration = effect.ticks.size() * static_cast<double>(effect.tick_divisor);
    const auto count =
        static_cast<std::size_t>(std::ceil(duration * speaker_sample_rate / pit_clock));
    std::vector<std::int16_t> result(count);
    double phase = 0;
    constexpr double step = pit_clock / speaker_sample_rate;
    for (std::size_t sample = 0; sample < count; ++sample)
    {
        auto time = sample * step;
        const auto end = std::min((sample + 1) * step, duration);
        double area = 0;
        while (time < end)
        {
            const auto tick = static_cast<std::size_t>(time / effect.tick_divisor);
            if (tick >= effect.ticks.size())
                break;
            const auto until = std::min(end, (tick + 1) * static_cast<double>(effect.tick_divisor));
            const auto &sound = effect.ticks[tick];
            const double period = sound.divisor ? sound.divisor : 65536;
            phase = std::fmod(phase, period);
            if (sound.enabled)
                area += integral(phase + until - time, period) - integral(phase, period);
            phase = std::fmod(phase + until - time, period);
            time = until;
        }
        result[sample] = static_cast<std::int16_t>(std::clamp(area / step, -1.0, 1.0) * 12000);
    }
    return result;
}
} // namespace opengold::por
