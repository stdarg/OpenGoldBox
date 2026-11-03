#include "game_lifecycle.h"
#include "application_settings.h"
#include "crash_reports.h"
#include "game_log.h"
#include "localization.h"
#include "screenshot_service.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include "opengold/diagnostic_log.h"
#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;
using presentation::required_node;

void GameLifecycle::_bind_methods()
{
}

void GameLifecycle::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  start_lifecycle();
                              });
}

void GameLifecycle::_exit_tree()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  crash_reports::end_session();
                              });
}

void GameLifecycle::start_lifecycle()
{
    crash_reports::install_terminate_handler();
    crash_reports::begin_session();
    game_log::open();
    if (settings::flag("--crash-check"))
    {
        crash_reports::crash_for_check();
    }
    i18n::initialize();
    set_process_mode(PROCESS_MODE_ALWAYS);
    auto *tree = get_tree(); // borrowed; SceneTree owns this autoload
    tree->set_auto_accept_quit(false);
    tree->get_root()->connect("close_requested",
                              presentation::guarded(this, &GameLifecycle::request_quit));
    watch_window(tree->get_root());
    tree->connect("node_added", presentation::guarded(this, &GameLifecycle::watch_window));
    tree->connect("node_added", presentation::guarded(this, &GameLifecycle::watch_control));
}

namespace
{
// "CampaignTown/Continue", where a control sits in the scene.
std::string control_name(Node &control)
{
    auto path = String(control.get_path());
    if (auto *scene = control.get_tree()->get_current_scene())
    {
        path = String(scene->get_path_to(&control));
    }
    return path.utf8().get_data();
}

void log_ui(const std::string &what)
{
    opengold::log_diagnostic(opengold::LogPriority::info, "ui", what);
}
} // namespace

void GameLifecycle::watch_control(Node *node)
{
    if (Object::cast_to<BaseButton>(node))
    {
        node->connect("pressed", presentation::guarded(this, &GameLifecycle::log_pressed).bind(node));
    }
    if (Object::cast_to<ItemList>(node) || Object::cast_to<OptionButton>(node))
    {
        node->connect("item_selected",
                      presentation::guarded(this, &GameLifecycle::log_selected).bind(node));
    }
    if (Object::cast_to<LineEdit>(node))
    {
        node->connect("text_submitted",
                      presentation::guarded(this, &GameLifecycle::log_submitted).bind(node));
    }
}

void GameLifecycle::log_pressed(Node *control)
{
    auto *button = Object::cast_to<BaseButton>(control);
    const auto *labelled = Object::cast_to<Button>(control);
    log_ui("pressed " + control_name(*control) +
           (labelled && !labelled->get_text().is_empty()
                ? " \"" + std::string(labelled->get_text().utf8().get_data()) + "\""
                : "") +
           (button->is_toggle_mode() ? (button->is_pressed() ? " (on)" : " (off)") : ""));
}

void GameLifecycle::log_selected(int64_t index, Node *control)
{
    String item;
    if (auto *list = Object::cast_to<ItemList>(control))
    {
        item = list->get_item_text(static_cast<int32_t>(index));
    }
    else if (auto *options = Object::cast_to<OptionButton>(control))
    {
        item = options->get_item_text(static_cast<int32_t>(index));
    }
    log_ui("selected " + control_name(*control) + " \"" + item.utf8().get_data() + "\"");
}

void GameLifecycle::log_submitted(const String &text, Node *control)
{
    log_ui("entered " + control_name(*control) + " \"" + text.utf8().get_data() + "\"");
}

void GameLifecycle::watch_window(Node *node)
{
    // Windows remain scene-owned. Signal connections are removed with their objects.
    if (auto *window = Object::cast_to<Window>(node))
    {
        window->connect("window_input",
                        presentation::guarded(this, &GameLifecycle::window_input).bind(window));
    }
}

void GameLifecycle::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_input(event);
                              });
}

void GameLifecycle::respond_to_input(const Ref<InputEvent> &event)
{
    shortcut(event, *get_viewport());
}

void GameLifecycle::window_input(const Ref<InputEvent> &event, Node *node)
{
    // The signal's emitting window owns the bound node for the connection's lifetime.
    if (auto *window = Object::cast_to<Window>(node))
    {
        shortcut(event, *window);
    }
}

void GameLifecycle::shortcut(const Ref<InputEvent> &event, Viewport &viewport)
{
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo() || !key->is_ctrl_pressed())
    {
        return;
    }
    if (key->get_keycode() != KEY_X && key->get_keycode() != KEY_S)
    {
        return;
    }
    viewport.set_input_as_handled();
    if (key->get_keycode() == KEY_X)
    {
        request_quit();
    }
    else
    {
        required_node<ScreenshotService>(*this, "/root/Screenshots").request_capture();
    }
}

void GameLifecycle::request_quit()
{
    if (quitting_)
    {
        return;
    }
    quitting_ = true;
    UtilityFunctions::print("OpenGoldBox: graceful shutdown requested\r");
    // Finish this iteration, then let Godot free the tree and native RAII owners.
    get_tree()->quit(0);
}
