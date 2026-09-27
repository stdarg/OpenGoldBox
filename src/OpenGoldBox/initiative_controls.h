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
                      const godot::Callable &keep, const godot::Callable &input,
                      const godot::Callable &select)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name("InitiativeChoice");
    owned->set_size(Vector2i(640, 360));
    owned->set_min_size(Vector2i(640, 360));
    owned->set_title(text(N_("Alert")));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(root, std::move(owned));
    w->connect("close_requested", keep);
    w->connect("window_input", input);
    auto *who = add_control<Label>(*w, "ResolveLabel", Rect2(24, 24, 130, 36));
    who->set_text(text(N_("Resolve next")));
    auto *owners = add_control<OptionButton>(*w, "Resolve", Rect2(164, 24, 452, 40));
    owners->set_fit_to_longest_item(false);
    owners->connect("item_selected", select);
    auto *label = add_control<Label>(*w, "AllyLabel", Rect2(24, 82, 130, 36));
    label->set_text(text(N_("Ally")));
    auto *allies = add_control<OptionButton>(*w, "Ally", Rect2(164, 82, 452, 40));
    allies->set_fit_to_longest_item(false);
    allies->connect("item_selected", select);
    auto *description = add_control<Label>(*w, "Text", Rect2(24, 140, 592, 138));
    description->set("autowrap_mode", 3);
    auto *no = add_control<Button>(*w, "Keep", Rect2(164, 296, 220, 40));
    no->set_text(text(N_("Keep initiative")));
    no->connect("pressed", keep);
    auto *yes = add_control<Button>(*w, "Swap", Rect2(396, 296, 220, 40));
    yes->set_text(text(N_("Swap initiative")));
    yes->connect("pressed", swap);
}

inline bool initiative_command(godot::Node &root, const opengold::rules::Command &command)
{
    if (command.verb != "initiative_keep" && command.verb != "initiative_swap")
        return true;
    const auto owner =
        root.get_node<godot::OptionButton>("InitiativeChoice/Resolve")->get_selected_id();
    const auto ally =
        root.get_node<godot::OptionButton>("InitiativeChoice/Ally")->get_selected_id();
    return command.actor == unsigned(owner) &&
           (command.verb == "initiative_keep" || command.target == unsigned(ally));
}

template <class Text, class Render>
void refresh_initiative(godot::Node &root, const opengold::rules::Snapshot &state,
                        const std::vector<opengold::rules::Command> &commands, Text text,
                        Render render)
{
    using namespace godot;
    auto *w = root.get_node<Window>("InitiativeChoice");
    if (state.initiative_choices.empty())
    {
        if (w->is_visible())
        {
            w->hide();
            root.get_node<Button>("End")->grab_focus();
        }
        return;
    }
    auto *owners = w->get_node<OptionButton>("Resolve");
    auto *allies = w->get_node<OptionButton>("Ally");
    const int prior = w->is_visible() ? owners->get_selected_id() : -1;
    const int prior_ally = w->is_visible() ? allies->get_selected_id() : -1;
    const auto label = [&](unsigned id)
    {
        const auto a = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto &c)
                                    {
                                        return c.id == id;
                                    });
        return render(opengold::rules::Message{
            N_("{name}: Initiative {total}"),
            {{"name", a->name}, {"total", std::to_string(a->initiative)}}});
    };
    owners->set_block_signals(true);
    owners->clear();
    int selected = 0;
    for (auto id : state.initiative_choices)
    {
        owners->add_item(label(id), id);
        if (int(id) == prior)
            selected = owners->get_item_count() - 1;
    }
    owners->select(selected);
    owners->set_block_signals(false);
    const auto owner = owners->get_selected_id();
    const bool multiple = state.initiative_choices.size() > 1;
    owners->set_visible(multiple);
    w->get_node<Label>("ResolveLabel")->set_visible(multiple);
    allies->set_block_signals(true);
    allies->clear();
    allies->add_item(text(N_("Choose an ally")), 0);
    selected = 0;
    for (const auto &c : commands)
        if (c.verb == "initiative_swap" && c.actor == unsigned(owner))
        {
            allies->add_item(label(c.target), c.target);
            if (owner == prior && int(c.target) == prior_ally)
                selected = allies->get_item_count() - 1;
        }
    allies->select(selected);
    allies->set_block_signals(false);
    w->get_node<Label>("Text")->set_text(
        label(owner) + "\n\n" +
        text(
            N_("Swap these Initiative totals, or keep your Initiative. No turn has started yet.")));
    w->get_node<Button>("Swap")->set_disabled(selected == 0);
    if (!w->is_visible())
    {
        w->popup_centered();
        (multiple ? static_cast<Control *>(owners) : static_cast<Control *>(allies))->grab_focus();
    }
}
} // namespace presentation
#endif
