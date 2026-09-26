#include "opengold/por_sound.h"

#include <algorithm>
#include <bit>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace opengold::por
{
namespace
{
constexpr std::size_t max_image = 1024 * 1024;
constexpr unsigned max_ticks = 4096;
[[noreturn]] void invalid(std::string_view reason)
{
    throw std::runtime_error("Pool of Radiance sound: " + std::string(reason));
}
std::uint8_t byte(std::span<const std::uint8_t> data, std::size_t p)
{
    if (p >= data.size())
        invalid("truncated data or out-of-range pointer");
    return data[p];
}
std::uint16_t word(std::span<const std::uint8_t> data, std::size_t p)
{
    return static_cast<std::uint16_t>(byte(data, p) | (byte(data, p + 1) << 8));
}
// Byte offsets are field identifiers in the sound data, not CPU registers.
enum Field : unsigned
{
    delay = 0,
    position = 2,
    pitch = 4,
    sweep = 6,
    output = 8,
    gate = 10,
    gate_delta = 12,
    wave = 28,
    phase = 30,
    phase_step = 32,
    depth = 34,
    period = 36
};
struct Voice
{
    std::array<std::uint16_t, 24> fields{};
    std::uint16_t &operator[](unsigned offset)
    {
        if (offset % 2 || offset >= 48)
            invalid("invalid voice field");
        return fields[offset / 2];
    }
};
void commands(std::span<const std::uint8_t> data, std::array<Voice, 4> &voices, std::size_t source,
              std::size_t &return_position)
{
    auto p = static_cast<std::size_t>(voices[source][position]);
    auto selected = source;
    for (unsigned budget = 0; budget < 4096; ++budget)
    {
        const auto op = byte(data, p++);
        if (op == 0xff || op == 0xfe)
        {
            const auto field = byte(data, p++);
            const auto value = word(data, p);
            p += 2;
            auto &destination = voices[selected][field];
            if (op == 0xfe)
            {
                if (destination == 0 || --destination != 0)
                    p = value;
            }
            else
            {
                destination = value;
                if (field == delay)
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
                invalid("return without a sound subroutine");
            p = return_position;
        }
        else if (op == 0xfd)
        {
            const auto offset = word(data, p);
            p += 2;
            if (offset % 48 || offset / 48 >= voices.size())
                invalid("invalid voice selection");
            selected = offset / 48;
            // Synthesis fields reset; sequencing and loop counters remain intact.
            for (const unsigned field :
                 {4U, 6U, 8U, 10U, 12U, 16U, 18U, 22U, 24U, 26U, 28U, 30U, 32U, 34U, 36U})
                voices[selected][field] = 0;
        }
        else
        {
            invalid("unsupported PC sound sequence command");
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
        invalid("START.EXE is not a supported DOS executable");
    const std::size_t header = word(file, 8) * 16U;
    if (header < 32 || header > file.size())
        invalid("invalid DOS header size");
    const auto packed = header + word(file, 22) * 16U;
    const bool exepack = word(file, 20) == 18 && packed <= file.size() &&
                         file.size() - packed >= 18 && word(file, packed + 16) == 0x4252;
    if (!exepack)
        return {file.begin() + header, file.end()};
    const std::size_t size = word(file, packed + 12) * 16U;
    if (!size || size > max_image || word(file, packed + 14) != 1)
        invalid("unsupported EXEPACK layout");
    std::vector<std::uint8_t> result(size);
    std::size_t input = packed, output = size;
    // EXEPACK pads the compressed stream to a paragraph with FF bytes.
    for (unsigned padding = 0; input > header && file[input - 1] == 255; ++padding)
    {
        if (padding >= 15)
            invalid("invalid EXEPACK padding");
        --input;
    }
    for (;;)
    {
        if (input - header < 3)
            invalid("truncated EXEPACK command");
        const auto op = file[--input];
        input -= 2;
        const std::size_t count = word(file, input);
        if (count > output)
            invalid("EXEPACK output overflow");
        output -= count;
        if ((op & 0xfe) == 0xb0)
        {
            if (input == header)
                invalid("truncated EXEPACK fill");
            std::fill_n(result.begin() + output, count, file[--input]);
        }
        else if ((op & 0xfe) == 0xb2)
        {
            if (count > input - header)
                invalid("truncated EXEPACK literal");
            input -= count;
            std::copy_n(file.begin() + input, count, result.begin() + output);
        }
        else
            invalid("invalid EXEPACK command");
        if (op & 1)
            break;
    }
    if (output != input - header)
        invalid("EXEPACK size mismatch");
    std::copy_n(file.begin() + header, output, result.begin());
    return result;
}

std::vector<SpeakerTick> decode_speaker_sequence(std::span<const std::uint8_t> data,
                                                 const std::array<std::uint16_t, 4> &entries)
{
    if (data.size() > 65536)
        invalid("sound segment too large");
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
                continue;
            active = true;
            v[gate] += v[gate_delta];
            v[pitch] += v[sweep];
            int modulation = 0;
            auto next = static_cast<std::uint16_t>(v[phase] + v[phase_step]);
            if (next)
            {
                if (!v[period])
                    invalid("zero modulation period");
                if (next >= v[period])
                    next -= v[period];
                if (next >= v[period] || next >= 32768)
                    invalid("invalid modulation phase");
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
            if (--v[delay] == 0)
                commands(data, voices, i, return_position);
            if (!chosen && v[delay] && v[gate])
            {
                chosen = true;
                sound = {v[output], (v[gate] & 3) == 3};
            }
        }
        if (!active)
            return result;
        result.push_back(sound);
    }
    invalid("sound duration limit exceeded");
}

std::vector<SoundEffect> load_sound_effects(const std::filesystem::path &directory)
{
    auto path = directory / "START.EXE";
    if (!std::filesystem::exists(path))
        path = directory / "start.exe";
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        invalid("cannot open START.EXE; select your Pool of Radiance PC 1.3 folder");
    const auto length = input.tellg();
    if (length <= 0 || length > static_cast<std::streamoff>(max_image))
        invalid("invalid START.EXE size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input)
        invalid("could not read START.EXE");
    const auto image = unpack_sound_executable(bytes);
    std::uint64_t fingerprint = 14695981039346656037ULL;
    for (const auto value : image)
    {
        fingerprint ^= value;
        fingerprint *= 1099511628211ULL;
    }
    if (image.size() != 0x10bf0 || fingerprint != 0x4bd7cdf744ac2004ULL)
        invalid("unsupported START.EXE revision; this demo requires the Steam PC 1.3 image");
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
            entries[voice] = word(segment, 0x55f + i * 8 + voice * 2);
        effects.push_back({static_cast<unsigned>(i + 1), std::string(names[i]), i >= 12, timer,
                           decode_speaker_sequence(segment, entries)});
    }
    return effects;
}
} // namespace opengold::por
