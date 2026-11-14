#ifndef OPENGOLDBOX_STARTUP_VIEW_H
#define OPENGOLDBOX_STARTUP_VIEW_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/window.hpp>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "application_settings.h"
#include "opengold/gold_box_game.h"
#include "opengold/sound_player.h"
#include "opengold/title_screens.h"

class StartupView : public godot::Control
{
    GDCLASS(StartupView, godot::Control)
  public:
    // A failed handler is already in the log, and this view has no status
    // line to show it; called by presentation::run_guarded.
    void report_failure(const std::exception &) noexcept
    {
    }

    void _ready() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void _process(double delta) override;

  protected:
    static void _bind_methods();

  private:
    // Where startup stands: a title on screen, or its menus and dialogs.
    enum class Stage
    {
        opening_title,
        menus,
        new_title,
        original_title
    };
    // What startup goes on to once every folder it needs is known.
    enum class AfterFolders
    {
        game_title,
        saved_game,
        new_party
    };
    void open_startup();
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    void advance_title();
    void back_from_title();
    Stage stage_{Stage::menus};
    double title_seconds_left_{};
    bool choosing_language_{};
    bool language_save_failed_{};
    bool save_pending_path_{};
    // The scene is changing to the game; nothing in startup reopens.
    bool starting_{};
    godot::String pending_path_;
    // OpenGoldBox's own title: its lettering over the shared picture.
    void show_opening_title();
    void show_lettering(const char *name);
    // The chosen game's title: Pool of Radiance's new one, or the other games'
    // own pictures as their originals show them when they start.
    void show_game_title();
    void show_title_screen();
    void play_title_sound(unsigned sound);
    void play_title_music(opengold::GoldBoxGame game);
    void next_title_screen();
    void stop_game_title();
    void end_game_title();
    std::optional<opengold::TitleScreens> game_title_;
    std::size_t title_screen_{};
    std::unique_ptr<opengold::por::SoundPlayer> title_sound_;
    void open_character_creation();
    // The new game's first question: its rules, then under the original rules
    // its aging (docs/ORIGINAL-RULES.md). --rules=srd or --rules=original
    // answers it for a check.
    void ask_rules();
    void show_rules_notes(std::int64_t);
    void begin_game();
    void cancel_new_game();
    // Leaves startup for the game screen, which begins as start says.
    void start_game(settings::GameStart start);
    settings::GameStart::Kind new_game_kind_{settings::GameStart::Kind::new_party};
    void accept_language();
    void activate_language(std::int64_t index);
    void preview_language(std::int64_t index);
    void close_language();
    void choose_language();
    void language_chosen();
    void connect_path_dialogs();
    void show_backdrop();
    // Escape in a dialog window goes back, as the dialog's back does.
    void connect_escape(godot::Window &dialog, void (StartupView::*back)());
    void dialog_input(const godot::Ref<godot::InputEvent> &event, const godot::Callable &back);
    // The main menu: Continue, Pick game, Options and Exit to desktop (the
    // user's design, 2026-11-14).
    void show_main_menu();
    void continue_newest();
    void show_game_picker();
    void game_chosen(std::int64_t game);
    // The chosen game's menu after its title.
    void show_game_menu();
    void continue_game();
    void load_game();
    void saves_closed();
    void play_default_party();
    void create_new_party();
    void open_options_from_main();
    void open_options_from_game();
    void open_options();
    void options_closed();
    bool options_from_game_{};
    void exit_to_desktop();
    void choose_folders(opengold::GoldBoxGame game, AfterFolders after);
    void next_folder();
    void folders_ready();
    void cancel_folders();
    AfterFolders after_folders_{AfterFolders::game_title};
    std::filesystem::path save_to_load_;
    bool main_menu_ready_{}, game_picker_ready_{}, game_menu_ready_{};
    // The game came from --game=, for a check: like the original's STING,
    // it skips the titles and the menus.
    bool game_from_flag_{};
    // The games whose folders startup still needs, the first being asked now.
    std::vector<opengold::GoldBoxGame> folders_needed_;
    // An Unlimited Adventures game asks for the design it plays after the
    // folders (docs/games/ua.md, From the startup menu).
    bool design_needed_{};
    void ask_design();

    [[nodiscard]] bool asking_design() const
    {
        return folders_needed_.empty() && design_needed_;
    }

    void show_path(const godot::String &message = {});
    void browse_path();
    void picked_path(const godot::String &directory);
    void path_edited(const godot::String &directory);
    void submitted_path(const godot::String &directory);
    void accept_path();
    void check_path();
    void continue_path();
};

#endif
