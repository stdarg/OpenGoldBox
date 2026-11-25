// The ending: the game's own closing film with its song, or a dropped-in
// video, over the view; and the town's music while the tour plays.
#include "rolf_tour_view.h"
#include "application_settings.h"
#include "godot_images.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "godot_sound_output.h"
#include "guarded_handlers.h"
#include "opengold/diagnostic_log.h"
#include <godot_cpp/classes/aspect_ratio_container.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/video_stream.hpp>
#include <godot_cpp/classes/video_stream_player.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace godot;
using namespace opengold::por;
using presentation::required_node;

void RolfTourView::play_ending_film()
{
    // The original's own closing film plays where it has one (Pools of
    // Darkness' FINAL, which its CONTROL.EXE plays after the win).
    if (play_closing_film())
    {
        return;
    }
    // Otherwise a film is dropped in as endings/<the game's folder>.ogv
    // (Theora) in the game's files; without one the closing words stand alone.
    const auto folder = opengold::game_folder(dax_session().game());
    const String path =
        "res://endings/" + String::utf8(folder.data(), static_cast<int>(folder.size())) + ".ogv";
    if (!ResourceLoader::get_singleton()->exists(path))
    {
        return;
    }
    const Ref<VideoStream> film = ResourceLoader::get_singleton()->load(path);
    if (film.is_null())
    {
        return;
    }
    auto owned_layer = presentation::make_node<CanvasLayer>();
    owned_layer->set_name("EndingFilm");
    owned_layer->set_layer(100);
    auto owned_screen = presentation::make_node<ColorRect>();
    owned_screen->set_color(Color(0, 0, 0));
    owned_screen->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    auto *screen = presentation::attach_child(*owned_layer, std::move(owned_screen));
    auto owned_frame = presentation::make_node<AspectRatioContainer>();
    owned_frame->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    auto *frame = presentation::attach_child(*screen, std::move(owned_frame));
    auto owned_player = presentation::make_node<VideoStreamPlayer>();
    owned_player->set_expand(true);
    owned_player->set_stream(film);
    auto *player = presentation::attach_child(*frame, std::move(owned_player));
    presentation::attach_child(*this, std::move(owned_layer));
    player->connect("finished", presentation::guarded(this, &RolfTourView::end_ending_film));
    player->play();
    const auto picture = player->get_video_texture();
    if (picture.is_valid() && picture->get_height() > 0)
    {
        frame->set_ratio(static_cast<float>(picture->get_width()) /
                         static_cast<float>(picture->get_height()));
    }
}

void RolfTourView::follow_music(const TourSnapshot &snapshot)
{
    if (snapshot.music_starts != played_music_starts_)
    {
        played_music_starts_ = snapshot.music_starts;
        if (snapshot.music && !checking_)
        {
            play_music(*snapshot.music);
        }
    }
    if (!snapshot.music && music_ && music_->current_sound())
    {
        music_->stop();
    }
}

// A piece of the game's MUSIC.DAX at the player's music level, from its start;
// a piece that cannot be read leaves the town silent.
void RolfTourView::play_music(unsigned piece)
{
    const auto game = settings::chosen_game();
    try
    {
        if (!music_ || music_piece_ != piece)
        {
            music_.reset();
            music_ = std::make_unique<SoundPlayer>(
                SoundBank::load_music(presentation::path_from_godot(settings::game_folder(game)),
                                      game, piece),
                std::make_unique<GodotSoundOutput>(
                    required_node<AudioStreamPlayer>(*this, "Music")));
            music_piece_ = piece;
            music_->set_volume(settings::sound_volume(settings::SoundKind::music));
        }
        music_->play(piece);
    }
    catch (const std::runtime_error &failure)
    {
        music_.reset();
        opengold::log_diagnostic(opengold::LogPriority::warning, "music", failure.what());
    }
}

// The game's closing film from the player's game, each screen for its time
// with its song at the player's music level; false when the game has none or
// it cannot be read.
bool RolfTourView::play_closing_film()
{
    const auto game = dax_session().game();
    const auto folder = presentation::path_from_godot(settings::game_folder(game));
    try
    {
        auto film = opengold::load_closing_film(folder, game);
        if (film.screens.empty())
        {
            return false;
        }
        closing_film_ = std::move(film);
    }
    catch (const std::runtime_error &failure)
    {
        opengold::log_diagnostic(opengold::LogPriority::warning, "ending", failure.what());
        return false;
    }
    // The game has ended, and its music with it, before the film plays.
    music_.reset();
    auto owned_layer = presentation::make_node<CanvasLayer>();
    owned_layer->set_name("EndingFilm");
    owned_layer->set_layer(100);
    auto owned_screen = presentation::make_node<ColorRect>();
    owned_screen->set_name("Screen");
    owned_screen->set_color(Color(0, 0, 0));
    owned_screen->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    auto *screen = presentation::attach_child(*owned_layer, std::move(owned_screen));
    auto picture = presentation::make_node<TextureRect>();
    picture->set_name("Picture");
    picture->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    picture->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    picture->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    picture->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
    presentation::attach_child(*screen, std::move(picture));
    auto owned_song_output = presentation::make_node<AudioStreamPlayer>();
    owned_song_output->set_name("Song");
    auto *song_output = presentation::attach_child(*owned_layer, std::move(owned_song_output));
    presentation::attach_child(*this, std::move(owned_layer));
    closing_screen_ = 0;
    show_closing_screen();
    if (const auto &song = closing_film_->film_song)
    {
        try
        {
            constexpr unsigned song_clip = 1;
            closing_song_ =
                std::make_unique<SoundPlayer>(SoundBank::load_song(song->file, game, song->number),
                                              std::make_unique<GodotSoundOutput>(*song_output));
            closing_song_->set_volume(settings::sound_volume(settings::SoundKind::music));
            closing_song_->play(song_clip);
        }
        catch (const std::runtime_error &failure)
        {
            // The film plays silent when its song cannot be read.
            opengold::log_diagnostic(opengold::LogPriority::warning, "ending", failure.what());
        }
    }
    return true;
}

void RolfTourView::show_closing_screen()
{
    const auto &shown = closing_film_->screens.at(closing_screen_);
    required_node<TextureRect>(*this, "EndingFilm/Screen/Picture")
        .set_texture(presentation::image_texture(shown.screen));
    closing_seconds_left_ = shown.seconds;
}

// Each screen of the closing film stays its time; the film ends after its last.
void RolfTourView::advance_closing_film(double delta)
{
    if (!closing_film_ || !ending_film_playing())
    {
        return;
    }
    closing_seconds_left_ -= delta;
    if (closing_seconds_left_ > 0)
    {
        return;
    }
    if (++closing_screen_ >= closing_film_->screens.size())
    {
        end_ending_film();
        return;
    }
    show_closing_screen();
}

void RolfTourView::end_ending_film()
{
    closing_film_.reset();
    closing_song_.reset();
    // Freed at the frame's end, as the film's own finished signal may call this.
    if (auto *film = get_node_or_null("EndingFilm"))
    {
        film->queue_free();
    }
}

bool RolfTourView::ending_film_playing() const
{
    const auto *film = get_node_or_null("EndingFilm");
    return film && !film->is_queued_for_deletion();
}
