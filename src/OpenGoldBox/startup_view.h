#ifndef OPENGOLDBOX_STARTUP_VIEW_H
#define OPENGOLDBOX_STARTUP_VIEW_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <exception>

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
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods();

  private:
    unsigned screen_{};
    bool finishing_{};
    bool choosing_language_{};
    bool language_save_failed_{};
    bool choosing_path_{};
    bool save_pending_path_{};
    godot::String pending_path_;
    double fade_elapsed_{};
    void show_screen();
    void finish();
    void open_character_creation();
    void begin_startup();
    void accept_language();
    void activate_language(std::int64_t index);
    void preview_language(std::int64_t index);
    void close_language();
    void choose_language();
    void choose_game_path();
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
