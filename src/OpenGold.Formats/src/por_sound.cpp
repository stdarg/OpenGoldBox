#include "opengold/por_sound.h"
#include "little_endian.h"

#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace opengold::por
{
namespace
{
constexpr std::size_t max_image = 1024 * 1024;
// About 4.6 minutes at the drivers' 237 ticks a second; Secret of the Silver
// Blades' title music is the longest song.
constexpr unsigned max_ticks = 65536;

[[noreturn]] void invalid(std::string_view reason)
{
    throw std::runtime_error("Pool of Radiance sound: " + std::string(reason));
}

std::uint8_t byte(std::span<const std::uint8_t> data, std::size_t p)
{
    if (p >= data.size())
    {
        invalid("truncated data or out-of-range pointer");
    }
    return data[p];
}

std::uint16_t word(std::span<const std::uint8_t> data, std::size_t p)
{
    if (p >= data.size() || data.size() - p < 2)
    {
        invalid("truncated data or out-of-range pointer");
    }
    return format::read_u16(data, p);
}

// Byte offsets are field identifiers in the sound data, not CPU registers.
enum class Field : unsigned
{
    delay = 0,
    position = 2,
    pitch = 4,
    sweep = 6,
    output = 8,
    gate = 10,
    gate_delta = 12,
    tempo = 14,
    transpose = 18,
    sustain = 20,
    envelope = 22,
    envelope_step = 24,
    envelope_ticks = 26,
    wave = 28,
    phase = 30,
    phase_step = 32,
    depth = 34,
    period = 36
};

struct Voice
{
    std::array<std::uint16_t, 24> fields{};

    std::uint16_t &operator[](Field field)
    {
        return at_offset(static_cast<unsigned>(field));
    }

    // A field the sound data names by its byte offset.
    std::uint16_t &at_offset(unsigned offset)
    {
        if (offset % 2 || offset >= 48)
        {
            invalid("invalid voice field");
        }
        return fields[offset / 2];
    }
};

// The synthesis fields a voice selection or a clear resets; its step, stream,
// tempo, sustain and loop counters remain.
void clear_voice(Voice &voice)
{
    for (const unsigned field :
         {4U, 6U, 8U, 10U, 12U, 16U, 18U, 22U, 24U, 26U, 28U, 30U, 32U, 34U, 36U})
    {
        voice.at_offset(field) = 0;
    }
}

// Note events, three bytes each, up to the one that ends the step: the voice
// (times 20h), the pitch in semitones (7Fh rests; bit 7 ends the step) and the
// step's length, which the selected voice takes. Only voice 0 takes a pitch,
// from the twelve dividers of the lowest octave, halved an octave up.
std::size_t play_notes(std::span<const std::uint8_t> data, std::array<Voice, 4> &voices,
                       Voice &selected, std::size_t p, std::uint16_t note_dividers)
{
    using enum Field;
    constexpr unsigned rest = 0x7f;
    for (unsigned budget = 0; budget < 4096; ++budget)
    {
        const auto voice = byte(data, p) >> 5;
        const auto pitch = byte(data, p + 1);
        const auto length = byte(data, p + 2);
        p += 3;
        const auto speed = static_cast<std::uint8_t>(selected[tempo]);
        selected[delay] = static_cast<std::uint16_t>(speed > 1 ? speed * length : length);
        if ((pitch & 0x7f) != rest && voice == 0)
        {
            if (!note_dividers)
            {
                invalid("a note in a driver without its note table");
            }
            auto &target = voices[0];
            target[delay] = selected[delay];
            target[sustain] = selected[delay];
            target[envelope_step] = 0;
            target[envelope_ticks] = 1;
            const auto semitones = static_cast<std::uint16_t>((pitch & 0x7f) + target[transpose]);
            const auto octave = semitones / 12U;
            const auto divider =
                octave < 16 ? word(data, note_dividers + 2U * (semitones % 12U)) >> octave : 0;
            target[Field::pitch] = static_cast<std::uint16_t>(divider);
            target[output] = static_cast<std::uint16_t>(divider);
        }
        if (pitch & 0x80)
        {
            return p;
        }
    }
    invalid("sound note budget exceeded");
}

// A voice's envelope when its stage ends: level entries (ticks FFFFh) set the
// level at once, and the next entry slides it for its ticks.
void step_envelope(std::span<const std::uint8_t> data, Voice &voice)
{
    using enum Field;
    for (unsigned budget = 0; budget < 256; ++budget)
    {
        const auto entry = static_cast<std::size_t>(voice[envelope]) + voice[envelope_step];
        const auto value = word(data, entry);
        const auto ticks = word(data, entry + 2);
        voice[envelope_step] = static_cast<std::uint16_t>(voice[envelope_step] + 4);
        if (ticks != 0xffff)
        {
            voice[gate_delta] = value;
            voice[envelope_ticks] = ticks;
            return;
        }
        voice[gate] = value;
        if (value == 0)
        {
            voice[gate_delta] = 0;
        }
    }
    invalid("sound envelope budget exceeded");
}

void commands(std::span<const std::uint8_t> data, std::array<Voice, 4> &voices, std::size_t source,
              std::size_t &return_position, std::uint16_t note_dividers)
{
    using enum Field;
    auto p = static_cast<std::size_t>(voices[source][position]);
    // A voice given a step but no stream, as a note's voice may be, reads nothing.
    if (p == 0)
    {
        return;
    }
    auto selected = source;
    for (unsigned budget = 0; budget < 4096; ++budget)
    {
        const auto op = byte(data, p++);
        if (op < 0xfa)
        {
            p = play_notes(data, voices, voices[selected], p - 1, note_dividers);
            voices[source][position] = voices[source][delay] ? static_cast<std::uint16_t>(p) : 0;
            return;
        }
        if (op == 0xfa)
        {
            clear_voice(voices[selected]);
        }
        else if (op == 0xff || op == 0xfe)
        {
            const auto field = byte(data, p++);
            const auto value = word(data, p);
            p += 2;
            auto &destination = voices[selected].at_offset(field);
            if (op == 0xfe)
            {
                if (destination == 0 || --destination != 0)
                {
                    p = value;
                }
            }
            else
            {
                destination = value;
                if (field == static_cast<unsigned>(delay))
                {
                    voices[source][position] =
                        voices[source][delay] ? static_cast<std::uint16_t>(p) : 0;
                    return;
                }
            }
        }
        else if (op == 0xfc)
        {
            const auto target = word(data, p);
            return_position = p + 2;
            p = target;
        }
        else if (op == 0xfb)
        {
            if (!return_position)
            {
                invalid("return without a sound subroutine");
            }
            p = return_position;
        }
        else if (op == 0xfd)
        {
            const auto offset = word(data, p);
            p += 2;
            if (offset % 48 || offset / 48 >= voices.size())
            {
                invalid("invalid voice selection");
            }
            selected = offset / 48;
            clear_voice(voices[selected]);
        }
    }
    invalid("sound command budget exceeded");
}
} // namespace

bool SoundEffect::audible() const noexcept
{
    return std::any_of(ticks.begin(), ticks.end(),
                       [](const auto &tick)
                       {
                           return tick.enabled;
                       });
}

std::vector<std::uint8_t> unpack_sound_executable(std::span<const std::uint8_t> file)
{
    if (file.size() < 32 || file.size() > max_image || word(file, 0) != 0x5a4d)
    {
        invalid("START.EXE is not a supported DOS executable");
    }
    const std::size_t header = word(file, 8) * 16U;
    if (header < 32 || header > file.size())
    {
        invalid("invalid DOS header size");
    }
    const auto packed = header + word(file, 22) * 16U;
    const bool exepack = word(file, 20) == 18 && packed <= file.size() &&
                         file.size() - packed >= 18 && word(file, packed + 16) == 0x4252;
    if (!exepack)
    {
        return {file.begin() + header, file.end()};
    }
    const std::size_t size = word(file, packed + 12) * 16U;
    if (!size || size > max_image || word(file, packed + 14) != 1)
    {
        invalid("unsupported EXEPACK layout");
    }
    std::vector<std::uint8_t> result(size);
    std::size_t input = packed, output = size;
    // EXEPACK pads the compressed stream to a paragraph with FF bytes.
    for (unsigned padding = 0; input > header && file[input - 1] == 255; ++padding)
    {
        if (padding >= 15)
        {
            invalid("invalid EXEPACK padding");
        }
        --input;
    }
    for (;;)
    {
        if (input - header < 3)
        {
            invalid("truncated EXEPACK command");
        }
        const auto op = file[--input];
        input -= 2;
        const std::size_t count = word(file, input);
        if (count > output)
        {
            invalid("EXEPACK output overflow");
        }
        output -= count;
        if ((op & 0xfe) == 0xb0)
        {
            if (input == header)
            {
                invalid("truncated EXEPACK fill");
            }
            std::fill_n(result.begin() + output, count, file[--input]);
        }
        else if ((op & 0xfe) == 0xb2)
        {
            if (count > input - header)
            {
                invalid("truncated EXEPACK literal");
            }
            input -= count;
            std::copy_n(file.begin() + input, count, result.begin() + output);
        }
        else
        {
            invalid("invalid EXEPACK command");
        }
        if (op & 1)
        {
            break;
        }
    }
    if (output != input - header)
    {
        invalid("EXEPACK size mismatch");
    }
    std::copy_n(file.begin() + header, output, result.begin());
    return result;
}

std::vector<SpeakerTick> decode_speaker_sequence(std::span<const std::uint8_t> data,
                                                 const std::array<std::uint16_t, 4> &entries,
                                                 std::uint16_t note_dividers)
{
    if (data.size() > 65536)
    {
        invalid("sound segment too large");
    }
    using enum Field;
    std::array<Voice, 4> voices{};
    for (std::size_t i = 0; i < voices.size(); ++i)
    {
        voices[i][delay] = entries[i] ? 1 : 0;
        voices[i][position] = entries[i];
    }
    std::vector<SpeakerTick> result;
    std::size_t return_position{};
    for (unsigned tick = 0; tick < max_ticks; ++tick)
    {
        bool active = false;
        SpeakerTick sound;
        bool chosen = false;
        for (std::size_t i = 0; i < voices.size(); ++i)
        {
            auto &v = voices[i];
            if (!v[delay])
            {
                continue;
            }
            active = true;
            v[gate] += v[gate_delta];
            v[pitch] += v[sweep];
            int modulation = 0;
            auto next = static_cast<std::uint16_t>(v[phase] + v[phase_step]);
            if (next)
            {
                if (!v[period])
                {
                    invalid("zero modulation period");
                }
                if (next >= v[period])
                {
                    next -= v[period];
                }
                if (next >= v[period] || next >= 32768)
                {
                    invalid("invalid modulation phase");
                }
                v[phase] = next;
                const auto sample = std::bit_cast<std::int8_t>(byte(data, v[wave] + next / 16));
                const auto product =
                    static_cast<std::int32_t>(sample) * 256 * std::bit_cast<std::int16_t>(v[depth]);
                modulation =
                    product >= 0
                        ? product / 65536
                        : -static_cast<int>((-static_cast<std::int64_t>(product) + 65535) / 65536);
            }
            v[output] = static_cast<std::uint16_t>(v[pitch] + modulation);
            // A note's sustain runs out into the envelope's release, its fifth entry.
            if (v[sustain] && --v[sustain] == 0)
            {
                v[envelope_step] = 16;
                v[envelope_ticks] = 1;
            }
            if (--v[delay] == 0)
            {
                commands(data, voices, i, return_position, note_dividers);
            }
            if (v[envelope_ticks] && --v[envelope_ticks] == 0)
            {
                step_envelope(data, v);
            }
            if (!chosen && v[delay] && v[gate])
            {
                chosen = true;
                sound = {v[output], (v[gate] & 3) == 3};
            }
        }
        if (!active)
        {
            return result;
        }
        result.push_back(sound);
    }
    invalid("sound duration limit exceeded");
}

std::vector<SoundEffect> load_sound_effects(const std::filesystem::path &directory)
{
    auto path = directory / "START.EXE";
    if (!std::filesystem::exists(path))
    {
        path = directory / "start.exe";
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        invalid("cannot open START.EXE; select your Pool of Radiance PC 1.3 folder");
    }
    const auto length = input.tellg();
    if (length <= 0 || length > static_cast<std::streamoff>(max_image))
    {
        invalid("invalid START.EXE size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input)
    {
        invalid("could not read START.EXE");
    }
    const auto image = unpack_sound_executable(bytes);
    std::uint64_t fingerprint = 14695981039346656037ULL;
    for (const auto value : image)
    {
        fingerprint ^= value;
        fingerprint *= 1099511628211ULL;
    }
    if (image.size() != 0x10bf0 || fingerprint != 0x4bd7cdf744ac2004ULL)
    {
        invalid("unsupported START.EXE revision; this demo requires the Steam PC 1.3 image");
    }
    constexpr std::array<std::string_view, 21> names{
        "Silence",          "Cast spell",       "Magic damage",
        "Magic effect",     "Reduced to 0 HP",  "Ranged attack launched",
        "Melee hit",        "Lightning bolt",   "Melee miss",
        "Footstep",         "Fireball",         "Ranged attack impact",
        "Unused effect 13", "Unused effect 14", "Unused effect 15",
        "Unused effect 16", "Unused effect 17", "Unused effect 18",
        "Unused effect 19", "Unused effect 20", "Instrument setup (silent)"};
    const auto segment = std::span(image).subspan(0x8ae0, 0x1378);
    const auto timer = word(image, 0x8ae0 + 0x13a6);
    std::vector<SoundEffect> effects;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        std::array<std::uint16_t, 4> entries{};
        for (std::size_t voice = 0; voice < 4; ++voice)
        {
            entries[voice] = word(segment, 0x55f + i * 8 + voice * 2);
        }
        effects.push_back({static_cast<unsigned>(i + 1), std::string(names[i]), i >= 12, timer,
                           decode_speaker_sequence(segment, entries)});
    }
    return effects;
}

namespace
{

// The sound effects of the driver Curse of the Azure Bonds and Secret of the
// Silver Blades link into START.EXE; their song tables hold songs songs.
std::vector<SoundEffect> decode_driver_sound_effects(std::span<const std::uint8_t> image,
                                                     std::size_t songs)
{
    // The driver opens with its tick divider (chain the BIOS handler every 13
    // ticks), the exit countdown and the coded error message.
    constexpr std::array<std::uint8_t, 10> driver_start{0x0d, 0x00, 0x00, 0x30, 0x00,
                                                        0x00, 0xd2, 0xf5, 0xee, 0xad};
    const auto found =
        std::search(image.begin(), image.end(), driver_start.begin(), driver_start.end());
    if (found == image.end())
    {
        invalid("no sound driver in START.EXE");
    }
    const auto driver = static_cast<std::size_t>(found - image.begin());
    // Its eight PC-speaker voice blocks follow 56h bytes in, the twelve note
    // dividers right after them, and its PC-speaker song table 332h bytes after
    // the blocks. Their addresses count from the start of the code segment
    // the driver is linked into, which ProgramSpeakerVoices names:
    // mov di, voice blocks ... mov ax, [si + song table].
    constexpr std::size_t voice_blocks_in_driver = 0x56, song_table_after_blocks = 0x332,
                          note_dividers_after_blocks = 8 * 48;
    std::size_t voice_blocks = 0, song_table = 0;
    for (std::size_t p = driver; p + 3 < image.size() && p < driver + 0x4000 && !song_table; ++p)
    {
        if (image[p] != 0xbf)
        {
            continue;
        }
        const auto blocks = word(image, p + 1);
        for (std::size_t q = p + 3; q + 4 <= image.size() && q < p + 16; ++q)
        {
            if (image[q] == 0x8b && image[q + 1] == 0x84 &&
                word(image, q + 2) == blocks + song_table_after_blocks)
            {
                voice_blocks = blocks;
                song_table = blocks + song_table_after_blocks;
                break;
            }
        }
    }
    if (!song_table || voice_blocks > driver + voice_blocks_in_driver)
    {
        invalid("no song table in the sound driver");
    }
    const auto segment = image.subspan(driver + voice_blocks_in_driver - voice_blocks);
    // InstallSoundTimer loads the timer's divisor: mov al,0B6h; out 43h,al; mov ax,divisor.
    constexpr std::array<std::uint8_t, 5> set_timer{0xb0, 0xb6, 0xe6, 0x43, 0xb8};
    const auto timer_code =
        std::search(segment.begin(), segment.end(), set_timer.begin(), set_timer.end());
    if (timer_code == segment.end())
    {
        invalid("no timer rate in the sound driver");
    }
    const auto timer =
        word(segment, static_cast<std::size_t>(timer_code - segment.begin()) + set_timer.size());
    // Sound command n plays song n - 1 (GDConsts in the original); commands 2
    // to 13 are the game's sounds.
    constexpr std::array<std::string_view, 21> names{
        "Silence",          "Spell bolt",       "Attack missed",
        "Attack hit",       "Creature down",    "Missile flight",
        "Melee hit",        "Lightning bolt",   "Melee miss",
        "Footstep",         "Fireball",         "Missile launch",
        "Title picture",    "Unused effect 14", "Unused effect 15",
        "Unused effect 16", "Unused effect 17", "Unused effect 18",
        "Unused effect 19", "Unused effect 20", "Instrument setup (silent)"};
    const auto note_dividers =
        static_cast<std::uint16_t>(voice_blocks + note_dividers_after_blocks);
    std::vector<SoundEffect> effects;
    for (std::size_t i = 0; i < std::min(songs, names.size()); ++i)
    {
        std::array<std::uint16_t, 4> entries{};
        for (std::size_t voice = 0; voice < entries.size(); ++voice)
        {
            entries[voice] = word(segment, song_table + i * 8 + voice * 2);
        }
        effects.push_back({static_cast<unsigned>(i + 1), std::string(names[i]), i >= 13, timer,
                           decode_speaker_sequence(segment, entries, note_dividers)});
    }
    return effects;
}

} // namespace

std::vector<SoundEffect> decode_curse_sound_effects(std::span<const std::uint8_t> image)
{
    return decode_driver_sound_effects(image, 21);
}

std::vector<SoundEffect> load_sound_effects(const std::filesystem::path &game_directory,
                                            GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return load_sound_effects(game_directory);
    case GoldBoxGame::curse_of_the_azure_bonds:
    // Secret of the Silver Blades links the same driver, with the same sounds
    // and no songs past the title's.
    case GoldBoxGame::secret_of_the_silver_blades:
    {
        auto path = game_directory / "START.EXE";
        if (!std::filesystem::exists(path))
        {
            path = game_directory / "start.exe";
        }
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            invalid("cannot open START.EXE");
        }
        const std::vector<std::uint8_t> file(std::istreambuf_iterator<char>(input), {});
        if (file.size() > max_image)
        {
            invalid("invalid START.EXE size");
        }
        return decode_driver_sound_effects(unpack_sound_executable(file),
                                           game == GoldBoxGame::secret_of_the_silver_blades ? 13
                                                                                            : 21);
    }
    default:
        invalid("the sounds of " + std::string(game_title(game)) + " are not decoded yet");
    }
}
} // namespace opengold::por
