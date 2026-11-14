// The later games' music played as the AdLib played it (docs/games/later-sound.md,
// "Playing the music"): FmSynth's notes and envelopes, the driver's timing of a
// song, and the real banks when OPENGOLD_GOLDBOX_DIR names the folder holding
// SECRET, GATEWAY, DARKNESS and TREASURE. Everything renders to memory; nothing
// is played.
#include "opengold/fm_synth.h"
#include "opengold/music_render.h"
#include "opengold/sound_bank.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::MusicBank;
using opengold::MusicEvent;
using opengold::test::check;

constexpr unsigned rate = 48000;
const double samples_per_tick = rate / opengold::music_ticks_per_second;

// A plain sine: a silent modulator, a carrier at full level that strikes at
// once, holds, and stops at once on release.
const std::vector<std::uint8_t> sine_instrument{0x00, 0x00, 0x21, 0x21, 0x3F, 0x00,
                                                0xF0, 0xF0, 0x0F, 0x0F, 0x00, 0x00};

// A bank of one song whose AdLib tracks (1 to 3) hold the given notes, each
// event a code and a length in ticks; track 0, the PC speaker's, is empty.
MusicBank bank(const std::vector<std::vector<MusicEvent>> &adlib_tracks)
{
    opengold::MusicSong song;
    song.velocity = 0x7F;
    song.tracks.emplace_back();
    for (std::size_t index = 0; index < adlib_tracks.size(); ++index)
    {
        opengold::MusicTrack track;
        track.events.push_back({0x81, static_cast<std::uint8_t>(index), 0, 0});
        for (auto event : adlib_tracks[index])
        {
            event.start_tick = track.length_ticks;
            track.length_ticks += event.length_ticks;
            track.events.push_back(event);
        }
        song.tracks.push_back(std::move(track));
    }
    return MusicBank{std::vector<std::vector<std::uint8_t>>(8, sine_instrument), {song}};
}

MusicEvent note(std::uint8_t pitch, std::uint32_t ticks, bool tied = false)
{
    return {pitch, static_cast<std::uint8_t>(tied ? 0x40 : 0x00), 0, ticks};
}

std::size_t sample_at_tick(double tick)
{
    return static_cast<std::size_t>(tick * samples_per_tick);
}

double peak(std::span<const std::int16_t> samples)
{
    double highest = 0;
    for (const auto sample : samples)
    {
        highest = std::max(highest, std::abs(static_cast<double>(sample)));
    }
    return highest;
}

double rms(std::span<const std::int16_t> samples)
{
    double sum = 0;
    for (const auto sample : samples)
    {
        sum += static_cast<double>(sample) * sample;
    }
    return samples.empty() ? 0 : std::sqrt(sum / static_cast<double>(samples.size()));
}

// The frequency from rising zero crossings, placed between samples.
double frequency(std::span<const std::int16_t> samples)
{
    double first = -1, last = -1;
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
    {
        const double before = samples[index - 1], after = samples[index];
        if (before < 0 && after >= 0)
        {
            const auto at = static_cast<double>(index - 1) + before / (before - after);
            if (first < 0)
            {
                first = at;
            }
            last = at;
            ++crossings;
        }
    }
    return crossings < 2 ? 0 : (crossings - 1) * rate / (last - first);
}

// The driver's note 69 is A above middle C: frequency number 243h in block 4,
// which the chip's clock makes 439.2 Hz. It sounds for its 128 ticks and stops.
void a_note_has_its_pitch_and_length()
{
    const auto samples = opengold::render_music(bank({{note(69, 128)}}), 0, rate);
    const std::span<const std::int16_t> all(samples);
    const auto expected_hz = 0x243 * opengold::fm_chip_rate * 16 / (1 << 20);
    const auto measured = frequency(all.subspan(sample_at_tick(10), rate / 2));
    check(std::abs(measured - expected_hz) < 0.5,
          "The note sounds at 439.2 Hz, not " + std::to_string(measured));
    check(rms(all.subspan(sample_at_tick(120), sample_at_tick(7))) > 4000,
          "It sounds until its last ticks");
    const auto end = sample_at_tick(128);
    // The driver releases the note on the tick after its last.
    check(samples.size() >= end && samples.size() < end + samples_per_tick + rate / 100,
          "The song ends with the note, its release short");
    check(peak(all.last(rate / 2000)) < 500, "The release fades out");
}

// Middle C, two octaves apart: the block, not the frequency number, changes.
void octaves_double_the_pitch()
{
    const auto samples = opengold::render_music(bank({{note(48, 64), note(72, 64)}}), 0, rate);
    const std::span<const std::int16_t> all(samples);
    const auto low = frequency(all.subspan(sample_at_tick(5), rate / 3));
    const auto high = frequency(all.subspan(sample_at_tick(69), rate / 3));
    check(std::abs(high / low - 4) < 0.01, "Two octaves up is four times the pitch");
}

// The FmSynth envelope: a quick attack, a decay to the sustain level 12 dB
// down (a quarter of the amplitude), held while the key is down, and a fast
// release to silence.
void the_envelope_attacks_decays_holds_and_releases()
{
    opengold::FmSynth chip(rate);
    chip.write(0x40, 0x3F); // Modulator silent.
    chip.write(0x23, 0x21); // Carrier: sustained, multiple 1.
    chip.write(0x43, 0x00); // Full level.
    chip.write(0x63, 0xA3); // Attack 10, decay 3.
    chip.write(0x83, 0x4F); // Sustain 12 dB, release 15.
    chip.write(0xA0, 0x57);
    chip.write(0xB0, 0x31); // Key on, block 4.
    std::vector<std::int16_t> samples;
    for (unsigned index = 0; index < rate * 3 / 2; ++index)
    {
        samples.push_back(static_cast<std::int16_t>(chip.next_sample() * 30000));
    }
    const std::span<const std::int16_t> all(samples);
    const auto window = rate / 100;
    const auto start = peak(all.first(rate / 2000));
    const auto top = peak(all.subspan(rate / 200, window / 2));
    const auto held = peak(all.subspan(rate, window));
    check(start < top / 2, "The attack rises over its first milliseconds");
    check(top > 28000, "It reaches full level");
    check(std::abs(held / top - 0.251) < 0.02, "It decays to 12 dB down and holds there");
    chip.write(0xB0, 0x11); // Key off.
    for (unsigned index = 0; index < rate / 20; ++index)
    {
        (void)chip.next_sample();
    }
    check(chip.silent(), "The release fades it out");
}

// A tied note goes on without striking again; a note before the same pitch
// is released a tick early so that the next one strikes anew.
void ties_hold_and_repeats_restrike()
{
    const auto tied = opengold::render_music(bank({{note(60, 40, true), note(60, 40)}}), 0, rate);
    const auto struck = opengold::render_music(bank({{note(60, 40), note(60, 40)}}), 0, rate);
    const auto gap =
        std::span<const std::int16_t>(struck).subspan(sample_at_tick(39.2), sample_at_tick(0.6));
    const auto held =
        std::span<const std::int16_t>(tied).subspan(sample_at_tick(39.2), sample_at_tick(0.6));
    check(peak(gap) < 200, "A repeated pitch is released for its last tick");
    check(rms(held) > 4000, "A tied note sounds on");
}

// The song ends with its longest track and plays once.
void the_song_ends_with_its_longest_track()
{
    const auto samples =
        opengold::render_music(bank({{note(60, 50)}, {note(64, 90)}, {note(67, 70)}}), 0, rate);
    const auto end = sample_at_tick(90);
    check(samples.size() >= end && samples.size() < end + samples_per_tick + rate / 100,
          "Three tracks end with the longest, then stop");
    check(peak(samples) < 32767, "Three voices at full level do not clip");
    check(opengold::render_music(bank({{note(60, 50)}}), 1, rate).empty(),
          "A song past the bank's songs is silent");
}

std::uint32_t longest_track(const opengold::MusicSong &song)
{
    std::uint32_t longest = 0;
    for (std::size_t track = 1; track < song.tracks.size(); ++track)
    {
        longest = std::max(longest, song.tracks[track].length_ticks);
    }
    return longest;
}

void real_bank_renders(const std::filesystem::path &folder, GoldBoxGame game, unsigned piece)
{
    const auto name = folder.filename().string() + " piece " + std::to_string(piece);
    const auto bank = opengold::load_music_bank(folder, game, piece);
    const auto samples = opengold::render_music(bank, 0, rate);
    const auto end = sample_at_tick(longest_track(bank.songs.at(0)));
    check(samples.size() >= end && samples.size() <= end + rate * 3 + samples_per_tick * 2,
          name + " lasts as long as its tracks");
    const auto loudness = rms(samples);
    check(peak(samples) < 32767, name + " does not clip");
    check(loudness > 500 && loudness < 12000, name + " is at a sane level");
}

void real_banks_render()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    if (!games || !std::filesystem::is_directory(games))
    {
        std::cout << "OPENGOLD_GOLDBOX_DIR not set; the real banks are not checked\n";
        return;
    }
    const std::filesystem::path root(games);
    if (std::filesystem::exists(root / "SECRET"))
    {
        real_bank_renders(root / "SECRET", GoldBoxGame::secret_of_the_silver_blades, 2);
    }
    if (std::filesystem::exists(root / "GATEWAY"))
    {
        real_bank_renders(root / "GATEWAY", GoldBoxGame::gateway_to_the_savage_frontier, 1);
    }
    if (std::filesystem::exists(root / "DARKNESS"))
    {
        for (unsigned piece = 1; piece <= 8; ++piece)
        {
            real_bank_renders(root / "DARKNESS", GoldBoxGame::pools_of_darkness, piece);
        }
        // The opening film's song, from its own file.
        const auto film_song = opengold::por::SoundBank::load_song(
            root / "DARKNESS" / "P1.SMS", GoldBoxGame::pools_of_darkness, 0);
        check(film_song.clips().size() == 1 && film_song.at(1).audible &&
                  film_song.at(1).pcm.samples().size() >
                      10u * opengold::por::PcmBuffer::sample_rate(),
              "P1.SMS's song 0 plays for more than ten seconds");
    }
    if (std::filesystem::exists(root / "TREASURE"))
    {
        const auto treasure = root / "TREASURE";
        real_bank_renders(treasure, GoldBoxGame::treasures_of_the_savage_frontier, 1);
        const auto music = opengold::por::SoundBank::load_music(
            treasure, GoldBoxGame::treasures_of_the_savage_frontier, 1);
        check(music.clips().size() == 1 && music.at(1).audible,
              "Treasures' title music is one clip, named by its piece");
        check(opengold::test::throws(
                  [&]
                  {
                      (void)opengold::load_music_bank(
                          treasure, GoldBoxGame::treasures_of_the_savage_frontier, 0);
                  }),
              "Record 0, the driver, is not music");
    }
}

} // namespace

int main()
{
    try
    {
        a_note_has_its_pitch_and_length();
        octaves_double_the_pitch();
        the_envelope_attacks_decays_holds_and_releases();
        ties_hold_and_repeats_restrike();
        the_song_ends_with_its_longest_track();
        real_banks_render();
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "music render tests passed\n";
    return 0;
}
