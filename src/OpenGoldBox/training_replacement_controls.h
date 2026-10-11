#ifndef OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#define OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#include "training_control.h"
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/button.hpp>

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
    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    scroll->set_follow_focus(true);
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

template <class Translate>
void refresh_training_replacement(godot::Window &w,
                                  const opengold::rules::TrainingReplacementOptions &options,
                                  std::span<const std::string> selected,
                                  const godot::Callable &toggled, const Translate &tr)
{
    using namespace godot;
    String current = tr(N_("Current selections")) + ": ";
    for (const auto &id : options.selected)
        for (const auto &option : options.group.options)
            if (option.id == id)
                current += tr(option.label) + "; ";
    required_node<Label>(w, "Current").set_text(current);
    required_node<Label>(w, "Limit").set_text(
        tr(N_("Selected")) + ": " + String::num_uint64(selected.size()) + " / " +
        String::num_uint64(options.group.count) + "    " + tr(N_("Replacement limit")) + ": " +
        String::num_uint64(options.replacement_limit));
    auto *rows = &required_node<VBoxContainer>(w, "Choices/Rows");
    for (int n = 0; n < rows->get_child_count(); ++n)
        if (auto *box = Object::cast_to<CheckBox>(rows->get_child(n)))
        {
            if (std::none_of(options.group.options.begin(), options.group.options.end(),
                             [&](const auto & o)
        {
            return training_string(o.id) == box->get_name();
            }))
            box->hide();
        }
    for (const auto &option : options.group.options)
    {
        const auto name = training_string(option.id);
        auto *box = Object::cast_to<CheckBox>(rows->get_node_or_null(name));
        if (!box)
        {
            auto owned = instantiate_control<CheckBox>(
                "res://scenes/control_templates/training_replacement.tscn");
            owned->set_name(name);
            style_choice(*owned);
            box = attach_child(*rows, std::move(owned));
            box->connect("toggled", toggled.bind(name));
        }
        const bool chosen =
            std::find(selected.begin(), selected.end(), option.id) != selected.end();
        box->set_text(tr(option.label) + " / " + tr(option.description));
        box->set_pressed_no_signal(chosen);
        box->set_disabled(!chosen && selected.size() >= options.group.count);
        box->show();
    }
}
} // namespace presentation
#endif
