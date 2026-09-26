#include "godot_sound_output.h"

#include <godot_cpp/core/object.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace godot;
using namespace opengold::por;

GodotSoundOutput::GodotSoundOutput(AudioStreamPlayer &player) : player_id_(player.get_instance_id())
{
}

GodotSoundOutput::~GodotSoundOutput()
{
    stop();
    if (auto *node = player())
        node->set_stream(Ref<AudioStream>());
}

AudioStreamPlayer *GodotSoundOutput::player() const noexcept
{
    return Object::cast_to<AudioStreamPlayer>(ObjectDB::get_instance(player_id_));
}

void GodotSoundOutput::prepare(const SoundBank &bank)
{
    if (!player())
        throw std::runtime_error("Sound output node no longer exists");
    std::map<unsigned, Ref<AudioStreamWAV>> prepared;
    for (const auto &clip : bank.clips())
    {
        const auto pcm = clip.pcm.little_endian_bytes();
        PackedByteArray bytes;
        bytes.resize(static_cast<int64_t>(pcm.size()));
        std::copy(pcm.begin(), pcm.end(), bytes.ptrw());
        Ref<AudioStreamWAV> stream;
        stream.instantiate();
        stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
        stream->set_mix_rate(clip.pcm.sample_rate());
        stream->set_stereo(false);
        stream->set_data(bytes);
        prepared.emplace(clip.id, stream);
    }
    stop();
    auto *node = player();
    node->set_stream(Ref<AudioStream>());
    node->set_max_polyphony(1);
    streams_.swap(prepared);
}

void GodotSoundOutput::play(unsigned id)
{
    const auto stream = streams_.find(id);
    if (stream == streams_.end())
        throw std::out_of_range("Sound stream was not prepared");
    auto *node = player();
    if (!node || !node->is_inside_tree())
        throw std::runtime_error("Sound output node is not in the active scene");
    node->stop();
    node->set_stream(stream->second);
    node->play();
}

void GodotSoundOutput::stop() noexcept
{
    if (auto *node = player())
        node->stop();
}

void GodotSoundOutput::set_gain(double gain) noexcept
{
    // Defensive even for direct adapter callers. Zero is a true mute.
    if (auto *node = player())
        node->set_volume_linear(
            static_cast<float>(std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0));
}

bool GodotSoundOutput::is_playing() const noexcept
{
    const auto *node = player();
    return node && node->is_playing();
}
