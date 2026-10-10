#ifndef OPENGOLDBOX_OPTIONAL_EFFECT_CONTROLS_H
#define OPENGOLDBOX_OPTIONAL_EFFECT_CONTROLS_H
#include "godot_nodes.h"
#include "localization.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <algorithm>

namespace presentation
{
template <class Text>
void setup_optional_effect(godot::Node &root, Text text, const godot::Callable &use,
                           const godot::Callable &skip, const godot::Callable &input,
                           const godot::Callable &select)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name("OptionalEffect");
    owned->set_size(Vector2i(640, 300));
    owned->set_min_size(Vector2i(640, 300));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(root, std::move(owned));
    w->connect("close_requested", skip);
    w->connect("window_input", input);
    auto *caption = add_control<Label>(*w, "ResolveLabel", Rect2(24, 18, 130, 36));
    caption->set_text(text(N_("Resolve next")));
    caption->hide();
    auto *options = add_control<OptionButton>(*w, "Resolve", Rect2(164, 18, 452, 40));
    options->set_fit_to_longest_item(false);
    options->connect("item_selected", select);
    options->hide();
    auto *label = add_control<Label>(*w, "Text", Rect2(24, 18, 592, 194));
    label->set("autowrap_mode", 3);
    auto *no = add_control<Button>(*w, "Skip", Rect2(284, 236, 150, 40));
    no->set_text(text(N_("Skip")));
    no->connect("pressed", skip);
    auto *yes = add_control<Button>(*w, "Use", Rect2(446, 236, 170, 40));
    yes->set_text(text(N_("Use")));
    yes->connect("pressed", use);
}

template <class Render>
void refresh_optional_effect(godot::Node &root,
                             const std::optional<opengold::rules::OptionalEffectChoice> &choice,
                             bool player, Render render)
{
    using namespace godot;
    auto *w = root.get_node<Window>("OptionalEffect");
    if (!player || !choice)
    {
        if (w->is_visible())
        {
            w->hide();
            if (player)
                root.get_node<Button>("End")->grab_focus();
        }
        return;
    }
    auto *options = w->get_node<OptionButton>("Resolve");
    const int prior = w->is_visible() ? options->get_selected_id() : -1;
    options->set_block_signals(true);
    options->clear();
    int selected = 0;
    for (const auto &option : choice->options)
    {
        options->add_item(render(option.title), option.id);
        if (int(option.id) == prior)
            selected = options->get_item_count() - 1;
    }
    if (!choice->options.empty())
        options->select(selected);
    options->set_block_signals(false);
    const bool multiple = choice->options.size() > 1;
    options->set_visible(multiple);
    w->get_node<Label>("ResolveLabel")->set_visible(multiple);
    w->set_size(Vector2i(640, multiple ? 360 : 300));
    auto *label = w->get_node<Label>("Text");
    label->set_position(Vector2(24, multiple ? 76 : 18));
    label->set_size(Vector2(592, multiple ? 198 : 194));
    w->get_node<Button>("Use")->set_position(Vector2(446, multiple ? 296 : 236));
    w->get_node<Button>("Skip")->set_position(Vector2(284, multiple ? 296 : 236));
    const auto *option = choice->options.empty() ? nullptr : &choice->options[selected];
    w->set_title(render(option ? option->title : choice->title));
    label->set_text(render(option ? option->description : choice->description));
    w->get_node<Button>("Use")->set_disabled(option && !option->available);
    if (!w->is_visible())
    {
        w->popup_centered();
        w->get_node<Button>("Use")->grab_focus();
    }
}

inline unsigned optional_effect_item(godot::Node &root, const opengold::rules::Snapshot &state)
{
    if (state.effect_targeting)
        return 1;
    const auto *options = root.get_node<godot::OptionButton>("OptionalEffect/Resolve");
    return options->get_item_count() ? unsigned(options->get_selected_id()) : 0;
}

template <class Act, class Refresh>
bool effect_target_input(const godot::Ref<godot::InputEvent> &event,
                         const opengold::rules::Snapshot &state,
                         const std::vector<opengold::rules::Command> &commands, unsigned &selected,
                         Act act, Refresh refresh)
{
    using namespace godot;
    if (!state.effect_targeting)
        return false;
    const auto actor = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto & a)
    {
        return a.id == state.actor;
    });
    if (actor == state.combatants.end() || actor->side != 0)
        return false;
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
        return false;
    const auto code = key->get_keycode();
    if (code == Key::KEY_ESCAPE)
    {
        for (const auto &command : commands)
            if (command.verb == "effect_skip")
            {
                act(command);
                break;
            }
        return true;
    }
    std::vector<opengold::rules::Command> targets;
    for (const auto &command : commands)
        if (command.verb == state.effect_targeting->verb)
            targets.push_back(command);
    if (targets.empty())
        return false;
    selected %= targets.size();
    if (code == Key::KEY_LEFT || code == Key::KEY_UP || code == Key::KEY_RIGHT ||
            code == Key::KEY_DOWN)
    {
        selected = static_cast<unsigned>(
                       (selected + ((code == Key::KEY_RIGHT || code == Key::KEY_DOWN)
                                    ? 1
                                    : targets.size() - 1)) %
                       targets.size());
        refresh();
        return true;
    }
    if (code == Key::KEY_SPACE || code == Key::KEY_ENTER)
    {
        act(targets[selected]);
        return true;
    }
    return false;
}
} // namespace presentation
#endif
