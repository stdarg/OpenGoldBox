#ifndef OPENGOLDBOX_OPTIONAL_EFFECT_CONTROLS_H
#define OPENGOLDBOX_OPTIONAL_EFFECT_CONTROLS_H
#include "godot_nodes.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <algorithm>
#include <optional>
#include <vector>

namespace presentation
{
void setup_optional_effect(godot::Node &root, const godot::Callable &use,
                           const godot::Callable &skip, const godot::Callable &input,
                           const godot::Callable &select);

void refresh_optional_effect(godot::Node &root,
                             const std::optional<opengold::rules::OptionalEffectChoice> &choice,
                             bool player);

inline unsigned optional_effect_item(godot::Node &root, const opengold::rules::Snapshot &state)
{
    if (state.effect_targeting)
    {
        return 1;
    }
    const auto *options = &required_node<godot::OptionButton>(root, "OptionalEffect/Resolve");
    return options->get_item_count() ? static_cast<unsigned>(options->get_selected_id()) : 0;
}

template <class Act, class Refresh>
bool effect_target_input(const godot::Ref<godot::InputEvent> &event,
                         const opengold::rules::Snapshot &state,
                         const std::vector<opengold::rules::Command> &commands, unsigned &selected,
                         Act act, Refresh refresh)
{
    using namespace godot;
    if (!state.effect_targeting)
    {
        return false;
    }
    const auto actor = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto &a)
                                    {
                                        return a.id == state.actor;
                                    });
    if (actor == state.combatants.end() || actor->side != opengold::rules::Side::party)
    {
        return false;
    }
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
    {
        return false;
    }
    const auto code = key->get_keycode();
    if (code == Key::KEY_ESCAPE)
    {
        for (const auto &command : commands)
        {
            if (command.verb == "effect_skip")
            {
                act(command);
                break;
            }
        }
        return true;
    }
    std::vector<opengold::rules::Command> targets;
    for (const auto &command : commands)
    {
        if (command.verb == state.effect_targeting->verb)
        {
            targets.push_back(command);
        }
    }
    if (targets.empty())
    {
        return false;
    }
    selected %= targets.size();
    if (code == Key::KEY_LEFT || code == Key::KEY_UP || code == Key::KEY_RIGHT ||
        code == Key::KEY_DOWN)
    {
        selected = static_cast<unsigned>(
            (selected +
             ((code == Key::KEY_RIGHT || code == Key::KEY_DOWN) ? 1 : targets.size() - 1)) %
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
