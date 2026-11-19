#include "opengold/music_bank.h"

#include <algorithm>
#include <array>
#include <limits>

namespace opengold
{
namespace
{
using Bytes = std::span<const std::uint8_t>;

std::uint16_t word(Bytes bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

struct ScoreVoice
{
    MusicTrack track;
    std::optional<std::uint8_t> note;
    std::uint32_t began{};
    std::uint32_t cursor{};

    void advance(std::uint32_t tick)
    {
        if (tick > cursor)
        {
            track.events.push_back({note.value_or(music_rest), 0, cursor, tick - cursor});
        }
        cursor = tick;
        track.length_ticks = tick;
    }

    void control(std::uint8_t code, std::uint8_t value, std::uint32_t tick)
    {
        advance(tick);
        track.events.push_back({code, value, tick, 0});
    }
};
} // namespace

std::optional<MusicBank> decode_champions_music(Bytes bytes)
{
    // SOUND's header maps the nine AdLib voices to MIDI channels. Each event
    // uses running status and ends with one byte of delay, not a MIDI VLQ.
    constexpr std::size_t header_size = 125, channel_table = 34;
    if (bytes.size() < header_size + 2 || word(bytes, 0) != header_size)
    {
        return std::nullopt;
    }
    std::array<ScoreVoice, 9> voices;
    std::uint32_t tick = 0;
    std::uint8_t status = 0;
    std::size_t offset = header_size;
    bool ended = false;
    while (offset < bytes.size())
    {
        if (bytes[offset] >= 0x80)
        {
            status = bytes[offset++];
        }
        if (status == 0xFC)
        {
            // The renderer plays either a looping (80h) or ending (81h)
            // score once. The terminator has no following delay.
            if (offset + 1 != bytes.size() || (bytes[offset] != 0x80 && bytes[offset] != 0x81))
            {
                return std::nullopt;
            }
            ended = true;
            break;
        }
        const auto command = status & 0xF0;
        const auto channel = status & 0x0F;
        const std::size_t count = command == 0xC0 ? 1 : 2;
        if ((command != 0xC0 && command != 0x90 && command != 0x80) ||
            bytes.size() - offset < count + 1)
        {
            return std::nullopt;
        }
        const auto first = bytes[offset++];
        const auto second = count == 2 ? bytes[offset++] : 0;
        if (first >= 0x80 || second >= 0x80)
        {
            return std::nullopt;
        }
        if (command == 0xC0)
        {
            for (std::size_t index = 0; index < voices.size(); ++index)
            {
                if (bytes[channel_table + index] == channel)
                {
                    voices[index].control(0x85, first, tick);
                }
            }
        }
        else if (command == 0x80 || second == 0)
        {
            for (std::size_t index = 0; index < voices.size(); ++index)
            {
                auto &voice = voices[index];
                if (bytes[channel_table + index] == channel && voice.note == first)
                {
                    voice.advance(tick);
                    voice.note.reset();
                }
            }
        }
        else
        {
            // First free voice of this channel; if all are occupied, the
            // original driver replaces the oldest sounding note.
            std::optional<std::size_t> selected;
            for (std::size_t index = 0; index < voices.size(); ++index)
            {
                if (bytes[channel_table + index] != channel)
                {
                    continue;
                }
                if (!voices[index].note)
                {
                    selected = index;
                    break;
                }
                if (!selected || voices[index].began < voices[*selected].began)
                {
                    selected = index;
                }
            }
            if (selected)
            {
                auto &voice = voices[*selected];
                voice.control(0x86, static_cast<std::uint8_t>(second), tick);
                voice.note = first;
                voice.began = tick;
            }
        }
        const auto delay = bytes[offset++];
        if (tick > std::numeric_limits<std::uint32_t>::max() - delay)
        {
            return std::nullopt;
        }
        tick += delay;
    }
    if (!ended)
    {
        return std::nullopt;
    }
    MusicSong song;
    song.velocity = 127;
    song.first_adlib_track = 0;
    song.adlib_voice_count = voices.size();
    song.ticks_per_second = 1193182.0 / 21845.0;
    song.rhythm_enabled = false;
    for (auto &voice : voices)
    {
        voice.advance(tick);
        song.tracks.push_back(std::move(voice.track));
    }
    MusicBank bank;
    bank.songs.push_back(std::move(song));
    return bank;
}

std::optional<std::vector<std::vector<std::uint8_t>>> decode_champions_instruments(Bytes driver)
{
    // This is the installed AdLib driver's program map and instrument pointer
    // table. Validate both before reading the ten-byte instrument definitions.
    constexpr std::size_t program_map = 0x150, pointers = 0x187, presets = 20;
    if (!is_music_driver(driver, GoldBoxGame::champions_of_krynn) ||
        driver.size() < pointers + presets * 2 || word(driver, 1) + 3 != 0x35B)
    {
        return std::nullopt;
    }
    std::vector<std::vector<std::uint8_t>> instruments;
    for (std::size_t program = 0; program < presets; ++program)
    {
        const auto preset = driver[program_map + program];
        if (preset >= presets)
        {
            return std::nullopt;
        }
        const auto offset = word(driver, pointers + 2 * preset);
        if (offset > driver.size() || driver.size() - offset < 10)
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> instrument(12);
        instrument[0] = driver[offset + 9]; // Signed pitch transposition.
        for (std::size_t reg = 0; reg < 4; ++reg)
        {
            instrument[2 + reg * 2] = driver[offset + reg];
            instrument[3 + reg * 2] = driver[offset + 4 + reg];
        }
        instrument[10] = driver[offset + 8];
        instruments.push_back(std::move(instrument));
    }
    return instruments;
}
} // namespace opengold
