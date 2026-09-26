#include "opengold/sound_player.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace opengold::por;
namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <class Exception, class F> void rejects(F action)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const Exception &)
    {
        rejected = true;
    }
    require(rejected, "Expected exception was not raised");
}
SoundEffect tone(unsigned id)
{
    return {id, "Synthetic tone", false, 5041, {{1193, true}, {1193, true}, {0, false}}};
}
SoundBank bank()
{
    // Sparse IDs and reversed ordering catch accidental indexing by ID - 1.
    return SoundBank({tone(42), tone(7)});
}

// The test owns this observation state; the fake borrows it only while alive.
struct OutputState
{
    unsigned alive{}, destroyed{}, prepares{}, starts{}, stops{}, last_id{};
    double gain{};
    bool playing{}, fail_prepare{}, fail_play{};
    std::vector<unsigned> prepared_ids;
};
class FakeOutput final : public SoundOutput
{
  public:
    explicit FakeOutput(OutputState &state) : state_(state)
    {
        ++state_.alive;
    }
    ~FakeOutput() override
    {
        stop();
        --state_.alive;
        ++state_.destroyed;
    }
    void prepare(const SoundBank &sounds) override
    {
        ++state_.prepares;
        for (const auto &clip : sounds.clips())
            state_.prepared_ids.push_back(clip.id);
        if (state_.fail_prepare)
        {
            state_.playing = true;
            throw std::runtime_error("Simulated partial preparation failure");
        }
    }
    void play(unsigned id) override
    {
        state_.last_id = id;
        ++state_.starts;
        state_.playing = true;
        if (state_.fail_play)
            throw std::runtime_error("Simulated device failure after starting");
    }
    void stop() noexcept override
    {
        ++state_.stops;
        state_.playing = false;
    }
    void set_gain(double gain) noexcept override
    {
        state_.gain = gain;
    }
    bool is_playing() const noexcept override
    {
        return state_.playing;
    }

  private:
    OutputState &state_;
};

void pcm_contract()
{
    std::vector<std::int16_t> samples{-32768, -1, 0, 1, 32767};
    PcmBuffer pcm(samples);
    samples[0] = 17;
    require(pcm.samples()[0] == -32768, "PCM buffer owns its samples");
    require(pcm.sample_rate() == 48000 && pcm.channels() == 1, "PCM format metadata");
    require(pcm.little_endian_bytes() ==
                std::vector<std::uint8_t>{0, 128, 255, 255, 0, 0, 1, 0, 255, 127},
            "Signed PCM serialization at the device boundary");
    require(pcm.duration() == 5.0 / 48000, "PCM duration");
    PcmBuffer empty({});
    require(empty.duration() == 0 && empty.little_endian_bytes().empty(),
            "Empty PCM is well-defined");
}

void bank_contract()
{
    auto sounds = bank();
    require(sounds.clips().size() == 2 && sounds.clips()[0].id == 42,
            "Bank preserves presentation order");
    require(sounds.at(7).id == 7 && !sounds.at(7).pcm.samples().empty(), "Lookup uses ID");
    const auto *samples = sounds.at(7).pcm.samples().data(); // Borrowed from the bank.
    require(sounds.at(7).pcm.samples().data() == samples, "Lookup reuses prepared PCM");
    rejects<std::out_of_range>(
        [&]
        {
            (void)sounds.at(1);
        });
    rejects<std::invalid_argument>(
        []
        {
            (void)SoundBank({tone(0)});
        });
    rejects<std::invalid_argument>(
        []
        {
            (void)SoundBank({tone(3), tone(3)});
        });
    auto invalid = tone(1);
    invalid.ticks.clear();
    rejects<std::invalid_argument>(
        [&]
        {
            (void)SoundBank({invalid});
        });
    invalid = tone(1);
    invalid.tick_divisor = 0;
    rejects<std::runtime_error>(
        [&]
        {
            (void)SoundBank({tone(2), invalid});
        });
    require(SoundBank({}).clips().empty(), "Empty bank is usable without a game installation");
}

void playback_contract()
{
    OutputState state;
    {
        SoundPlayer player(bank(), std::make_unique<FakeOutput>(state));
        require(state.alive == 1 && state.prepares == 1 &&
                    state.prepared_ids == std::vector<unsigned>{42, 7},
                "Player owns output and prepares the bank once");
        require(!player.current_sound() && state.gain == 1, "Initial state");
        player.play(7);
        require(player.current_sound() == 7 && state.last_id == 7, "Playback by ID");
        const auto starts = state.starts, stops = state.stops;
        rejects<std::out_of_range>(
            [&]
            {
                player.play(999);
            });
        require(state.starts == starts && state.stops == stops && player.current_sound() == 7,
                "Invalid ID leaves current audio uninterrupted");
        player.play(7);
        require(state.starts == starts + 1 && state.stops == stops + 1, "Replay restarts output");
        player.play(42);
        require(player.current_sound() == 42 && state.prepares == 1, "Switch uses existing cache");
        state.playing = false; // Simulate device completion, without real time or hardware.
        require(!player.current_sound(), "Completion clears observable playback state");
        player.play(7);
        player.stop();
        require(!player.current_sound() && !state.playing, "Explicit stop");

        player.set_volume(.25);
        player.set_muted(true);
        require(player.muted() && player.volume() == .25 && state.gain == 0,
                "Mute preserves volume and is exact silence");
        player.set_volume(.6);
        require(state.gain == 0 && player.volume() == .6, "Volume change while muted");
        player.set_muted(false);
        require(state.gain == .6 && player.effective_gain() == .6, "Unmute restores latest volume");
        for (const auto invalid : {-0.1, 1.1, std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::quiet_NaN()})
        {
            rejects<std::invalid_argument>(
                [&]
                {
                    player.set_volume(invalid);
                });
            require(player.volume() == .6 && state.gain == .6,
                    "Rejected volume leaves output unchanged");
        }
        player.set_volume(0);
        require(state.gain == 0 && !player.muted(), "Zero volume independently silences output");
        state.fail_play = true;
        rejects<std::runtime_error>(
            [&]
            {
                player.play(42);
            });
        require(!state.playing && !player.current_sound(),
                "Device failure stops partially started playback");
        state.fail_play = false;
        player.play(7);
        require(state.playing, "Player can recover after device failure");
    }
    require(state.alive == 0 && state.destroyed == 1 && !state.playing,
            "RAII shutdown during playback");
}

void failed_construction()
{
    rejects<std::invalid_argument>(
        []
        {
            SoundPlayer player(bank(), nullptr);
        });
    OutputState state;
    state.fail_prepare = true;
    rejects<std::runtime_error>(
        [&]
        {
            SoundPlayer player(bank(), std::make_unique<FakeOutput>(state));
        });
    require(state.alive == 0 && state.destroyed == 1 && !state.playing,
            "Failed preparation releases owned output and stops partial playback");
}
} // namespace

static_assert(!std::is_copy_constructible_v<SoundPlayer>);
static_assert(!std::is_move_constructible_v<SoundPlayer>);
static_assert(std::has_virtual_destructor_v<SoundOutput>);

int main()
{
    try
    {
        pcm_contract();
        bank_contract();
        playback_contract();
        failed_construction();
        std::cout << "Sound bank and playback tests passed (no Godot or game assets required)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
