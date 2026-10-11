#ifndef OPENGOLDBOX_CANTRIP_CONTROL_H
#define OPENGOLDBOX_CANTRIP_CONTROL_H
#include "training_control.h"

namespace presentation
{
template <class Translate>
void refresh_cantrip_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                              const godot::Callable &toggled, const Translate &tr)
{
    using namespace godot;
    auto *rows = &required_node<VBoxContainer>(parent, "SpellChoices/Rows");
    const auto group = creator.rules().cantrip_options(creator.draft());
    const auto picked = creator.draft().cantrips.value_or(std::vector<std::string> {});
    required_node<Label>(*rows, "Count").set_text(tr(group.label) + " (" +
            String::num_uint64(picked.size()) + " / " +
            String::num_uint64(group.count) + ")");
    required_node<Label>(*rows, "Pending").set_text(tr(N_(
                "Unfilled cantrip choices remain pending. More choices will become available as spell support expands.")));
    // A Paladin prepares spells without learning cantrips.
    required_node<Label>(*rows, "Count").set_visible(group.count > 0);
    required_node<Label>(*rows, "Pending").set_visible(picked.size() < group.count);
    for (int i = 0; i < rows->get_child_count(); ++i)
        if (auto *check = Object::cast_to<CheckBox>(rows->get_child(i)))
        {
            const auto id = check->get_name();
            if (std::none_of(group.options.begin(), group.options.end(),
                             [&](const auto & option)
        {
            return training_string(option.id) == id;
            }))
            check->hide();
        }
    for (const auto &option : group.options)
    {
        const auto name = training_string(option.id);
        auto *check = Object::cast_to<CheckBox>(rows->get_node_or_null(name));
        if (!check)
        {
            auto owned = instantiate_control<CheckBox>(
                "res://scenes/control_templates/cantrip_choice.tscn");
            owned->set_name(name);
            check = attach_child(*rows, std::move(owned));
            style_choice(*check);
            check->connect("toggled", toggled.bind(name));
        }
        const bool selected = std::find(picked.begin(), picked.end(), option.id) != picked.end();
        check->show();
        check->set_text(tr(option.label));
        check->set_tooltip_text(tr(option.description));
        check->set_pressed_no_signal(selected);
        check->set_disabled(!selected && picked.size() >= group.count);
    }
}
} // namespace presentation
#endif
