#ifndef OPENGOLD_MUSIC_BANK_H
#define OPENGOLD_MUSIC_BANK_H

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "opengold/gold_box_game.h"

// The music of the later games: the songs their music driver plays (Secret of
// the Silver Blades' SOUND.DAX, MUSIC.DAX in Gateway, Pools of Darkness and
// Treasures of the Savage Frontier, and the .SMS files of Pools of Darkness'
// introduction). The layout: docs/games/later-sound.md.
namespace opengold
{

// The driver counts time in ticks of the PC's timer, which it sets to 72.8 a
// second (1193182 / 16384).
inline constexpr double music_ticks_per_second = 1193182.0 / 16384.0;

inline constexpr std::uint8_t music_rest = 0x80;

struct MusicEvent
{
    // A note's pitch in semitones (00h to 7Fh), music_rest, or a control
    // (81h to FEh), which plays nothing and takes no time.
    std::uint8_t code{};
    // A note's or rest's length code (docs/games/later-sound.md), or the
    // control's value.
    std::uint8_t argument{};
    std::uint32_t start_tick{};
    std::uint32_t length_ticks{};

    [[nodiscard]] bool control() const noexcept
    {
        return code > music_rest;
    }

    // The next note goes on sounding as this one, without striking again.
    [[nodiscard]] bool tied() const noexcept
    {
        return !control() && (argument & 0x40) != 0;
    }
};

struct MusicTrack
{
    std::vector<MusicEvent> events;
    std::uint32_t length_ticks{};
};

struct MusicSong
{
    std::uint16_t tempo{};
    // Units of a note's length a tick: a whole note is 6900h units.
    std::uint16_t tick_divisor{};
    std::uint8_t velocity{};
    std::vector<MusicTrack> tracks;
};

struct MusicBank
{
    // Eight voices for each sound card, in the driver's order: AdLib (12
    // bytes each), Creative Music System (18), Innovation (8), Covox (8).
    std::vector<std::vector<std::uint8_t>> instruments;
    std::vector<MusicSong> songs;
};

// A parameter block of the music driver: its instrument library ("GLBR") and
// its songs ("SLBR"), every track read to its end. The game decides how the
// driver turns a song's tempo into ticks. nullopt for any other bytes.
[[nodiscard]] std::optional<MusicBank> decode_music_bank(std::span<const std::uint8_t> bytes,
                                                         GoldBoxGame game);

// Whether the bytes are the music driver program that plays these banks. It is
// recognized, not run: OpenGoldBox reads the banks instead.
[[nodiscard]] bool is_music_driver(std::span<const std::uint8_t> bytes) noexcept;

} // namespace opengold

#endif
