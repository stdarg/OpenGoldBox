#ifndef OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#define OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#include "training_control.h"
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <algorithm>
#include <span>
#include <vector>

namespace presentation
{
template <class Translate>
godot::Window *setup_training_replacement(godot::Node &parent, const godot::Callable &keep,
        const godot::Callable &apply, const Translate &tr)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name("RestTraining");
    owned->set_title(tr(N_("Weapon Mastery")));
    attach_dialog_layout(*owned);
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(parent, std::move(owned));
    w->connect("close_requested", keep);
    dialog_control<Label>(*w, "Title");
    dialog_control<Label>(*w, "Current");
    dialog_control<Label>(*w, "Limit");
    auto *scroll = dialog_control<ScrollContainer>(*w, "Choices");
    auto rows = instantiate_control<VBoxContainer>(
        "res://scenes/control_templates/training_replacement_rows.tscn");
    attach_child(*scroll, std::move(rows));
    dialog_control<Label>(*w, "Error");
    auto *cancel = dialog_control<Button>(*w, "Cancel");
    cancel->set_text(tr(N_("Keep current")));
    cancel->connect("pressed", keep);
    auto *confirm = dialog_control<Button>(*w, "Apply");
    confirm->set_text(tr(N_("Apply training")));
    confirm->connect("pressed", apply);
    return w;
}

// One section per choice the rest can change. Weapon Mastery checks several
// boxes; a single pick (the Hunter's Prey, the land) works like radio buttons.
template <class Translate>
void refresh_training_replacement(godot::Window &w,
                                  std::span<const opengold::rules::TrainingReplacementOptions> choices,
                                  std::span<const std::string> selected,
                                  const godot::Callable &toggled, const Translate &tr)
{
    using namespace godot;
    const auto chosen = [&](const std::string & id)
    {
        return std::find(selected.begin(), selected.end(), id) != selected.end();
    };
    String current = tr(N_("Current selections")) + ": ";
    for (const auto &choice : choices)
        for (const auto &id : choice.selected)
            for (const auto &option : choice.group.options)
                if (option.id == id)
                    current += tr(option.label) + "; ";
    required_node<Label>(w, "Current").set_text(current);
    auto &limit = required_node<Label>(w, "Limit");
    limit.hide();
    auto *rows = &required_node<VBoxContainer>(w, "Choices/Rows");
    // Rows that stay are never hidden on the way, which would drop their focus.
    std::vector<String> shown;
    for (const auto &choice : choices)
    {
        if (choices.size() > 1)
            shown.push_back("Heading_" + training_string(choice.group.id).replace(":", "_"));
        for (const auto &option : choice.group.options)
            shown.push_back(training_string(option.id));
    }
    for (int n = 0; n < rows->get_child_count(); ++n)
        if (auto *control = Object::cast_to<Control>(rows->get_child(n)))
            if (std::find(shown.begin(), shown.end(), String(control->get_name())) == shown.end())
                control->hide();
    int position = 0;
    for (const auto &choice : choices)
    {
        const auto &group = choice.group;
        const auto picks = static_cast<unsigned>(std::count_if(group.options.begin(),
                           group.options.end(), [&](const auto & o)
        {
            return chosen(o.id);
        }));
        if (group.count > 1)
        {
            limit.set_text(tr(N_("Selected")) + ": " + String::num_uint64(picks) + " / " +
                           String::num_uint64(group.count) + "    " + tr(N_("Replacement limit")) +
                           ": " + String::num_uint64(choice.replacement_limit));
            limit.show();
        }
        if (choices.size() > 1)
        {
            const auto name = "Heading_" + training_string(group.id).replace(":", "_");
            auto *heading = Object::cast_to<Label>(rows->get_node_or_null(name));
            if (!heading)
            {
                auto owned = make_node<Label>();
                owned->set_name(name);
                heading = attach_child(*rows, std::move(owned));
            }
            heading->set_text(tr(group.label));
            if (heading->get_index() != position)
                rows->move_child(heading, position);
            ++position;
            heading->show();
        }
        for (const auto &option : group.options)
        {
            const auto name = training_string(option.id);
            auto *box = Object::cast_to<CheckBox>(rows->get_node_or_null(name));
            if (!box)
            {
                auto owned = instantiate_control<CheckBox>(
                    "res://scenes/control_templates/training_replacement.tscn");
                owned->set_name(name);
                box = attach_child(*rows, std::move(owned));
                box->connect("toggled", toggled.bind(name));
            }
            const bool on = chosen(option.id);
            box->set_text(tr(option.label) + " / " + tr(option.description));
            box->set_pressed_no_signal(on);
            box->set_disabled(!on && group.count > 1 && picks >= group.count);
            if (box->get_index() != position)
                rows->move_child(box, position);
            ++position;
            box->show();
        }
    }
}
} // namespace presentation
#endif
