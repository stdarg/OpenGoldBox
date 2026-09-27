#ifndef OPENGOLD_SPEAKER_AUDIO_H
#define OPENGOLD_SPEAKER_AUDIO_H
#include "opengold/por_sound.h"

namespace opengold::por
{
inline constexpr unsigned speaker_sample_rate = 48000;

// An owning value. Views remain valid until this buffer is moved or destroyed.
class PcmBuffer
{
  public:
    explicit PcmBuffer(std::vector<std::int16_t> samples);

    [[nodiscard]] std::span<const std::int16_t> samples() const noexcept
    {
        return samples_;
    }

    [[nodiscard]] static constexpr unsigned sample_rate() noexcept
    {
        return speaker_sample_rate;
    }

    [[nodiscard]] static constexpr unsigned channels() noexcept
    {
        return 1;
    }

    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] std::vector<std::uint8_t> little_endian_bytes() const;

  private:
    std::vector<std::int16_t> samples_;
};

// Mono signed PCM; playback gain/mute belong to the presentation layer.
[[nodiscard]] std::vector<std::int16_t> render_speaker_audio(const SoundEffect &effect);
} // namespace opengold::por
#endif
