#include "opengold/sound_player.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace opengold::por
{

SoundPlayer::SoundPlayer(SoundBank bank, std::unique_ptr<SoundOutput> output)
    : bank_(std::move(bank)), output_(std::move(output))
{
    if (!output_)
        throw std::invalid_argument("Sound player requires an audio output");
    output_->prepare(bank_);
    output_->set_gain(effective_gain());
}

SoundPlayer::~SoundPlayer()
{
    stop();
}

void SoundPlayer::play(unsigned id)
{
    (void)bank_.at(id);
    stop();
    try
    {
        output_->play(id);
    }
    catch (...)
    {
        // A device can fail after partially starting a stream.
        stop();
        throw;
    }
    current_ = id;
}

void SoundPlayer::stop() noexcept
{
    output_->stop();
    current_.reset();
}

std::optional<unsigned> SoundPlayer::current_sound() const noexcept
{
    return output_->is_playing() ? current_ : std::nullopt;
}

void SoundPlayer::set_volume(double volume)
{
    if (!std::isfinite(volume) || volume < 0 || volume > 1)
        throw std::invalid_argument("Sound volume must be finite and between 0 and 1");
    volume_ = volume;
    output_->set_gain(effective_gain());
}

void SoundPlayer::set_muted(bool muted) noexcept
{
    muted_ = muted;
    output_->set_gain(effective_gain());
}

} // namespace opengold::por
