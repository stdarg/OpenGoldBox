#include "opengold/sound_bank.h"

#include "opengold/music_render.h"

#include <algorithm>
#include <fstream>
#include <iterator>
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
        {
            throw std::invalid_argument("Sound bank requires unique, nonzero sound IDs");
        }
        if (effect.ticks.empty())
        {
            throw std::invalid_argument("Sound bank entries require at least one tick");
        }
    }
    clips_.reserve(effects.size());
    for (auto &effect : effects)
    {
        PcmBuffer pcm(render_speaker_audio(effect));
        clips_.push_back(
            {effect.id, std::move(effect.name), effect.unused, effect.audible(), std::move(pcm)});
    }
}

SoundBank SoundBank::load(const std::filesystem::path &game_directory, GoldBoxGame game)
{
    return SoundBank(load_sound_effects(game_directory, game));
}

SoundBank SoundBank::load_music(const std::filesystem::path &game_directory, GoldBoxGame game,
                                unsigned piece)
{
    // The driver always starts a bank's first song.
    PcmBuffer pcm(
        render_music(load_music_bank(game_directory, game, piece), 0, PcmBuffer::sample_rate()));
    SoundBank bank(std::vector<SoundEffect>{});
    bank.clips_.push_back({piece, "Music " + std::to_string(piece), false, true, std::move(pcm)});
    return bank;
}

SoundBank SoundBank::load_song(const std::filesystem::path &song_file, GoldBoxGame game,
                               unsigned song)
{
    std::ifstream input(song_file, std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    const auto music = decode_music_bank(bytes, game);
    if (!input || !music)
    {
        throw std::runtime_error(song_file.string() + " is not the music driver's songs");
    }
    PcmBuffer pcm(render_music(*music, song, PcmBuffer::sample_rate()));
    SoundBank bank(std::vector<SoundEffect>{});
    bank.clips_.push_back({1, song_file.filename().string(), false, true, std::move(pcm)});
    return bank;
}

const SoundClip &SoundBank::at(unsigned id) const
{
    const auto found = std::find_if(clips_.begin(), clips_.end(),
                                    [id](const SoundClip &clip)
                                    {
                                        return clip.id == id;
                                    });
    if (found == clips_.end())
    {
        throw std::out_of_range("Unknown sound ID: " + std::to_string(id));
    }
    return *found;
}

} // namespace opengold::por
