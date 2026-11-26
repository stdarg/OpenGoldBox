#ifndef OPENGOLDBOX_GAME_LIFECYCLE_H
#define OPENGOLDBOX_GAME_LIFECYCLE_H

#include "application_settings.h"
#include "game_log.h"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <exception>

class GameLifecycle : public godot::Node
{
    GDCLASS(GameLifecycle, godot::Node)
  public:
    // A failed handler is already in the log, and this view has no status
    // line to show it; called by presentation::run_guarded.
    void report_failure(const std::exception &) noexcept
    {
    }

    // The application's settings, which the views read and change: the
    // lifecycle outlives every scene.
    [[nodiscard]] settings::Settings &settings()
    {
        return settings_;
    }

    void _ready() override;
    void _exit_tree() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods();

  private:
    void start_lifecycle();
    void respond_to_input(const godot::Ref<godot::InputEvent> &event);
    bool quitting_{};
    void watch_window(godot::Node *node);
    // The game's log records each control the player uses (game_log.h).
    void watch_control(godot::Node *node);
    void log_pressed(godot::Node *control);
    void log_selected(int64_t index, godot::Node *control);
    void log_submitted(const godot::String &text, godot::Node *control);
    void window_input(const godot::Ref<godot::InputEvent> &event, godot::Node *node);
    void shortcut(const godot::Ref<godot::InputEvent> &event, godot::Viewport &viewport);
    void request_quit();

    settings::Settings settings_;
    GameLog log_;
};

// The lifecycle's settings (the ApplicationLifecycle autoload), for a node in
// the scene tree.
[[nodiscard]] settings::Settings &application_settings(const godot::Node &node);

#endif
