#ifndef OPENGOLDBOX_GAME_MENU_VIEW_H
#define OPENGOLDBOX_GAME_MENU_VIEW_H
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <cstdint>
#include <exception>

// The in-game menu Escape opens (Return to game, Save game, Load game,
// Options, Exit to main menu, Exit to desktop) and its Options screen. The
// host decides what the menu allows, pauses the game beneath it and answers
// its signals: save_requested(saving), main_menu_requested, closed.
class GameMenuView : public godot::Control
{
    GDCLASS(GameMenuView, godot::Control)
  public:
    struct Allowed
    {
        bool save{}, load{}, main_menu{};
    };

    void _ready() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;
    void open(Allowed allowed);
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
    void close_options();
    void show_section(std::int64_t section);
    void build_options();
    void difficulty_selected(std::int64_t index);
    void challenge_changed(double value);
    void ability_selected(std::int64_t index, std::int64_t row);
    void restore_ability_defaults();
    void language_selected(std::int64_t index);
    void zoom_changed(double value);
};
#endif
