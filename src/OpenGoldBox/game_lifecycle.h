#ifndef OPENGOLDBOX_GAME_LIFECYCLE_H
#define OPENGOLDBOX_GAME_LIFECYCLE_H

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

    void _ready() override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods();

  private:
    bool quitting_{};
    void watch_window(godot::Node *node);
    void window_input(const godot::Ref<godot::InputEvent> &event, godot::Node *window);
    void shortcut(const godot::Ref<godot::InputEvent> &event, godot::Viewport &viewport);
    void request_quit();
};

#endif
