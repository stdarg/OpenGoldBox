#include "opengold/speaker_audio.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace opengold::por;

namespace
{
using Bytes = std::vector<std::uint8_t>;

void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F action)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::runtime_error &)
    {
        rejected = true;
    }
    require(rejected, "Malformed sound input was accepted");
}

void word(Bytes &b, std::size_t at, unsigned value)
{
    b.at(at) = value & 255;
    b.at(at + 1) = (value >> 8) & 255;
}

void command(Bytes &b, unsigned op, unsigned field, unsigned value)
{
    b.push_back(static_cast<std::uint8_t>(op));
    b.push_back(static_cast<std::uint8_t>(field));
    b.push_back(value & 255);
    b.push_back((value >> 8) & 255);
}

void packing()
{
    // One literal run and one fill run, read backwards, produce 16 bytes.
    Bytes file(32 + 16 + 18);
    word(file, 0, 0x5a4d);
    word(file, 8, 2);
    word(file, 20, 18);
    word(file, 22, 1);
    const Bytes packed{1, 2, 3, 4, 4, 0, 0xb3, 7, 12, 0, 0xb0, 255, 255, 255, 255, 255};
    std::copy(packed.begin(), packed.end(), file.begin() + 32);
    word(file, 48 + 12, 1);
    word(file, 48 + 14, 1);
    word(file, 48 + 16, 0x4252);
    const auto result = unpack_sound_executable(file);
    require(result.size() == 16 && result[0] == 1 && result[3] == 4 && result[15] == 7,
            "EXEPACK runs");
    auto bad = file;
    bad[41] = 255;
    rejects(
        [&]
    {
        (void)unpack_sound_executable(bad);
    });
    bad = file;
    bad[42] = 0x90;
    rejects(
        [&]
    {
        (void)unpack_sound_executable(bad);
    });
    bad = file;
    word(bad, 8, 0xffff);
    rejects(
        [&]
    {
        (void)unpack_sound_executable(bad);
    });
    for (std::size_t i = 0; i < 32; ++i)
        rejects(
            [&]
    {
        (void)unpack_sound_executable(std::span(file).first(i));
    });
    Bytes plain(32 + 4);
    word(plain, 0, 0x5a4d);
    word(plain, 8, 2);
    plain[32] = 91;
    require(unpack_sound_executable(plain).front() == 91, "Uncompressed image");
}

void sequences()
{
    Bytes data(16);
    command(data, 255, 10, 3);
    command(data, 255, 4, 1000);
    command(data, 255, 6, 100);
    command(data, 255, 38, 3);
    const auto loop = static_cast<unsigned>(data.size());
    command(data, 255, 0, 2);
    command(data, 254, 38, loop);
    command(data, 255, 10, 0);
    command(data, 255, 0, 0);
    const auto ticks = decode_speaker_sequence(data, {16, 0, 0, 0});
    require(ticks.size() == 7, "Loop duration");
    require(ticks[1].divisor == 1100 && ticks[5].divisor == 1500, "Pitch sweep");
    require(ticks[1].enabled && !ticks.back().enabled, "Speaker gate/termination");
    auto bad = data;
    bad[17] = 49;
    rejects(
        [&]
    {
        (void)decode_speaker_sequence(bad, {16, 0, 0, 0});
    });
    bad = data;
    bad[16] = 0xfa;
    rejects(
        [&]
    {
        (void)decode_speaker_sequence(bad, {16, 0, 0, 0});
    });
    bad = data;
    command(bad, 254, 38, 65535);
    rejects(
        [&]
    {
        (void)decode_speaker_sequence(bad,
        {static_cast<std::uint16_t>(bad.size() - 4), 0, 0, 0});
    });
    bad = Bytes(16);
    command(bad, 254, 38, 16);
    rejects(
        [&]
    {
        (void)decode_speaker_sequence(bad, {16, 0, 0, 0});
    });
    bad = Bytes(16);
    command(bad, 255, 0, 65535);
    rejects(
        [&]
    {
        (void)decode_speaker_sequence(bad, {16, 0, 0, 0});
    });
    // Table bytes and depth are signed. The second tick samples table byte -64.
    data = Bytes(16);
    data[1] = 192;
    command(data, 255, 10, 3);
    command(data, 255, 4, 1000);
    command(data, 255, 28, 0);
    command(data, 255, 36, 32);
    command(data, 255, 32, 16);
    command(data, 255, 34, 1024);
    command(data, 255, 0, 3);
    command(data, 255, 0, 0);
    const auto modulation = decode_speaker_sequence(data, {16, 0, 0, 0});
    require(modulation[1].divisor == 744 && modulation[2].divisor == 1000,
            "Signed modulation/wrap");
}

void rendering()
{
    SoundEffect tone{2, "Synthetic", false, 5041, std::vector<SpeakerTick>(100, {1193, true})};
    const auto pcm = render_speaker_audio(tone);
    require(pcm.size() == 20280, "PIT duration to sample count");
    unsigned crossings = 0;
    for (std::size_t i = 1; i < pcm.size(); ++i)
        if (pcm[i - 1] < 0 && pcm[i] >= 0)
            ++crossings;
    require(crossings >= 421 && crossings <= 423, "Approximate 1 kHz tone frequency");
    tone.ticks.assign(3, {0, false});
    const auto silence = render_speaker_audio(tone);
    require(std::all_of(silence.begin(), silence.end(),
                        [](auto v)
    {
        return v == 0;
    }),
    "Silence renders zero PCM");
}

void installed()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR"); // Borrowed environment view.
    if (!directory || !*directory)
    {
        std::cout << "Installed sound check skipped (OPENGOLD_GAME_DIR unset)\n";
        return;
    }
    const auto effects = load_sound_effects(directory);
    require(effects.size() == 21, "Complete sound directory");
    unsigned audible = 0;
    for (const auto &effect : effects)
    {
        const auto pcm = render_speaker_audio(effect);
        const auto energy = std::any_of(pcm.begin(), pcm.end(),
                                        [](auto v)
        {
            return v != 0;
        });
        require(energy == effect.audible(), "Decoded gate agrees with PCM energy");
        require(effect.audible() == (effect.id >= 2 && effect.id <= 20),
                "Audible and silent entries");
        audible += energy;
        std::cout << effect.id << " " << effect.name << ": " << effect.ticks.size() << " ticks, "
                  << pcm.size() << " samples\n";
    }
    require(audible == 19, "All original PC effects render");
}
} // namespace

int main()
{
    try
    {
        packing();
        sequences();
        rendering();
        installed();
        std::cout << "Sound tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
