#ifndef OPENGOLD_SOUND_PLAYER_H
#define OPENGOLD_SOUND_PLAYER_H

#include "opengold/sound_bank.h"

#include <memory>
#include <optional>

namespace opengold::por
{

// Device boundary. Implementations own/release their resources with RAII and
// stop playback on destruction, including failed preparation. The bank passed
// to prepare() is borrowed only for that call; retain no references to it.
class SoundOutput
{
  public:
    virtual ~SoundOutput() = default;
    virtual void prepare(const SoundBank &bank) = 0;
    virtual void play(unsigned id) = 0;
    virtual void stop() noexcept = 0;
    virtual void set_gain(double linear_gain) noexcept = 0;
    [[nodiscard]] virtual bool is_playing() const noexcept = 0;
};

// Single-voice game playback. Owns its bank and injected output; there is no
// dependency on Godot or UI nodes. Call from the output's owning thread.
class SoundPlayer
{
  public:
    SoundPlayer(SoundBank bank, std::unique_ptr<SoundOutput> output);
    ~SoundPlayer();
    SoundPlayer(const SoundPlayer &) = delete;
    SoundPlayer &operator=(const SoundPlayer &) = delete;
    SoundPlayer(SoundPlayer &&) = delete;
    SoundPlayer &operator=(SoundPlayer &&) = delete;

    [[nodiscard]] const SoundBank &bank() const noexcept
    {
        return bank_;
    }
    // Unknown IDs fail before interrupting the currently playing sound.
    void play(unsigned id);
    void stop() noexcept;
    [[nodiscard]] std::optional<unsigned> current_sound() const noexcept;
    // Linear volume in [0, 1]; non-finite/out-of-range values are rejected.
    void set_volume(double volume);
    void set_muted(bool muted) noexcept;
    [[nodiscard]] double volume() const noexcept
    {
        return volume_;
    }
    [[nodiscard]] bool muted() const noexcept
    {
        return muted_;
    }
    [[nodiscard]] double effective_gain() const noexcept
    {
        return muted_ ? 0 : volume_;
    }

  private:
    SoundBank bank_;
    std::unique_ptr<SoundOutput> output_;
    std::optional<unsigned> current_;
    double volume_{1};
    bool muted_{};
};

} // namespace opengold::por
#endif
