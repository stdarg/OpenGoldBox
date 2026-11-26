#include "game_folder_questions.h"
#include "application_settings.h"
#include "game_lifecycle.h"
#include "godot_nodes.h"
#include "godot_keys.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "opengold/game_finder.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <algorithm>
#include <utility>

using namespace godot;
using presentation::required_node;

namespace
{
// The questions' heading, naming the game whose folder is asked for.
String game_heading(opengold::GoldBoxGame game)
{
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        return i18n::text("Pool of Radiance game files");
    }
    return i18n::format(N_("{game} game files"),
                        {{"game", i18n::text(opengold::game_title(game).data())}});
}
} // namespace

bool GameFolderQuestions::folder_chosen(const settings::Settings &settings,
                                        opengold::GoldBoxGame game)
{
    const auto located = settings.locate_game_folder(game);
    return located && located->source == opengold::GameFolderSource::chosen &&
           settings::validate_game_folder(settings.game_folder(game), game).usable;
}

void GameFolderQuestions::ask(Request request, Outcome ready, Outcome back)
{
    prepare();
    ready_ = std::move(ready);
    back_ = std::move(back);
    folders_needed_ = {request.game};
    design_needed_ = request.design;
    saved_design_kept_ = request.saved_design_kept;
    if (request.game != opengold::GoldBoxGame::pool_of_radiance &&
        !folder_chosen(application_settings(*this), opengold::GoldBoxGame::pool_of_radiance))
    {
        folders_needed_.push_back(opengold::GoldBoxGame::pool_of_radiance);
    }
    next_folder();
}

bool GameFolderQuestions::asking() const
{
    for (const char *question : {"PathDialog", "FoundFolder", "ChecksumWarning"})
    {
        if (required_node<Window>(*this, question).is_visible())
        {
            return true;
        }
    }
    return false;
}

// Translated and connected only when first asked: after the language is chosen.
void GameFolderQuestions::prepare()
{
    if (prepared_)
    {
        return;
    }
    prepared_ = true;
    i18n::prepare_ui(required_node<Window>(*this, "PathDialog"));
    i18n::prepare_ui(required_node<Window>(*this, "ChecksumWarning"));
    i18n::prepare_ui(required_node<Window>(*this, "FoundFolder"));
    auto *found = &required_node<Window>(*this, "FoundFolder"); // scene-owned
    connect_escape(*found);
    required_node<Button>(*found, "Use")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::use_found_folder));
    required_node<Button>(*found, "Choose")
        .connect("pressed",
                 presentation::guarded(this, &GameFolderQuestions::choose_another_folder));
    auto *dialog = &required_node<Window>(*this, "PathDialog"); // scene-owned
    connect_escape(*dialog);
    required_node<Button>(*dialog, "Cancel")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::close));
    required_node<Button>(*dialog, "Continue")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::accept_path));
    required_node<Button>(*dialog, "Browse")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::browse_path));
    required_node<LineEdit>(*dialog, "Path")
        .connect("text_submitted",
                 presentation::guarded(this, &GameFolderQuestions::submitted_path));
    required_node<LineEdit>(*dialog, "Path")
        .connect("text_changed", presentation::guarded(this, &GameFolderQuestions::path_edited));
    required_node<FileDialog>(*dialog, "BrowseDialog")
        .connect("dir_selected", presentation::guarded(this, &GameFolderQuestions::picked_path));
    auto *warning = &required_node<Window>(*this, "ChecksumWarning");
    connect_escape(*warning);
    required_node<Button>(*warning, "Quit")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::quit_game));
    required_node<Button>(*warning, "Continue")
        .connect("pressed", presentation::guarded(this, &GameFolderQuestions::continue_path));
}

// Escape or the window's X goes back, as the folder dialog's Cancel does.
void GameFolderQuestions::connect_escape(Window &dialog)
{
    dialog.connect("window_input", presentation::guarded(this, &GameFolderQuestions::dialog_input));
    dialog.connect("close_requested", presentation::guarded(this, &GameFolderQuestions::close));
}

void GameFolderQuestions::dialog_input(const Ref<InputEvent> &event)
{
    if (presentation::escape_pressed(event))
    {
        get_viewport()->set_input_as_handled();
        presentation::guarded(this, &GameFolderQuestions::close).call_deferred();
    }
}

// Asks for the next game's folder unless the player's choice still holds the
// game: a folder the search found to confirm, else the folder dialog; ready
// is called once every folder is known.
void GameFolderQuestions::next_folder()
{
    if (asking_design())
    {
        ask_design();
        return;
    }
    if (folders_needed_.empty())
    {
        const auto ready = std::move(ready_);
        back_ = nullptr;
        ready();
        return;
    }
    const auto game = folders_needed_.front();
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    const auto heading = game_heading(game);
    dialog->set_title(heading);
    required_node<Label>(*dialog, "Title").set_text(heading);
    required_node<Label>(*dialog, "Instructions")
        .set_text(i18n::text(
            N_("Choose the folder containing START.EXE and the original DAX game files.")));
    auto *found = &required_node<Window>(*this, "FoundFolder");
    found->set_title(heading);
    required_node<Label>(*found, "Title").set_text(heading);
    // A chosen folder that no longer holds the game's files is looked for
    // again, as when none was chosen (the user's decision, 2026-11-20).
    const auto &settings = application_settings(*this);
    const auto located = settings.locate_game_folder(game);
    pending_path_ = settings.game_folder(game);
    if (!located ||
        (game == opengold::GoldBoxGame::pool_of_radiance && settings::flag("--reset-game-path")))
    {
        show_path();
        return;
    }
    if (located->source == opengold::GameFolderSource::found)
    {
        confirm_found_folder();
        return;
    }
    check_path();
}

// Closing a question goes back to where it was asked from.
void GameFolderQuestions::close()
{
    required_node<Window>(*this, "PathDialog").hide();
    required_node<Window>(*this, "FoundFolder").hide();
    required_node<Window>(*this, "ChecksumWarning").hide();
    save_pending_path_ = false;
    folders_needed_.clear();
    design_needed_ = false;
    ready_ = nullptr;
    if (const auto back = std::move(back_))
    {
        back_ = nullptr;
        back();
    }
}

// The warning's Quit follows the game's graceful shutdown, as the window's X does.
void GameFolderQuestions::quit_game()
{
    get_tree()->get_root()->emit_signal("close_requested");
}

// A folder the search found is shown for the player to confirm; it is saved
// only once confirmed, so the player is asked once (the user's decision,
// 2026-11-20).
void GameFolderQuestions::confirm_found_folder()
{
    auto *found = &required_node<Window>(*this, "FoundFolder");
    required_node<Label>(*found, "Folder").set_text(pending_path_);
    found->popup_centered();
    required_node<Button>(*found, "Use").grab_focus();
}

void GameFolderQuestions::use_found_folder()
{
    required_node<Window>(*this, "FoundFolder").hide();
    save_pending_path_ = true;
    check_path();
}

// Declining the found folder opens the folder dialog for the player to choose one.
void GameFolderQuestions::choose_another_folder()
{
    required_node<Window>(*this, "FoundFolder").hide();
    pending_path_ = String();
    show_path();
}

// The design is asked each time Unlimited Adventures is chosen, the last one
// offered, as the player may have several; a check's --game=UA plays the
// saved one.
void GameFolderQuestions::ask_design()
{
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    const auto heading = i18n::text(N_("Unlimited Adventures design"));
    dialog->set_title(heading);
    required_node<Label>(*dialog, "Title").set_text(heading);
    required_node<Label>(*dialog, "Instructions")
        .set_text(i18n::text(N_(
            "Choose the folder of the design to play, which the construction kit names NAME.DSN.")));
    pending_path_ = application_settings(*this).design_folder();
    if (saved_design_kept_ && settings::validate_design_folder(pending_path_).usable)
    {
        check_path();
        return;
    }
    show_path();
}

void GameFolderQuestions::show_path(const String &message)
{
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    required_node<LineEdit>(*dialog, "Path").set_text(pending_path_);
    required_node<Label>(*dialog, "Status").set_text(message);
    dialog->popup_centered();
    required_node<LineEdit>(*dialog, "Path").grab_focus();
}

void GameFolderQuestions::browse_path()
{
    auto *browser = &required_node<FileDialog>(*this, "PathDialog/BrowseDialog");
    browser->set_current_dir(required_node<LineEdit>(*this, "PathDialog/Path").get_text());
    browser->popup_centered_ratio(.7f);
}

void GameFolderQuestions::picked_path(const String &directory)
{
    required_node<LineEdit>(*this, "PathDialog/Path").set_text(directory);
    // Programmatic text changes do not emit LineEdit's text_changed signal.
    path_edited(directory);
    required_node<Button>(*this, "PathDialog/Continue").grab_focus();
}

void GameFolderQuestions::path_edited(const String &)
{
    required_node<Label>(*this, "PathDialog/Status").set_text(String());
}

void GameFolderQuestions::submitted_path(const String &)
{
    accept_path();
}

void GameFolderQuestions::accept_path()
{
    pending_path_ = required_node<LineEdit>(*this, "PathDialog/Path").get_text().strip_edges();
    save_pending_path_ = true;
    check_path();
}

void GameFolderQuestions::check_path()
{
    const auto result =
        asking_design() ? settings::validate_design_folder(pending_path_)
                        : settings::validate_game_folder(pending_path_, folders_needed_.front());
    if (!result.usable)
    {
        String message = result.error.is_empty() ? i18n::text("Missing or unreadable game files:")
                                                 : i18n::text(result.error.utf8().get_data());
        for (int i = 0; i < std::min(static_cast<int>(result.missing.size()), 5); ++i)
        {
            message += " " + result.missing[i];
        }
        if (result.missing.size() > 5)
        {
            message += " ...";
        }
        show_path(message);
        return;
    }
    required_node<Window>(*this, "PathDialog").hide();
    if (!result.different.is_empty())
    {
        auto *warning = &required_node<Window>(*this, "ChecksumWarning");
        required_node<RichTextLabel>(*warning, "Files")
            .set_text(pending_path_ + "\n\n" + String("\n").join(result.different));
        warning->popup_centered();
        required_node<Button>(*warning, "Quit").grab_focus();
        return;
    }
    continue_path();
}

void GameFolderQuestions::continue_path()
{
    required_node<Window>(*this, "ChecksumWarning").hide();
    const bool design = asking_design();
    if (save_pending_path_ &&
        !(design ? application_settings(*this).save_design_folder(pending_path_)
                 : application_settings(*this).save_game_folder(folders_needed_.front(),
                                                                pending_path_)))
    {
        show_path(i18n::text(
            "Cannot save settings. Check that settings.cfg in the game's user data folder can be written and is valid."));
        return;
    }
    save_pending_path_ = false;
    get_viewport()->set_input_as_handled();
    if (design)
    {
        design_needed_ = false;
    }
    else
    {
        folders_needed_.erase(folders_needed_.begin());
    }
    next_folder();
}
