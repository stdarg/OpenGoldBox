#ifndef OPENGOLD_FM_SYNTH_H
#define OPENGOLD_FM_SYNTH_H

#include <array>
#include <cstdint>

// An FM synthesizer in the manner of the AdLib card's chip, Yamaha's OPL2:
// nine channels of two operators each, driven by writes to the chip's
// registers. It is our own model of the chip's documented behaviour, not a
// copy of its circuit; docs/games/later-sound.md says what it covers.
namespace opengold
{

// The chip's own clock: 3.579545 MHz over 72 steps a sample.
inline constexpr double fm_chip_rate = 3579545.0 / 72.0;

class FmSynth
{
  public:
    explicit FmSynth(unsigned sample_rate);

    // Sets one of the chip's registers, as a program writes port 389h after
    // naming the register at port 388h.
    void write(std::uint8_t address, std::uint8_t value);

    // The next sample: the sum of the channels' outputs, each operator heard
    // within -1 to 1.
    [[nodiscard]] double next_sample();

    // Whether every operator has faded out.
    [[nodiscard]] bool silent() const noexcept;

  private:
    enum class Stage
    {
        off,
        attack,
        decay,
        sustain,
        release
    };

    struct Operator
    {
        // Register 20h.
        bool tremolo{};
        bool vibrato{};
        bool sustained{};
        bool key_scaled_rates{};
        std::uint8_t multiple{};
        // Register 40h.
        std::uint8_t key_scale_level{};
        std::uint8_t total_level{};
        // Registers 60h and 80h.
        std::uint8_t attack_rate{};
        std::uint8_t decay_rate{};
        std::uint8_t sustain_level{};
        std::uint8_t release_rate{};

        Stage stage{Stage::off};
        double envelope_db{96};
        double phase{}; // In cycles, 0 to 1.
        // Set from the rates and the channel's pitch by update_rates.
        double attack_factor{1};
        double decay_db_per_sample{};
        double release_db_per_sample{};
    };

    struct Channel
    {
        std::array<Operator, 2> operators; // The modulator, then the carrier.
        std::uint16_t frequency_number{};
        std::uint8_t block{};
        bool keyed{};
        std::uint8_t feedback{};
        bool additive{};
        std::array<double, 2> modulator_history{};
    };

    void write_operator(unsigned slot, std::uint8_t group, std::uint8_t value);
    void write_channel(unsigned channel_index, std::uint8_t group, std::uint8_t value);
    void key(Channel &channel, bool on);
    void update_rates(Channel &channel);
    void advance_envelope(Operator &op) const;
    [[nodiscard]] double attenuation_db(const Channel &channel, const Operator &op) const;
    [[nodiscard]] double channel_output(Channel &channel, double vibrato_factor);

    double sample_rate_;
    std::array<Channel, 9> channels_{};
    bool note_select_{};
    bool deep_tremolo_{};
    bool deep_vibrato_{};
    double tremolo_phase_{};
    double vibrato_phase_{};
};

} // namespace opengold

#endif
