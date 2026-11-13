#ifndef OPENGOLDBOX_GAME_MENU_VIEW_H
#define OPENGOLDBOX_GAME_MENU_VIEW_H
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <cstdint>
#include <exception>

// The in-game menu Escape opens (Return to game, Save game, Load game,
// Options, Exit to main menu, Exit to desktop) and its Options screen. The
// host decides what the menu allows, pauses the game beneath it and answers
// its signals: save_requested(saving), main_menu_requested, closed,
// art_changed and new_art_changed (another new art folder was chosen).
class GameMenuView : public godot::Control
{
    GDCLASS(GameMenuView, godot::Control)
  public:
    struct Allowed
    {
        bool save{}, load{}, main_menu{};
        // The game plays by the SRD rules, whose balance settings (the
        // encounter challenge and the monster abilities) the Options show.
        bool srd_rules{true};
    };

    void _ready() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void open(Allowed allowed);
    // Only the Options screen, as the startup menu opens it; closing it closes the view.
    void open_options_alone();
    void close();
    [[nodiscard]] bool is_open() const;
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods();

  private:
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    void menu_save(bool saving);
    void exit_to_main_menu();
    void exit_to_desktop();
    void open_options();
    void show_srd_balance();
    bool options_alone_{};
    bool srd_rules_{true};
    void close_options();
    void show_section(std::int64_t section);
    void build_options();
    void difficulty_selected(std::int64_t index);
    void challenge_changed(double value);
    void ability_selected(std::int64_t index, std::int64_t row);
    void restore_ability_defaults();
    void language_selected(std::int64_t index);
    void zoom_changed(double value);
    void wall_art_selected(std::int64_t index);
    void combat_art_selected(std::int64_t index);
    void portrait_art_selected(std::int64_t index);
    // The new art download's folder: shown, chosen, and checked before it is
    // kept. It changes only while the menu may load a game, since changing it
    // loads the game again with the new art.
    void show_new_art_folder(const godot::String &problem = {});
    void choose_new_art_folder();
    void new_art_folder_chosen(const godot::String &folder);
    void auto_equip_selected(std::int64_t index);
    void unidentified_magic_selected(std::int64_t index);
    void keep_quick_selected(std::int64_t index);
    void original_shops_selected(std::int64_t index);
    // "Do not equip unidentified magic items" is offered only while auto-equip is on.
    void show_unidentified_magic(bool shown);
    bool may_change_art_folder_{};
};
#endif
