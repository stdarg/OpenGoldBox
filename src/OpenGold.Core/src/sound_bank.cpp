#include "opengold/sound_bank.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace opengold::por
{

SoundBank::SoundBank(std::vector<SoundEffect> effects)
{
    // Validate identities before spending time preparing any audio.
    std::set<unsigned> ids;
    for (const auto &effect : effects)
    {
        if (!effect.id || !ids.insert(effect.id).second)
            throw std::invalid_argument("Sound bank requires unique, nonzero sound IDs");
        if (effect.ticks.empty())
            throw std::invalid_argument("Sound bank entries require at least one tick");
    }
    clips_.reserve(effects.size());
    for (auto &effect : effects)
    {
        PcmBuffer pcm(render_speaker_audio(effect));
        clips_.push_back(
        {effect.id, std::move(effect.name), effect.unused, effect.audible(), std::move(pcm)});
    }
}

SoundBank SoundBank::load(const std::filesystem::path &game_directory)
{
    return SoundBank(load_sound_effects(game_directory));
}

const SoundClip &SoundBank::at(unsigned id) const
{
    const auto found = std::find_if(clips_.begin(), clips_.end(),
                                    [id](const SoundClip & clip)
    {
        return clip.id == id;
    });
    if (found == clips_.end())
        throw std::out_of_range("Unknown sound ID: " + std::to_string(id));
    return *found;
}

} // namespace opengold::por
