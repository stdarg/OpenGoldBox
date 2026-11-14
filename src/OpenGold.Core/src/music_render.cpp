#include "opengold/music_render.h"

#include "opengold/fm_synth.h"
#include "opengold/formats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>

namespace opengold
{
namespace
{

// On the AdLib the driver plays tracks 1 to 3, one voice each; track 0 is
// the single voice the PC speaker plays.
constexpr std::size_t first_adlib_track = 1;
constexpr std::size_t adlib_voices = 3;
constexpr std::size_t adlib_instrument_size = 12;

constexpr std::uint8_t end_of_track = 0xFF;
constexpr std::uint8_t take_voice = 0x81;
constexpr std::uint8_t reset_velocity = 0x84;
constexpr std::uint8_t full_velocity = 0x7F;

// The driver's frequency numbers for C to B; a note's block is its octave
// less one, so note 60 is middle C.
constexpr std::array<std::uint16_t, 12> frequency_numbers{0x157, 0x16D, 0x181, 0x198, 0x1B1, 0x1CB,
                                                          0x1E6, 0x203, 0x222, 0x243, 0x266, 0x28A};
// What the driver writes to registers A0h and B0h to release a note: the key
// bit clear, with a fixed pitch that the fading note takes on.
constexpr std::uint8_t release_frequency_low = 0x4A;
constexpr std::uint8_t release_block_and_high = 0x0E;
// Register BDh as the driver keeps it: the rhythm section on, the shallow
// tremolo and vibrato.
constexpr std::uint8_t rhythm_register = 0x20;
constexpr std::uint8_t note_select = 0x40;

// The output's level: a single operator at full volume reaches a quarter of
// the 16-bit range, so the three voices cannot clip.
constexpr double output_gain = 32767.0 / 4;
// How long the last notes may fade after the song ends, and the level below
// which they no longer count once they stay there long enough.
constexpr double longest_tail_seconds = 3;
constexpr int quiet_level = 8;
constexpr double quiet_seconds = 0.05;

enum class VoiceState
{
    releasing_early, // Released a tick before a note of the same pitch.
    sounding,
    tied, // The next note goes on without striking again.
    silent
};

struct Voice
{
    const MusicTrack *track{};
    // The voice's number, and its channel on the chip: the driver hands
    // each voice the first free channel, as no instrument holds one.
    unsigned channel{};
    std::size_t next_event{};
    std::uint32_t due_tick{};
    bool playing{true};
    std::uint8_t velocity{};
    VoiceState state{VoiceState::silent};
    std::span<const std::uint8_t> instrument;
};

std::uint8_t modulator_offset(unsigned channel)
{
    return static_cast<std::uint8_t>(channel / 3 * 8 + channel % 3);
}

std::uint8_t carrier_offset(unsigned channel)
{
    return static_cast<std::uint8_t>(modulator_offset(channel) + 3);
}

// The music driver's sequencer and its AdLib routines, writing to the chip
// as they do.
class AdlibDriver
{
  public:
    AdlibDriver(const MusicBank &bank, const MusicSong &song, FmSynth &chip)
        : bank_(bank), chip_(chip)
    {
        chip_.write(0x01, 0);
        chip_.write(0xBD, rhythm_register);
        chip_.write(0x08, note_select);
        silence();
        for (std::size_t index = 0; index < adlib_voices; ++index)
        {
            const auto track = first_adlib_track + index;
            if (track >= song.tracks.size())
            {
                break;
            }
            Voice voice;
            voice.track = &song.tracks[track];
            voice.channel = static_cast<unsigned>(index);
            voice.velocity = song.velocity;
            voices_.push_back(voice);
        }
        finished_ = voices_.empty();
    }

    [[nodiscard]] bool finished() const noexcept
    {
        return finished_;
    }

    // Moves every voice that is due on to its next note or rest; when the
    // last track ends, silences the chip. The song does not start again.
    void tick(std::uint32_t now)
    {
        bool track_ended = false;
        for (auto &voice : voices_)
        {
            if (voice.playing && voice.due_tick <= now)
            {
                track_ended |= step(voice);
            }
        }
        const bool any_playing = std::ranges::any_of(voices_, &Voice::playing);
        if (track_ended && !any_playing)
        {
            silence();
            finished_ = true;
        }
    }

  private:
    void silence()
    {
        for (std::uint8_t channel = 0; channel < 9; ++channel)
        {
            chip_.write(static_cast<std::uint8_t>(0xB0 + channel), release_block_and_high);
        }
        chip_.write(0xBD, rhythm_register);
    }

    // Returns whether the track ended.
    [[nodiscard]] bool step(Voice &voice)
    {
        if (voice.state == VoiceState::releasing_early)
        {
            release(voice);
            voice.state = VoiceState::silent;
            ++voice.due_tick;
            return false;
        }
        const auto &events = voice.track->events;
        while (voice.next_event < events.size() && events[voice.next_event].control())
        {
            obey_control(voice, events[voice.next_event]);
            ++voice.next_event;
        }
        if (voice.next_event >= events.size())
        {
            voice.playing = false;
            silence_voice(voice);
            return true;
        }
        const auto &event = events[voice.next_event];
        voice.due_tick += event.length_ticks;
        if (event.code == music_rest)
        {
            silence_voice(voice);
        }
        else
        {
            play_note(voice, event);
        }
        ++voice.next_event;
        return false;
    }

    void obey_control(Voice &voice, const MusicEvent &event)
    {
        if (event.code == take_voice)
        {
            // The driver does not read the control's value: each voice
            // takes the AdLib instrument of its own number.
            silence_voice(voice);
            load_instrument(voice, voice.channel);
        }
        else if (event.code == reset_velocity)
        {
            voice.velocity = full_velocity;
        }
    }

    void play_note(Voice &voice, const MusicEvent &event)
    {
        if (voice.state != VoiceState::tied)
        {
            silence_voice(voice);
            strike(voice, event.code);
        }
        const auto &events = voice.track->events;
        const auto next_code =
            voice.next_event + 1 < events.size() ? events[voice.next_event + 1].code : end_of_track;
        if (event.tied())
        {
            voice.state = VoiceState::tied;
        }
        else if (next_code == event.code)
        {
            --voice.due_tick;
            voice.state = VoiceState::releasing_early;
        }
        else
        {
            voice.state = VoiceState::sounding;
        }
    }

    void silence_voice(Voice &voice)
    {
        if (voice.state != VoiceState::silent)
        {
            release(voice);
        }
        voice.state = VoiceState::silent;
    }

    void load_instrument(Voice &voice, std::size_t index)
    {
        if (index >= bank_.instruments.size() ||
            bank_.instruments[index].size() != adlib_instrument_size)
        {
            return;
        }
        // A byte the driver skips, the channel the instrument may be held
        // to (none in any bank), then registers 20h, 40h, 60h and 80h for
        // each operator, C0h, and a byte the driver skips.
        voice.instrument = bank_.instruments[index];
        const auto &bytes = voice.instrument;
        for (std::size_t op = 0; op < 2; ++op)
        {
            const auto offset =
                op == 0 ? modulator_offset(voice.channel) : carrier_offset(voice.channel);
            chip_.write(static_cast<std::uint8_t>(0x40 + offset), bytes[4 + op]);
            if (op == 0)
            {
                chip_.write(static_cast<std::uint8_t>(0xC0 + voice.channel), bytes[10]);
            }
            chip_.write(static_cast<std::uint8_t>(0x60 + offset), bytes[6 + op]);
            chip_.write(static_cast<std::uint8_t>(0x80 + offset), bytes[8 + op]);
            chip_.write(static_cast<std::uint8_t>(0x20 + offset), bytes[2 + op]);
        }
    }

    void release(const Voice &voice)
    {
        chip_.write(static_cast<std::uint8_t>(0xA0 + voice.channel), release_frequency_low);
        chip_.write(static_cast<std::uint8_t>(0xB0 + voice.channel), release_block_and_high);
    }

    // The carrier's level follows the velocity, the instrument's own level
    // standing for full velocity; then the key goes down at the note's pitch.
    void strike(const Voice &voice, std::uint8_t note)
    {
        release(voice);
        if (voice.instrument.empty())
        {
            return;
        }
        const auto level_register = voice.instrument[5];
        const int softest = 0x3F;
        const int loudness = softest - (level_register & 0x3F);
        const auto level = softest - ((loudness * voice.velocity) >> 7);
        chip_.write(static_cast<std::uint8_t>(0x40 + carrier_offset(voice.channel)),
                    static_cast<std::uint8_t>(level | (level_register & 0xC0)));
        const auto block = std::clamp(note / 12 - 1, 0, 7);
        const auto frequency = frequency_numbers[note % 12];
        chip_.write(static_cast<std::uint8_t>(0xA0 + voice.channel),
                    static_cast<std::uint8_t>(frequency & 0xFF));
        chip_.write(static_cast<std::uint8_t>(0xB0 + voice.channel),
                    static_cast<std::uint8_t>(0x20 | block << 2 | frequency >> 8));
    }

    const MusicBank &bank_;
    FmSynth &chip_;
    std::vector<Voice> voices_;
    bool finished_{};
};

std::int16_t to_pcm(double sample)
{
    return static_cast<std::int16_t>(
        std::clamp(std::lround(sample * output_gain), -32768L, 32767L));
}

std::vector<std::uint8_t> read_archive(const std::filesystem::path &directory,
                                       const std::string &name)
{
    auto path = directory / name;
    if (!std::filesystem::exists(path))
    {
        auto lower = name;
        std::ranges::transform(lower, lower.begin(),
                               [](unsigned char c)
                               {
                                   return static_cast<char>(std::tolower(c));
                               });
        path = directory / lower;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("cannot open " + name);
    }
    return {std::istreambuf_iterator<char>(input), {}};
}

} // namespace

MusicBank load_music_bank(const std::filesystem::path &game_directory, GoldBoxGame game,
                          unsigned piece)
{
    const std::string name =
        game == GoldBoxGame::secret_of_the_silver_blades ? "SOUND.DAX" : "MUSIC.DAX";
    const auto archive = decode_dax_archive(read_archive(game_directory, name));
    if (!archive)
    {
        throw std::runtime_error(name + " is not a readable archive");
    }
    const auto record = std::ranges::find(archive.records, piece, &DaxRecord::id);
    if (record == archive.records.end())
    {
        throw std::runtime_error(name + " has no record " + std::to_string(piece));
    }
    auto bank = decode_music_bank(record->bytes, game);
    if (!bank)
    {
        throw std::runtime_error(name + " record " + std::to_string(piece) + " is not music");
    }
    return std::move(*bank);
}

std::optional<unsigned> title_music_piece(GoldBoxGame game) noexcept
{
    switch (game)
    {
    case GoldBoxGame::secret_of_the_silver_blades:
        return 2;
    case GoldBoxGame::gateway_to_the_savage_frontier:
    case GoldBoxGame::pools_of_darkness:
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return 1;
    default:
        return std::nullopt;
    }
}

std::vector<std::int16_t> render_music(const MusicBank &bank, std::size_t song,
                                       unsigned sample_rate)
{
    if (song >= bank.songs.size())
    {
        return {};
    }
    FmSynth chip(sample_rate);
    AdlibDriver driver(bank, bank.songs[song], chip);
    const auto samples_per_tick = sample_rate / music_ticks_per_second;
    std::vector<std::int16_t> samples;
    for (std::uint32_t tick = 0; !driver.finished(); ++tick)
    {
        driver.tick(tick);
        const auto tick_end = static_cast<std::size_t>(std::lround((tick + 1) * samples_per_tick));
        while (samples.size() < tick_end)
        {
            samples.push_back(to_pcm(chip.next_sample()));
        }
    }
    // The last notes fade out until the chip falls silent, or stays below
    // hearing for a while, or the tail grows too long.
    const auto tail_end =
        samples.size() + static_cast<std::size_t>(longest_tail_seconds * sample_rate);
    const auto quiet_run = static_cast<std::size_t>(quiet_seconds * sample_rate);
    std::size_t quiet_samples = 0;
    while (!chip.silent() && samples.size() < tail_end && quiet_samples < quiet_run)
    {
        samples.push_back(to_pcm(chip.next_sample()));
        quiet_samples = std::abs(samples.back()) < quiet_level ? quiet_samples + 1 : 0;
    }
    return samples;
}

} // namespace opengold
