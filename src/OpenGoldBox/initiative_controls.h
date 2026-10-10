#ifndef OPENGOLDBOX_INITIATIVE_CONTROLS_H
#define OPENGOLDBOX_INITIATIVE_CONTROLS_H
#include "godot_nodes.h"
#include "localization.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>
#include <algorithm>

namespace presentation
{
template <class Text>
void setup_initiative(godot::Node &root, Text text, const godot::Callable &swap,
                      const godot::Callable &keep, const godot::Callable &metabolism,
                      const godot::Callable &input, const godot::Callable &select)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name("InitiativeChoice");
    set_dialog_window_size(*owned);
    owned->set_title(text(N_("Initiative")));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(root, std::move(owned));
    w->connect("close_requested", keep);
    w->connect("window_input", input);
    auto *who = add_control<Label>(*w, "ResolveLabel");
    who->set_text(text(N_("Resolve next")));
    auto *owners = add_control<OptionButton>(*w, "Resolve");
    owners->set_fit_to_longest_item(false);
    owners->connect("item_selected", select);
    auto *label = add_control<Label>(*w, "AllyLabel");
    label->set_text(text(N_("Ally")));
    auto *allies = add_control<OptionButton>(*w, "Ally");
    allies->set_fit_to_longest_item(false);
    allies->connect("item_selected", select);
    auto *description = add_control<Label>(*w, "Text");
    description->set("autowrap_mode", 3);
    auto *no = add_control<Button>(*w, "Keep");
    no->set_text(text(N_("Keep initiative")));
    no->connect("pressed", keep);
    auto *yes = add_control<Button>(*w, "Swap");
    yes->set_text(text(N_("Swap initiative")));
    yes->connect("pressed", swap);
    // A Monk's Uncanny Metabolism, used when Initiative is rolled.
    auto *restore = add_control<Button>(*w, "Metabolism");
    restore->set_text(text(N_("Uncanny Metabolism")));
    restore->connect("pressed", metabolism);
}

inline bool initiative_command(godot::Node &root, const opengold::rules::Command &command)
{
    if (command.verb != "initiative_keep" && command.verb != "initiative_swap" &&
            command.verb != "uncanny_metabolism")
        return true;
    const auto owner =
        required_node<godot::OptionButton>(root, "InitiativeChoice/Resolve").get_selected_id();
    const auto ally =
        required_node<godot::OptionButton>(root, "InitiativeChoice/Ally").get_selected_id();
    return command.actor == static_cast<unsigned>(owner) &&
           (command.verb != "initiative_swap" || command.target == static_cast<unsigned>(ally));
}

template <class Text, class Render>
void refresh_initiative(godot::Node &root, const opengold::rules::Snapshot &state,
                        const std::vector<opengold::rules::Command> &commands, Text text,
                        Render render)
{
    using namespace godot;
    auto *w = &required_node<Window>(root, "InitiativeChoice");
    if (state.initiative_choices.empty())
    {
        if (w->is_visible())
        {
            w->hide();
            required_node<Button>(root, "End").grab_focus();
        }
        return;
    }
    auto *owners = &required_node<OptionButton>(*w, "Resolve");
    auto *allies = &required_node<OptionButton>(*w, "Ally");
    const int prior = w->is_visible() ? owners->get_selected_id() : -1;
    const int prior_ally = w->is_visible() ? allies->get_selected_id() : -1;
    const auto label = [&](unsigned id)
    {
        const auto a = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto & c)
        {
            return c.id == id;
        });
        return render(opengold::rules::Message
        {
            N_("{name}: Initiative {total}"),
            {{"name", a->name}, {"total", std::to_string(a->initiative)}}});
    };
    SignalsBlocked owners_quiet(*owners);
    owners->clear();
    int selected = 0;
    for (auto id : state.initiative_choices)
    {
        owners->add_item(label(id), id);
        if (static_cast<int>(id) == prior)
            selected = owners->get_item_count() - 1;
    }
    owners->select(selected);
    owners_quiet.unblock();
    const auto owner = owners->get_selected_id();
    const bool multiple = state.initiative_choices.size() > 1;
    owners->set_visible(multiple);
    required_node<Label>(*w, "ResolveLabel").set_visible(multiple);
    SignalsBlocked allies_quiet(*allies);
    allies->clear();
    allies->add_item(text(N_("Choose an ally")), 0);
    selected = 0;
    for (const auto &c : commands)
        if (c.verb == "initiative_swap" && c.actor == static_cast<unsigned>(owner))
        {
            allies->add_item(label(c.target), c.target);
            if (owner == prior && static_cast<int>(c.target) == prior_ally)
                selected = allies->get_item_count() - 1;
        }
    allies->select(selected);
    allies_quiet.unblock();
    // Alert offers the swap; a Monk's Uncanny Metabolism its own button.
    const bool swaps = allies->get_item_count() > 1;
    const bool metabolism = std::any_of(commands.begin(), commands.end(), [&](const auto & c)
    {
        return c.verb == "uncanny_metabolism" && c.actor == static_cast<unsigned>(owner);
    });
    for (const char *name : {"AllyLabel", "Ally", "Swap"})
        required_node<Control>(*w, name).set_visible(swaps);
    required_node<Button>(*w, "Metabolism").set_visible(metabolism);
    required_node<Label>(*w, "Text").set_text(
        label(owner) + "\n\n" +
        (swaps ? text(N_("Swap these Initiative totals, or keep your Initiative. No turn has started yet."))
         : text(N_("Use Uncanny Metabolism to regain all Focus Points and 1d6 + your Monk level Hit Points (once per Long Rest), or keep going. No turn has started yet."))));
    required_node<Button>(*w, "Swap").set_disabled(selected == 0);
    if (!w->is_visible())
    {
        w->popup_centered();
        (multiple ? static_cast<Control *>(owners) : static_cast<Control *>(allies))->grab_focus();
    }
}
} // namespace presentation
#endif
