#ifndef OPENGOLDBOX_INITIATIVE_CONTROLS_H
#define OPENGOLDBOX_INITIATIVE_CONTROLS_H
#include "godot_nodes.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/option_button.hpp>
#include <vector>

namespace presentation
{
void setup_initiative(godot::Node &root, const godot::Callable &swap, const godot::Callable &keep,
                      const godot::Callable &metabolism, const godot::Callable &input,
                      const godot::Callable &select);

inline bool initiative_command(godot::Node &root, const opengold::rules::Command &command)
{
    if (command.verb != "initiative_keep" && command.verb != "initiative_swap" &&
        command.verb != "uncanny_metabolism" && command.verb != "persistent_rage")
    {
        return true;
    }
    const auto owner =
        required_node<godot::OptionButton>(root, "InitiativeChoice/Resolve").get_selected_id();
    const auto ally =
        required_node<godot::OptionButton>(root, "InitiativeChoice/Ally").get_selected_id();
    return command.actor == static_cast<unsigned>(owner) &&
           (command.verb != "initiative_swap" || command.target == static_cast<unsigned>(ally));
}

void refresh_initiative(godot::Node &root, const opengold::rules::Snapshot &state,
                        const std::vector<opengold::rules::Command> &commands);
} // namespace presentation
#endif
