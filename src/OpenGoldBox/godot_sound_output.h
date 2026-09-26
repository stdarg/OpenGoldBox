#ifndef OPENGOLD_GODOT_SOUND_OUTPUT_H
#define OPENGOLD_GODOT_SOUND_OUTPUT_H

#include "opengold/sound_player.h"

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <map>

// Main-thread Godot adapter. The supplied node stays scene-owned and must be
// dedicated to this output. An instance ID is a checked, non-owning handle:
// teardown is safe even if the scene frees the node before this adapter.
class GodotSoundOutput final : public opengold::por::SoundOutput
{
  public:
    explicit GodotSoundOutput(godot::AudioStreamPlayer &player);
    ~GodotSoundOutput() override;
    GodotSoundOutput(const GodotSoundOutput &) = delete;
    GodotSoundOutput &operator=(const GodotSoundOutput &) = delete;

    void prepare(const opengold::por::SoundBank &bank) override;
    void play(unsigned id) override;
    void stop() noexcept override;
    void set_gain(double linear_gain) noexcept override;
    [[nodiscard]] bool is_playing() const noexcept override;

  private:
    std::uint64_t player_id_;
    std::map<unsigned, godot::Ref<godot::AudioStreamWAV>> streams_;
    [[nodiscard]] godot::AudioStreamPlayer *player() const noexcept; // Borrowed, never owned.
};
#endif
