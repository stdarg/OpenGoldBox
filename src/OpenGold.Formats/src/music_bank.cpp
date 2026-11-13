#include "opengold/music_bank.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace opengold
{
namespace
{

using Bytes = std::span<const std::uint8_t>;

constexpr std::size_t library_header_size = 12;   // Tag, length, a word, the count.
constexpr std::size_t song_list_header_size = 10; // Tag, length, the count, a byte.
constexpr std::size_t song_header_size = 14;      // Tempo, velocity, tracks, five offsets.
constexpr std::size_t max_tracks = 4;
constexpr std::size_t voices_per_card = 8;
constexpr std::array<std::size_t, 4> instrument_sizes{12, 18, 8,
                                                      8}; // AdLib, CMS, Innovation, Covox.
constexpr std::uint8_t end_of_track = 0xFF;
constexpr std::uint32_t whole_note_units = 0x6900;

// The music's numbers are stored high byte first.
std::uint16_t read_be16(Bytes bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

std::uint32_t read_be32(Bytes bytes, std::size_t offset) noexcept
{
    return (static_cast<std::uint32_t>(read_be16(bytes, offset)) << 16) |
           read_be16(bytes, offset + 2);
}

bool has_tag(Bytes bytes, std::size_t offset, std::string_view tag) noexcept
{
    return bytes.size() - offset >= tag.size() &&
           std::equal(tag.begin(), tag.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

// The units of a note's length: a whole note shortened by halving (bits 0 to
// 2), dotted (bit 3), and played as n of n + 1 equal parts (bits 4 and 5 give
// n / 2). Bit 6 ties the note to the next; bit 7 is not read.
std::uint32_t length_units(std::uint8_t code) noexcept
{
    auto units = whole_note_units >> (code & 0x07);
    if ((code & 0x08) != 0)
    {
        units = units / 2 * 3;
    }
    if (const auto parts = static_cast<std::uint32_t>((code & 0x30) >> 3); parts != 0)
    {
        units = units / (parts + 1) * parts;
    }
    return units;
}

// Secret of the Silver Blades' driver ignores the song's tempo; the later
// drivers divide it by 84.
std::uint16_t tick_divisor(std::uint16_t tempo, GoldBoxGame game) noexcept
{
    constexpr std::uint16_t secret_divisor = 118, tempo_per_unit = 84;
    return game == GoldBoxGame::secret_of_the_silver_blades ? secret_divisor
                                                            : tempo / tempo_per_unit;
}

std::optional<std::vector<std::vector<std::uint8_t>>> decode_instruments(Bytes library)
{
    const auto count = read_be16(library, 10);
    if (count > voices_per_card * instrument_sizes.size() ||
        library.size() < library_header_size + 2 * std::size_t{count})
    {
        return std::nullopt;
    }
    std::vector<std::vector<std::uint8_t>> instruments;
    auto offset = library_header_size + 2 * std::size_t{count};
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto size = read_be16(library, library_header_size + 2 * index);
        if (size != instrument_sizes[index / voices_per_card] || library.size() - offset < size)
        {
            return std::nullopt;
        }
        instruments.emplace_back(library.begin() + static_cast<std::ptrdiff_t>(offset),
                                 library.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += size;
    }
    if (offset != library.size())
    {
        return std::nullopt;
    }
    return instruments;
}

// The events from offset to the end mark, timed as the driver times them: each
// note's ticks are its units over the divisor, the remainders carried.
std::optional<MusicTrack> decode_track(Bytes song, std::size_t offset, std::uint16_t divisor)
{
    MusicTrack track;
    std::uint32_t remainder = 0;
    for (; offset + 1 < song.size(); offset += 2)
    {
        MusicEvent event{song[offset], song[offset + 1], track.length_ticks, 0};
        if (event.code == end_of_track)
        {
            return track;
        }
        if (!event.control())
        {
            const auto units = length_units(event.argument);
            event.length_ticks = units / divisor;
            remainder += units % divisor;
            if (remainder >= divisor)
            {
                ++event.length_ticks;
                remainder -= divisor;
            }
            track.length_ticks += event.length_ticks;
        }
        track.events.push_back(event);
    }
    return std::nullopt; // No end mark.
}

std::optional<MusicSong> decode_song(Bytes song, GoldBoxGame game)
{
    if (song.size() < song_header_size)
    {
        return std::nullopt;
    }
    MusicSong decoded;
    decoded.tempo = read_be16(song, 0);
    decoded.velocity = song[2];
    decoded.tick_divisor = tick_divisor(decoded.tempo, game);
    const std::size_t track_count = song[3];
    if (decoded.tick_divisor == 0 || track_count == 0 || track_count > max_tracks)
    {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < track_count; ++index)
    {
        const auto start = song_header_size + read_be16(song, 4 + 2 * index);
        if (start >= song.size())
        {
            return std::nullopt;
        }
        auto track = decode_track(song, start, decoded.tick_divisor);
        if (!track)
        {
            return std::nullopt;
        }
        decoded.tracks.push_back(std::move(*track));
    }
    return decoded;
}

std::optional<std::vector<MusicSong>> decode_songs(Bytes list, GoldBoxGame game)
{
    if (list.size() < song_list_header_size)
    {
        return std::nullopt;
    }
    const std::size_t count = list[8];
    const auto first_song = song_list_header_size + 2 * (count + 1);
    if (count == 0 || list.size() < first_song)
    {
        return std::nullopt;
    }
    std::vector<MusicSong> songs;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto start = first_song + read_be16(list, song_list_header_size + 2 * index);
        const auto end = first_song + read_be16(list, song_list_header_size + 2 * (index + 1));
        if (start > end || end > list.size())
        {
            return std::nullopt;
        }
        auto song = decode_song(list.subspan(start, end - start), game);
        if (!song)
        {
            return std::nullopt;
        }
        songs.push_back(std::move(*song));
    }
    return songs;
}

} // namespace

std::optional<MusicBank> decode_music_bank(Bytes bytes, GoldBoxGame game)
{
    if (bytes.size() < library_header_size || !has_tag(bytes, 0, "GLBR"))
    {
        return std::nullopt;
    }
    const std::size_t library_size = read_be32(bytes, 4);
    if (library_size < library_header_size || library_size > bytes.size() ||
        !has_tag(bytes, library_size, "SLBR") ||
        bytes.size() - library_size < song_list_header_size)
    {
        return std::nullopt;
    }
    const std::size_t list_size = read_be32(bytes, library_size + 4);
    // Some files carry stray bytes after the songs, which the driver never reads.
    if (list_size < song_list_header_size || list_size > bytes.size() - library_size)
    {
        return std::nullopt;
    }
    auto instruments = decode_instruments(bytes.first(library_size));
    auto songs = decode_songs(bytes.subspan(library_size, list_size), game);
    if (!instruments || !songs)
    {
        return std::nullopt;
    }
    return MusicBank{std::move(*instruments), std::move(*songs)};
}

bool is_music_driver(Bytes bytes) noexcept
{
    // Its far entry sets its data segment from its code segment, and it names
    // the banks it refuses.
    constexpr std::array<std::uint8_t, 8> entry{0x55, 0x8B, 0xEC, 0x1E, 0x06, 0x8C, 0xC8, 0x05};
    constexpr std::string_view refusal = "Invalid Parts File";
    return bytes.size() > entry.size() && std::equal(entry.begin(), entry.end(), bytes.begin()) &&
           std::search(bytes.begin(), bytes.end(), refusal.begin(), refusal.end()) != bytes.end();
}

} // namespace opengold
