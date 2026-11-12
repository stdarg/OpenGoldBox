#ifndef OPENGOLD_POR_SOUND_H
#define OPENGOLD_POR_SOUND_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace opengold::por
{

struct SpeakerTick
{
    std::uint16_t divisor{};
    bool enabled{};
};

struct SoundEffect
{
    unsigned id{};
    std::string name;
    bool unused{};
    std::uint16_t tick_divisor{};
    std::vector<SpeakerTick> ticks;
    [[nodiscard]] bool audible() const noexcept;
};

// Data-only decompression. No relocation, execution, or filesystem writes.
[[nodiscard]] std::vector<std::uint8_t> unpack_sound_executable(std::span<const std::uint8_t> file);
// Bounded decoder for the PC bank's command subset; also accepts synthetic data.
[[nodiscard]] std::vector<SpeakerTick>
decode_speaker_sequence(std::span<const std::uint8_t> segment,
                        const std::array<std::uint16_t, 4> &entries);
[[nodiscard]] std::vector<SoundEffect>
load_sound_effects(const std::filesystem::path &game_directory);
// The sound effects of Curse of the Azure Bonds' START.EXE, from its unpacked
// image (unpack_sound_executable). Its driver keeps Pool of Radiance's
// sequencer; docs/games/curse.md says where the effects are.
[[nodiscard]] std::vector<SoundEffect>
decode_curse_sound_effects(std::span<const std::uint8_t> image);

} // namespace opengold::por
#endif
