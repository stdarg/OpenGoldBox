// The later games' music and digitized sounds (docs/games/later-sound.md):
// the music driver's banks of instruments and songs, the driver program beside
// them, and Pools of Darkness' and Treasures of the Savage Frontier's sound
// effects. The real files are read when OPENGOLD_GOLDBOX_DIR names the folder
// holding SECRET, GATEWAY, DARKNESS and TREASURE.
#include <string_view>
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/gateway_formats.h"
#include "opengold/music_bank.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;

using Bytes = std::vector<std::uint8_t>;

constexpr auto darkness = GoldBoxGame::pools_of_darkness;

void append_be16(Bytes &bytes, unsigned value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void append_be32(Bytes &bytes, unsigned value)
{
    append_be16(bytes, value >> 16);
    append_be16(bytes, value & 0xFFFF);
}

void append_text(Bytes &bytes, const std::string &text)
{
    bytes.insert(bytes.end(), text.begin(), text.end());
}

// A library of two AdLib voices (instrument sizes as given), then one song of
// the given tempo whose tracks are the given event bytes, end marks included.
Bytes bank(unsigned tempo, const std::vector<Bytes> &tracks,
           const std::vector<unsigned> &instrument_sizes = {12, 12})
{
    Bytes library;
    append_text(library, "GLBR");
    append_be32(library, 0); // Its length, set below.
    append_be16(library, 0xFFFF);
    append_be16(library, static_cast<unsigned>(instrument_sizes.size()));
    for (const auto size : instrument_sizes)
    {
        append_be16(library, size);
    }
    for (const auto size : instrument_sizes)
    {
        library.insert(library.end(), size, 0x11);
    }
    const auto library_size = static_cast<unsigned>(library.size());
    library[6] = static_cast<std::uint8_t>(library_size >> 8);
    library[7] = static_cast<std::uint8_t>(library_size & 0xFF);

    Bytes song;
    append_be16(song, tempo);
    song.push_back(0x7F);
    song.push_back(static_cast<std::uint8_t>(tracks.size()));
    Bytes events;
    std::vector<unsigned> offsets;
    for (const auto &track : tracks)
    {
        offsets.push_back(static_cast<unsigned>(events.size()));
        events.insert(events.end(), track.begin(), track.end());
    }
    offsets.resize(5, static_cast<unsigned>(events.size()));
    for (const auto offset : offsets)
    {
        append_be16(song, offset);
    }
    song.insert(song.end(), events.begin(), events.end());

    Bytes list;
    append_text(list, "SLBR");
    append_be32(list, static_cast<unsigned>(10 + 4 + song.size()));
    list.push_back(1); // One song.
    list.push_back(0);
    append_be16(list, 0);
    append_be16(list, static_cast<unsigned>(song.size()));
    list.insert(list.end(), song.begin(), song.end());

    library.insert(library.end(), list.begin(), list.end());
    return library;
}

// Tempo 2A00h is 128 units a tick: a quarter note (code 2) of 6720 units is 52
// ticks with 64 left over, which the dotted quarter's 96 carry into a tick.
void notes_are_timed_as_the_driver_times_them()
{
    const auto decoded = opengold::decode_music_bank(
        bank(0x2A00, {{0x87, 0x01, 0x3C, 0x02, 0x80, 0x0A, 0x40, 0x52, 0x41, 0x00, 0xFF, 0xFF},
                      {0x30, 0x00, 0xFF, 0xFF}}),
        darkness);
    check(decoded.has_value(), "The bank decodes");
    check(decoded->instruments.size() == 2 && decoded->instruments[1].size() == 12,
          "Two AdLib voices of 12 bytes");
    check(decoded->songs.size() == 1, "One song");
    const auto &song = decoded->songs[0];
    check(song.tempo == 0x2A00 && song.tick_divisor == 128 && song.velocity == 0x7F,
          "Tempo 2A00h divides by 84 into 128 units a tick");
    check(song.tracks.size() == 2, "Two tracks");
    const auto &events = song.tracks[0].events;
    check(events.size() == 5, "A control, a note, a rest, a tied triplet and a whole note");
    check(events[0].control() && events[0].code == 0x87 && events[0].argument == 0x01 &&
              events[0].length_ticks == 0,
          "A control takes no time");
    check(events[1].code == 0x3C && events[1].start_tick == 0 && events[1].length_ticks == 52,
          "A quarter note is 52 ticks");
    check(events[2].code == opengold::music_rest && events[2].start_tick == 52 &&
              events[2].length_ticks == 79,
          "A dotted quarter rest carries the remainders into a 79th tick");
    check(events[3].tied() && events[3].start_tick == 131 && events[3].length_ticks == 35,
          "A tied quarter triplet is two thirds of 6720 units");
    check(!events[4].tied() && events[4].start_tick == 166 && events[4].length_ticks == 210,
          "A whole note is 210 ticks");
    check(song.tracks[0].length_ticks == 376, "The track ends after 376 ticks");
    check(song.tracks[1].length_ticks == 210, "The second track holds one whole note");
}

void secrets_driver_ignores_the_tempo()
{
    const auto decoded = opengold::decode_music_bank(bank(0x2A00, {{0x30, 0x00, 0xFF, 0xFF}}),
                                                     GoldBoxGame::secret_of_the_silver_blades);
    check(decoded.has_value() && decoded->songs[0].tick_divisor == 118,
          "Secret of the Silver Blades counts 118 units a tick");
    check(decoded->songs[0].tracks[0].length_ticks == 227,
          "Its whole note is 26880 units over 118");
}

void malformed_banks_are_refused()
{
    const Bytes track{0x30, 0x00, 0xFF, 0xFF};
    check(opengold::decode_music_bank(bank(0x2A00, {track}), darkness).has_value(),
          "The well-formed bank decodes");
    check(!opengold::decode_music_bank(bank(0x2A00, {{0x30, 0x00, 0x31, 0x00}}), darkness),
          "A track without its end mark is refused");
    check(!opengold::decode_music_bank(bank(0x2A00, {track}, {12, 8}), darkness),
          "An AdLib voice must be 12 bytes");
    check(!opengold::decode_music_bank(bank(83, {track}), darkness),
          "A tempo below 84 would divide by zero");
    check(!opengold::decode_music_bank(bank(0x2A00, {}), darkness), "A song needs a track");
    check(!opengold::decode_music_bank(bank(0x2A00, {track, track, track, track, track}), darkness),
          "A song has at most four tracks");
    auto truncated = bank(0x2A00, {track});
    truncated.resize(truncated.size() - 3);
    check(!opengold::decode_music_bank(truncated, darkness), "A cut-off bank is refused");
    auto renamed = bank(0x2A00, {track});
    renamed[0] = 'X';
    check(!opengold::decode_music_bank(renamed, darkness), "The library starts GLBR");
}

void the_driver_is_recognized()
{
    Bytes driver{0x55, 0x8B, 0xEC, 0x1E, 0x06, 0x8C, 0xC8, 0x05, 0xD1, 0x02};
    append_text(driver, "Invalid Parts File");
    check(opengold::is_music_driver(driver), "The driver's entry and its refusal");
    driver[0] = 0x90;
    check(!opengold::is_music_driver(driver), "Another program is not the driver");
    check(!opengold::is_music_driver(bank(0x2A00, {{0xFF, 0xFF}})), "A bank is not the driver");
}

// An Amiga sound file as the games store it: every byte's top bit flipped.
Bytes flipped_amiga_sound(std::uint16_t rate, const Bytes &body, std::uint8_t packing = 0)
{
    Bytes file;
    const auto text = [&file](std::string_view letters)
    {
        file.insert(file.end(), letters.begin(), letters.end());
    };
    const auto big_endian = [&file](std::uint32_t value, int size)
    {
        for (int shift = (size - 1) * 8; shift >= 0; shift -= 8)
        {
            file.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    };
    text("FORM");
    big_endian(static_cast<std::uint32_t>(4 + 8 + 20 + 8 + body.size()), 4);
    text("8SVX");
    text("VHDR");
    big_endian(20, 4);
    big_endian(static_cast<std::uint32_t>(body.size()), 4);
    big_endian(0, 4);
    big_endian(0, 4);
    big_endian(rate, 2);
    file.push_back(1);
    file.push_back(packing);
    big_endian(0x10000, 4);
    text("BODY");
    big_endian(static_cast<std::uint32_t>(body.size()), 4);
    file.insert(file.end(), body.begin(), body.end());
    for (auto &byte : file)
    {
        byte ^= 0x80;
    }
    return file;
}

void later_sound_effects_play_at_9000()
{
    const Bytes samples(100, 0x80);
    const auto effect = opengold::gateway::decode_later_sound_sample(4, samples);
    check(effect.has_value() && effect->rate == 9000 && effect->samples == samples,
          "An effect that is not a sound file plays whole at 9000 samples a second");
    const Bytes body{0x00, 0x7F, 0x80, 0xFF};
    const auto amiga =
        opengold::gateway::decode_later_sound_sample(2, flipped_amiga_sound(8363, body));
    Bytes unsigned_body;
    for (const auto sample : body)
    {
        unsigned_body.push_back(static_cast<std::uint8_t>(sample ^ 0x80));
    }
    check(amiga && amiga->rate == 8363 && amiga->samples == unsigned_body,
          "A sound file plays its samples alone at its own rate, without its header's click");
    check(!opengold::gateway::decode_later_sound_sample(2, flipped_amiga_sound(8363, body, 1)),
          "A packed sound file is refused");
    check(!opengold::gateway::decode_later_sound_sample(0, samples), "Record 0 is never loaded");
    check(!opengold::gateway::decode_later_sound_sample(12, samples), "Nor is record 12");
    check(!opengold::gateway::decode_later_sound_sample(1, Bytes(9001, 0x80)),
          "An effect fits the 9000-byte buffer");
    check(!opengold::gateway::decode_later_sound_sample(1, Bytes{}), "An effect has samples");
}

Bytes read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<opengold::DaxRecord> archive(const std::filesystem::path &path)
{
    auto decoded = opengold::decode_dax_archive(read_file(path));
    check(static_cast<bool>(decoded), path.filename().string() + " is an archive");
    return std::move(decoded.records);
}

const Bytes &record(const std::vector<opengold::DaxRecord> &records, unsigned id)
{
    for (const auto &entry : records)
    {
        if (entry.id == id)
        {
            return entry.bytes;
        }
    }
    throw opengold::test::CheckFailure("record " + std::to_string(id) + " is missing");
}

void darkness_music_reads(const std::filesystem::path &folder)
{
    const auto music = archive(folder / "MUSIC.DAX");
    check(music.size() == 9, "Pools of Darkness' MUSIC.DAX holds the driver and eight banks");
    check(opengold::is_music_driver(record(music, 0)), "Record 0 is the driver");
    check(record(music, 0) == read_file(folder / "SOUND.COM"),
          "VACE's SOUND.COM is the same driver");
    check(record(music, 1) == read_file(folder / "P1.SMS") &&
              record(music, 3) == read_file(folder / "P3.SMS"),
          "The introduction's and finale's songs are banks 1 and 3");

    const auto bank4 = opengold::decode_music_bank(record(music, 4), darkness);
    check(bank4.has_value(), "Bank 4 decodes");
    check(bank4->instruments.size() == 32, "Eight voices for each of four cards");
    check(bank4->instruments[2] ==
              Bytes{0x00, 0x00, 0x30, 0x71, 0xC8, 0x80, 0xD5, 0x61, 0x19, 0x1B, 0x0C, 0x0E},
          "The third AdLib voice");
    const auto &song = bank4->songs.at(0);
    check(song.tempo == 0x2300 && song.tick_divisor == 106, "Tempo 2300h: 106 units a tick");
    check(song.tracks.size() == 4, "Four tracks");
    const auto &first = song.tracks[0];
    check(first.events.size() == 36 && first.length_ticks == 1014,
          "The first track: 36 events over 1014 ticks");
    check(first.events[4].code == 0x36 && first.events[4].argument == 0x03 &&
              first.events[4].length_ticks == 31,
          "Its first note, an eighth, is 31 ticks");
    check(song.tracks[2].length_ticks == 1046, "The third track runs 32 ticks longer");
}

void real_files_decode()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    if (!games)
    {
        std::cout << "OPENGOLD_GOLDBOX_DIR is not set; the real files are not checked.\n";
        return;
    }
    const std::filesystem::path root(games);
    if (opengold::identify_game(root / "DARKNESS") == darkness)
    {
        darkness_music_reads(root / "DARKNESS");
    }
    if (opengold::identify_game(root / "TREASURE") == GoldBoxGame::treasures_of_the_savage_frontier)
    {
        const auto music = archive(root / "TREASURE" / "MUSIC.DAX");
        const auto bank = opengold::decode_music_bank(
            record(music, 1), GoldBoxGame::treasures_of_the_savage_frontier);
        check(bank && bank->songs.at(0).tick_divisor == 182 &&
                  bank->songs[0].tracks.at(3).length_ticks == 3544,
              "Treasures' song: 182 units a tick, its tracks 3544 ticks long");
        const auto sounds = archive(root / "TREASURE" / "ZOUNDS.DAX");
        const auto click = opengold::gateway::decode_later_sound_sample(9, record(sounds, 9));
        check(click && click->samples.size() == 132, "Treasures' effect 9 is 132 samples");
        // Most effects are Amiga sound files with every byte's top bit flipped;
        // the original played them header and all.
        const auto first = opengold::gateway::decode_later_sound_sample(1, record(sounds, 1));
        check(first && first->samples.size() == 2468 - 104 && first->rate == 9861,
              "Effect 1 plays without its 104-byte header, at its own 9861 a second");
    }
    if (opengold::identify_game(root / "SECRET") == GoldBoxGame::secret_of_the_silver_blades)
    {
        const auto sound = archive(root / "SECRET" / "SOUND.DAX");
        check(opengold::is_music_driver(record(sound, 1)), "Secret's record 1 is the driver");
        const auto bank =
            opengold::decode_music_bank(record(sound, 2), GoldBoxGame::secret_of_the_silver_blades);
        check(bank && bank->instruments.size() == 31, "Secret's library has 31 voices");
        const auto &song = bank->songs.at(0);
        check(song.tempo == 0x2200 && song.tick_divisor == 118, "Secret's title music");
        for (const auto &track : song.tracks)
        {
            check(track.length_ticks == 6150, "Each of its tracks lasts 6150 ticks");
        }
        check(opengold::take_census(root / "SECRET", GoldBoxGame::secret_of_the_silver_blades)
                  .complete(),
              "Secret's census is complete");
    }
    if (opengold::identify_game(root / "GATEWAY") == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        const auto music = archive(root / "GATEWAY" / "MUSIC.DAX");
        const auto bank = opengold::decode_music_bank(record(music, 1),
                                                      GoldBoxGame::gateway_to_the_savage_frontier);
        check(bank && bank->songs.at(0).tick_divisor == 137 &&
                  bank->songs[0].tracks.at(0).length_ticks == 3973,
              "Gateway's song: 137 units a tick, 3973 ticks");
    }
}

} // namespace

int main()
{
    try
    {
        notes_are_timed_as_the_driver_times_them();
        secrets_driver_ignores_the_tempo();
        malformed_banks_are_refused();
        the_driver_is_recognized();
        later_sound_effects_play_at_9000();
        real_files_decode();
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "late sound tests passed\n";
    return 0;
}
