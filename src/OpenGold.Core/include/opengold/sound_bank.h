#ifndef OPENGOLD_SOUND_BANK_H
#define OPENGOLD_SOUND_BANK_H

#include "opengold/speaker_audio.h"

namespace opengold::por
{

struct SoundClip
{
    unsigned id{}; // Original sound-directory ID, independent of UI ordering.
    std::string name;
    bool unused{};
    bool audible{};
    PcmBuffer pcm;
};

// Owns immutable prepared clips. No Godot, audio device, environment settings,
// or extracted files. Construction either succeeds completely or throws.
class SoundBank
{
  public:
    // Accepts decoded effects, including hand-authored effects for unit tests.
    explicit SoundBank(std::vector<SoundEffect> effects);
    // The game's sound effects; Pool of Radiance's unless told otherwise.
    [[nodiscard]] static SoundBank load(const std::filesystem::path &game_directory,
                                        GoldBoxGame game = GoldBoxGame::pool_of_radiance);
    // A piece of the later games' music (load_music_bank), as an AdLib card
    // played it: one clip, whose ID is the piece.
    [[nodiscard]] static SoundBank load_music(const std::filesystem::path &game_directory,
                                              GoldBoxGame game, unsigned piece);

    [[nodiscard]] std::span<const SoundClip> clips() const noexcept
    {
        return clips_;
    }

    [[nodiscard]] const SoundClip &at(unsigned id) const;

  private:
    std::vector<SoundClip> clips_;
};

} // namespace opengold::por
#endif
