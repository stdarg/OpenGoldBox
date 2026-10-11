#include "sound_board_view.h"
#include "godot_sound_output.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <filesystem>
#include <stdexcept>

using namespace godot;
using namespace opengold::por;

namespace
{
String gs(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int64_t>(value.size()));
}

String button_name(int index)
{
    return "Sound" + String::num_int64(index + 1);
}

} // namespace

void SoundBoardView::_bind_methods()
{
}

void SoundBoardView::_ready()
{
    get_window()->set_min_size(Vector2i(
        get_theme_constant("sound_board_min_width", "OpenGoldMetrics"),
        get_theme_constant("sound_board_min_height", "OpenGoldMetrics")));
    get_window()->set_title("OpenGoldBox - Pool of Radiance Sound Board");
    // Node lookups are borrowed from the scene tree; resources use Godot Ref RAII.
    for (int i = 0; i < 21; ++i)
    {
        auto *button = get_node<Button>(button_name(i));
        button->connect("pressed", callable_mp(this, &SoundBoardView::play).bind(i));
    }
    get_node<Button>("Stop")->connect("pressed", callable_mp(this, &SoundBoardView::stop));
    get_node<Button>("Mute")->connect("toggled", callable_mp(this, &SoundBoardView::mute));
    get_node<HSlider>("Volume")->connect("value_changed",
                                         callable_mp(this, &SoundBoardView::volume));
    refresh_buttons();
    volume(get_node<HSlider>("Volume")->get_value());
    if (Engine::get_singleton()->is_editor_hint())
        return;
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    checking_ = args.has("--sound-check");
    capture_ = args.has("--capture");
    try
    {
        auto directory = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
        if (directory.is_empty())
            directory =
                ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
        if (directory.is_empty())
            throw std::runtime_error(
                "Set OPENGOLD_GAME_DIR to your Pool of Radiance folder, then restart.");
        audio_ = std::make_unique<SoundPlayer>(
                     SoundBank::load(std::filesystem::path(directory.utf16().get_data())),
                     std::make_unique<GodotSoundOutput>(*get_node<AudioStreamPlayer>("Player")));
        for (std::size_t i = 0; i < audio_->bank().clips().size(); ++i)
        {
            const auto &clip = audio_->bank().clips()[i];
            const auto detail =
                clip.audible ? String::num(clip.pcm.duration(), 2) + " s" : "Silent control";
            get_node<Button>(button_name(static_cast<int>(i)))
            ->set_text(String::num_int64(clip.id).pad_zeros(2) + "   " + gs(clip.name) + "\n" +
                       detail);
        }
        volume(get_node<HSlider>("Volume")->get_value());
        mute(get_node<Button>("Mute")->is_pressed());
        loaded_ = true;
        refresh_buttons();
        get_node<Label>("Status")->set_text("Ready. Choose a sound to play.");
        get_node<Label>("Source")->set_tooltip_text(directory.path_join("START.EXE"));
    }
    catch (const std::exception &error)
    {
        get_node<Label>("Status")->set_text(gs(error.what()));
        if (checking_)
        {
            UtilityFunctions::printerr(gs(error.what()));
            get_tree()->quit(1);
        }
    }
}

void SoundBoardView::_exit_tree()
{
    // Release cached streams and stop the device when leaving this scene.
    audio_.reset();
    teardown_check_output_.reset();
    loaded_ = false;
    selected_ = -1;
}

void SoundBoardView::refresh_buttons()
{
    for (int i = 0; i < 21; ++i)
    {
        auto *button = get_node<Button>(button_name(i));
        button->set_disabled(!loaded_);
        button->set_theme_type_variation(i == selected_ ? "SoundBoardButtonSelected"
                                         : "SoundBoardButton");
    }
}

void SoundBoardView::play(int index)
{
    if (!loaded_ || index < 0 || static_cast<std::size_t>(index) >= audio_->bank().clips().size())
        return;
    const auto &clip = audio_->bank().clips()[index];
    try
    {
        audio_->play(clip.id);
        selected_ = index;
        refresh_buttons();
        get_node<Label>("Status")->set_text(
            clip.audible ? "Playing: " + gs(clip.name)
            : gs(clip.name) + " - this entry contains no audible effect.");
    }
    catch (const std::exception &error)
    {
        selected_ = -1;
        refresh_buttons();
        get_node<Label>("Status")->set_text(gs(error.what()));
    }
}

void SoundBoardView::stop()
{
    if (audio_)
        audio_->stop();
    selected_ = -1;
    refresh_buttons();
    if (loaded_)
        get_node<Label>("Status")->set_text("Stopped. Choose a sound to play.");
}

void SoundBoardView::finished()
{
    if (selected_ >= 0 && audio_->bank().clips()[selected_].audible)
        get_node<Label>("Status")->set_text(
            "Played: " + gs(audio_->bank().clips()[selected_].name) + ". Click to replay.");
    selected_ = -1;
    refresh_buttons();
}

void SoundBoardView::volume(double value)
{
    if (audio_)
        audio_->set_volume(value / 100);
    get_node<Label>("VolumeLabel")
    ->set_text("Volume " + String::num_int64(static_cast<int64_t>(value)) + "%");
}

void SoundBoardView::mute(bool value)
{
    get_node<Button>("Mute")->set_text(value ? "Unmute" : "Mute");
    if (audio_)
        audio_->set_muted(value);
}

void SoundBoardView::_process(double delta)
{
    ++frames_;
    if (check_finishing_)
    {
        // Give the audio mixer time to release stopped playback handles and
        // the scene tree time to free the queued node before checking shutdown.
        check_elapsed_ += delta;
        if (check_elapsed_ < .2)
            return;
        check_passed_ = check_passed_ && !teardown_check_output_->is_playing();
        teardown_check_output_->set_gain(0);
        teardown_check_output_->stop();
        bool rejected = false;
        try
        {
            teardown_check_output_->play(99);
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        check_passed_ = check_passed_ && rejected;
        teardown_check_output_.reset();
        UtilityFunctions::print(
            check_passed_
            ? "Sound board check passed: 21 buttons, completion, replay, switching, stop, volume, mute, RAII teardown, freed node"
            : "Sound board control check failed");
        check_finishing_ = false;
        checking_ = false;
        get_tree()->quit(check_passed_ ? 0 : 1);
        return;
    }
    if (audio_ && selected_ >= 0 && !audio_->current_sound())
        finished();
    if (capture_ && !captured_ && frames_ > 5)
    {
        const auto path = ProjectSettings::get_singleton()->globalize_path(
                              "res://../../user-data/sound-board.png");
        get_viewport()->get_texture()->get_image()->save_png(path);
        captured_ = true;
    }
    if (!checking_ || !loaded_ || frames_ < 8)
        return;
    check_elapsed_ += delta;
    auto *player = get_node<AudioStreamPlayer>("Player");
    if (check_elapsed_ > 20)
    {
        UtilityFunctions::printerr("Sound playback timed out");
        get_tree()->quit(1);
        checking_ = false;
        return;
    }
    if (player->is_playing())
        return;
    if (++check_index_ == 21)
    {
        get_node<HSlider>("Volume")->set_value(35);
        get_node<Button>("Mute")->set_pressed(true);
        const auto muted = player->get_volume_linear() == 0;
        get_node<Button>("Mute")->set_pressed(false);
        const auto restored = std::abs(player->get_volume_linear() - .35) < .0001;
        get_node<Button>("Sound2")->emit_signal("pressed");
        const auto first_stream = player->get_stream();
        get_node<Button>("Sound2")->emit_signal("pressed");
        const auto replayed = player->get_stream() == first_stream && audio_->current_sound() == 2;
        get_node<Button>("Sound3")->emit_signal("pressed");
        const auto switched = player->get_stream() != first_stream && audio_->current_sound() == 3;
        get_node<Button>("Stop")->emit_signal("pressed");
        const auto stopped = !player->is_playing() && !audio_->current_sound();
        get_node<Button>("Sound2")->emit_signal("pressed");
        audio_.reset(); // Destruction during playback must stop and detach the stream.
        const auto released = !player->is_playing() && player->get_stream().is_null();
        loaded_ = false;
        selected_ = -1;
        check_passed_ = muted && restored && replayed && switched && stopped && released;
        // This adapter must survive the scene-owned node being freed first.
        teardown_check_output_ = std::make_unique<GodotSoundOutput>(*player);
        teardown_check_output_->prepare(
        SoundBank({SoundEffect{99, "Lifecycle fixture", false, 5041, {{1000, true}}}}));
        player->queue_free();
        check_elapsed_ = 0;
        check_finishing_ = true;
        return;
    }
    check_elapsed_ = 0;
    get_node<Button>(button_name(check_index_))->emit_signal("pressed");
    const auto &clip = audio_->bank().clips()[check_index_];
    if (audio_->current_sound() != clip.id || player->get_stream().is_null() ||
            std::abs(player->get_stream()->get_length() - clip.pcm.duration()) > .0001)
    {
        UtilityFunctions::printerr("Sound button did not start its stream");
        checking_ = false;
        get_tree()->quit(1);
    }
}
