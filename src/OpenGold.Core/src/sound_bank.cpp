#include "opengold/sound_bank.h"

#include "opengold/gateway_formats.h"
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

namespace
{
// A digitized effect as mono signed PCM at the speaker's rate: its unsigned
// 8-bit samples centred and stepped through at their own rate.
std::vector<std::int16_t> resampled(const gateway::SoundSample &effect)
{
    const auto count =
        static_cast<std::size_t>(effect.samples.size() * PcmBuffer::sample_rate() / effect.rate);
    std::vector<std::int16_t> pcm;
    pcm.reserve(count);
    for (std::size_t n = 0; n < count; ++n)
    {
        const auto source =
            std::min(n * effect.rate / PcmBuffer::sample_rate(), effect.samples.size() - 1);
        constexpr int centre = 128, scale = 256;
        pcm.push_back(static_cast<std::int16_t>((effect.samples[source] - centre) * scale));
    }
    return pcm;
}
} // namespace

SoundBank SoundBank::load(const std::filesystem::path &game_directory, GoldBoxGame game)
{
    if (game != GoldBoxGame::pools_of_darkness &&
        game != GoldBoxGame::treasures_of_the_savage_frontier)
    {
        return SoundBank(load_sound_effects(game_directory, game));
    }
    // Pools of Darkness and Treasures load ZOUNDS.DAX's records 1 to 11 as
    // they start; their sound n plays record n - 1 (docs/games/later-sound.md).
    const auto archive = decode_dax_archive(read_game_archive(game_directory, "ZOUNDS.DAX"));
    if (!archive)
    {
        throw std::runtime_error("ZOUNDS.DAX is not a readable archive");
    }
    SoundBank bank(std::vector<SoundEffect>{});
    for (const auto &record : archive.records)
    {
        const auto effect = gateway::decode_later_sound_sample(record.id, record.bytes);
        if (!effect || effect->samples.empty() || effect->rate == 0)
        {
            continue;
        }
        const unsigned id = record.id + 1U;
        bank.clips_.push_back(
            {id, "Sound " + std::to_string(id), false, true, PcmBuffer(resampled(*effect))});
    }
    return bank;
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
