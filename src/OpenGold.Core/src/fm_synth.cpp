#include "opengold/fm_synth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace opengold
{
namespace
{

constexpr double silent_db = 96;
constexpr double total_level_step_db = 0.75;
constexpr double sustain_level_step_db = 3;
// The datasheet's times for rate 1: an attack from silence to full, and a
// decay over the whole 96 dB. Each step of the rate halves them.
constexpr double slowest_attack_seconds = 2.82624;
constexpr double slowest_decay_seconds = 39.28064;
// The attack approaches full volume exponentially; it ends this close.
constexpr double attack_end_db = 0.05;
// The low-frequency oscillators run at the chip's rate over a fixed count.
constexpr double tremolo_hz = fm_chip_rate / 13440;
constexpr double vibrato_hz = fm_chip_rate / 8192;
constexpr double shallow_tremolo_db = 1, deep_tremolo_db = 4.8;
constexpr double shallow_vibrato_cents = 7, deep_vibrato_cents = 14;
// A full-scale modulator shifts the carrier's phase by four cycles.
constexpr double modulation_cycles = 4;

constexpr std::array<double, 16> frequency_multiples{0.5, 1, 2,  3,  4,  5,  6,  7,
                                                     8,   9, 10, 10, 12, 12, 15, 15};
// Key scaling's attenuation at 6 dB an octave for the top four bits of the
// frequency number, at block 8; each block lower takes 6 dB off.
constexpr std::array<double, 16> key_scale_db{0,  24,   30,    33.75, 36,    38.25, 39.75, 41.25,
                                              42, 43.5, 44.25, 45,    45.75, 46.5,  47.25, 48};
// Register 40h's two key-scale bits: none, 3, 1.5 or 6 dB an octave.
constexpr std::array<double, 4> key_scale_shares{0, 0.5, 0.25, 1};

constexpr std::size_t sine_steps = 4096;
constexpr double amplitude_steps_per_db = 16;

const std::vector<double> &sine_table()
{
    static const auto table = []
    {
        std::vector<double> values(sine_steps);
        for (std::size_t step = 0; step < sine_steps; ++step)
        {
            values[step] = std::sin(2 * std::numbers::pi * static_cast<double>(step) / sine_steps);
        }
        return values;
    }();
    return table;
}

const std::vector<double> &amplitude_table()
{
    static const auto table = []
    {
        std::vector<double> values(static_cast<std::size_t>(silent_db * amplitude_steps_per_db) +
                                   1);
        for (std::size_t step = 0; step < values.size(); ++step)
        {
            values[step] = std::pow(10.0, -static_cast<double>(step) / amplitude_steps_per_db / 20);
        }
        return values;
    }();
    return table;
}

// The sine of a phase given in cycles.
double sine(double cycles)
{
    const auto fraction = cycles - std::floor(cycles);
    return sine_table()[static_cast<std::size_t>(fraction * sine_steps) % sine_steps];
}

double amplitude(double attenuation_db)
{
    if (attenuation_db >= silent_db)
    {
        return 0;
    }
    return amplitude_table()[static_cast<std::size_t>(attenuation_db * amplitude_steps_per_db)];
}

// A triangle from -1 to 1 and back over one cycle.
double triangle(double cycles)
{
    return 1 - 4 * std::abs(cycles - 0.5);
}

// A rate register's value raised by the key scaling's offset, as the chip
// combines them: four steps a rate, at most 63.
unsigned effective_rate(std::uint8_t rate, unsigned rate_offset)
{
    return std::min(63U, 4U * rate + rate_offset);
}

// The time a rate takes for the given slowest time: halved for every rate
// step, the offset's quarter steps shortening it in between.
double rate_seconds(double slowest_seconds, unsigned rate)
{
    const auto step = static_cast<int>(rate / 4);
    const auto quarter = rate % 4;
    return slowest_seconds * std::exp2(1 - step) * 4 / (4 + quarter);
}

void wrap_phase(double &phase)
{
    phase -= std::floor(phase);
}

} // namespace

FmSynth::FmSynth(unsigned sample_rate) : sample_rate_(sample_rate)
{
}

void FmSynth::write(std::uint8_t address, std::uint8_t value)
{
    if (address == 0x08)
    {
        note_select_ = (value & 0x40) != 0;
        for (auto &channel : channels_)
        {
            update_rates(channel);
        }
        return;
    }
    if (address == 0xBD)
    {
        // The rhythm section's bits are not modeled (later-sound.md).
        deep_tremolo_ = (value & 0x80) != 0;
        deep_vibrato_ = (value & 0x40) != 0;
        return;
    }
    const auto group = static_cast<std::uint8_t>(address & 0xE0);
    if (group >= 0x20 && group <= 0x80)
    {
        const unsigned offset = address & 0x1F;
        if ((offset & 7) < 6 && offset < 22)
        {
            write_operator(offset, group, value);
        }
        return;
    }
    const auto channel_group = static_cast<std::uint8_t>(address & 0xF0);
    const unsigned channel_index = address & 0x0F;
    if (channel_group >= 0xA0 && channel_group <= 0xC0 && channel_index < channels_.size())
    {
        write_channel(channel_index, channel_group, value);
    }
    // Others, among them the waveform registers (E0h to F5h), which register
    // 01h keeps disabled on the AdLib's chip, are not modeled.
}

void FmSynth::write_operator(unsigned slot, std::uint8_t group, std::uint8_t value)
{
    // The chip numbers its operators in rows of eight, six used, each row
    // serving three channels: the modulators first, then the carriers.
    auto &channel = channels_[slot / 8 * 3 + slot % 8 % 3];
    auto &op = channel.operators[slot % 8 / 3];
    switch (group)
    {
    case 0x20:
        op.tremolo = (value & 0x80) != 0;
        op.vibrato = (value & 0x40) != 0;
        op.sustained = (value & 0x20) != 0;
        op.key_scaled_rates = (value & 0x10) != 0;
        op.multiple = value & 0x0F;
        break;
    case 0x40:
        op.key_scale_level = static_cast<std::uint8_t>(value >> 6);
        op.total_level = value & 0x3F;
        break;
    case 0x60:
        op.attack_rate = static_cast<std::uint8_t>(value >> 4);
        op.decay_rate = value & 0x0F;
        break;
    default: // 0x80
        op.sustain_level = static_cast<std::uint8_t>(value >> 4);
        op.release_rate = value & 0x0F;
        break;
    }
    update_rates(channel);
}

void FmSynth::write_channel(unsigned channel_index, std::uint8_t group, std::uint8_t value)
{
    auto &channel = channels_[channel_index];
    switch (group)
    {
    case 0xA0:
        channel.frequency_number =
            static_cast<std::uint16_t>((channel.frequency_number & 0x300) | value);
        break;
    case 0xB0:
        channel.frequency_number =
            static_cast<std::uint16_t>((channel.frequency_number & 0xFF) | ((value & 3) << 8));
        channel.block = static_cast<std::uint8_t>((value >> 2) & 7);
        key(channel, (value & 0x20) != 0);
        break;
    default: // 0xC0
        channel.feedback = static_cast<std::uint8_t>((value >> 1) & 7);
        channel.additive = (value & 1) != 0;
        break;
    }
    update_rates(channel);
}

// A key-on restarts both operators' envelopes and phases; a key-off releases them.
void FmSynth::key(Channel &channel, bool on)
{
    if (on == channel.keyed)
    {
        return;
    }
    channel.keyed = on;
    for (auto &op : channel.operators)
    {
        if (on)
        {
            op.stage = Stage::attack;
            op.phase = 0;
        }
        else if (op.stage != Stage::off)
        {
            op.stage = Stage::release;
        }
    }
}

// The envelope's speeds depend on the rates and, through key scaling, on the
// channel's pitch.
void FmSynth::update_rates(Channel &channel)
{
    const unsigned pitch_bit =
        note_select_ ? (channel.frequency_number >> 8) & 1 : (channel.frequency_number >> 9) & 1;
    const unsigned key_scale_number = channel.block * 2U + pitch_bit;
    for (auto &op : channel.operators)
    {
        const auto offset = op.key_scaled_rates ? key_scale_number : key_scale_number >> 2;
        const auto per_db = [&](std::uint8_t rate)
        {
            if (rate == 0)
            {
                return 0.0;
            }
            return silent_db / (rate_seconds(slowest_decay_seconds, effective_rate(rate, offset)) *
                                sample_rate_);
        };
        op.decay_db_per_sample = per_db(op.decay_rate);
        op.release_db_per_sample = per_db(op.release_rate);
        if (op.attack_rate == 0)
        {
            op.attack_factor = 1;
            continue;
        }
        const auto rate = effective_rate(op.attack_rate, offset);
        if (rate >= 60)
        {
            op.attack_factor = 0; // Instant.
            continue;
        }
        const auto seconds = rate_seconds(slowest_attack_seconds, rate);
        op.attack_factor =
            std::exp(-std::log(silent_db / attack_end_db) / (seconds * sample_rate_));
    }
}

void FmSynth::advance_envelope(Operator &op) const
{
    switch (op.stage)
    {
    case Stage::off:
    case Stage::sustain:
        return;
    case Stage::attack:
        op.envelope_db *= op.attack_factor;
        if (op.envelope_db <= attack_end_db)
        {
            op.envelope_db = 0;
            op.stage = Stage::decay;
        }
        return;
    case Stage::decay:
    {
        const auto sustain_db =
            op.sustain_level == 15 ? 93.0 : op.sustain_level * sustain_level_step_db;
        op.envelope_db += op.decay_db_per_sample;
        if (op.envelope_db >= sustain_db)
        {
            op.envelope_db = sustain_db;
            // A sustained sound holds there; any other keeps falling at
            // its release rate though the key is still down.
            op.stage = op.sustained ? Stage::sustain : Stage::release;
        }
        return;
    }
    case Stage::release:
        op.envelope_db += op.release_db_per_sample;
        if (op.envelope_db >= silent_db)
        {
            op.envelope_db = silent_db;
            op.stage = Stage::off;
        }
        return;
    }
}

double FmSynth::attenuation_db(const Channel &channel, const Operator &op) const
{
    auto db = op.envelope_db + op.total_level * total_level_step_db;
    const auto scaled = key_scale_db[channel.frequency_number >> 6] - 6.0 * (8 - channel.block);
    db += std::max(0.0, scaled) * key_scale_shares[op.key_scale_level];
    if (op.tremolo)
    {
        const auto depth = deep_tremolo_ ? deep_tremolo_db : shallow_tremolo_db;
        db += depth * (triangle(tremolo_phase_) + 1) / 2;
    }
    return db;
}

double FmSynth::channel_output(Channel &channel, double vibrato_factor)
{
    const auto base_hz = channel.frequency_number * std::exp2(channel.block) * fm_chip_rate /
                         static_cast<double>(1 << 20);
    for (auto &op : channel.operators)
    {
        advance_envelope(op);
        const auto hz =
            base_hz * frequency_multiples[op.multiple] * (op.vibrato ? vibrato_factor : 1);
        op.phase += hz / sample_rate_;
        wrap_phase(op.phase);
    }
    auto &modulator = channel.operators[0];
    auto &carrier = channel.operators[1];
    // The modulator feeds back the mean of its last two outputs, from
    // 1/32 of a cycle at feedback 1 to two cycles at 7.
    const auto feedback = channel.feedback == 0
                              ? 0.0
                              : (channel.modulator_history[0] + channel.modulator_history[1]) / 2 *
                                    std::exp2(channel.feedback - 6);
    const auto modulator_out =
        sine(modulator.phase + feedback) * amplitude(attenuation_db(channel, modulator));
    channel.modulator_history = {channel.modulator_history[1], modulator_out};
    const auto carrier_amplitude = amplitude(attenuation_db(channel, carrier));
    if (channel.additive)
    {
        return modulator_out + sine(carrier.phase) * carrier_amplitude;
    }
    return sine(carrier.phase + modulation_cycles * modulator_out) * carrier_amplitude;
}

double FmSynth::next_sample()
{
    tremolo_phase_ += tremolo_hz / sample_rate_;
    vibrato_phase_ += vibrato_hz / sample_rate_;
    wrap_phase(tremolo_phase_);
    wrap_phase(vibrato_phase_);
    const auto cents = deep_vibrato_ ? deep_vibrato_cents : shallow_vibrato_cents;
    const auto vibrato_factor = std::exp2(cents * triangle(vibrato_phase_) / 1200);
    double sum = 0;
    for (auto &channel : channels_)
    {
        sum += channel_output(channel, vibrato_factor);
    }
    return sum;
}

bool FmSynth::silent() const noexcept
{
    return std::ranges::all_of(channels_,
                               [](const Channel &channel)
                               {
                                   return std::ranges::all_of(channel.operators,
                                                              [](const Operator &op)
                                                              {
                                                                  return op.stage == Stage::off;
                                                              });
                               });
}

} // namespace opengold
