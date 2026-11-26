#ifndef OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#define OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#include "godot_nodes.h"
#include "training_control.h"
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace presentation
{
inline godot::VBoxContainer *spell_rows(godot::Node &parent, const godot::String &name)
{
    auto *rows = godot::Object::cast_to<godot::VBoxContainer>(parent.get_node_or_null(name));
    if (!rows)
    {
        auto owned = instantiate_control<godot::VBoxContainer>(
            "res://scenes/control_templates/spell_rows.tscn");
        owned->set_name(name);
        rows = attach_child(parent, std::move(owned));
    }
    return rows;
}

void refresh_spell_groups(godot::VBoxContainer &rows,
                          const opengold::rules::SpellChoiceOptions &options,
                          const opengold::rules::SpellChoices &choices,
                          const godot::Callable &toggled, bool level_up_theme = false);

// Every visible choice under `root` becomes as wide as the widest one, so the
// choices line up across groups instead of spanning the page.
inline void equalize_choice_widths(godot::Node &root)
{
    using namespace godot;
    std::vector<CheckBox *> choices;
    std::vector<Node *> pending{&root};
    while (!pending.empty())
    {
        auto *node = pending.back();
        pending.pop_back();
        for (int i = 0; i < node->get_child_count(); ++i)
        {
            pending.push_back(node->get_child(i));
        }
        auto *choice = Object::cast_to<CheckBox>(node);
        if (choice && choice->is_visible())
        {
            choices.push_back(choice);
        }
    }
    real_t widest = 0;
    for (auto *choice : choices)
    {
        choice->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
        choice->set_custom_minimum_size({0, choice->get_custom_minimum_size().y});
        widest = std::max(widest, choice->get_combined_minimum_size().x);
    }
    for (auto *choice : choices)
    {
        choice->set_custom_minimum_size({widest, choice->get_custom_minimum_size().y});
    }
}

inline void toggle_spell(opengold::rules::SpellChoices &choice, opengold::ChoiceToggle toggle)
{
    const auto [group, picked, selected] = toggle;
    // A later class's prepared spell carries its mark ("@cleric", MULTI-1).
    const bool preparing = std::string_view(group).starts_with("prepared");
    const auto value = preparing
                           ? std::string(picked) + std::string(std::string_view(group).substr(8))
                           : std::string(picked);
    if (preparing && !choice.prepared)
    {
        choice.prepared.emplace();
    }
    auto &values = preparing ? *choice.prepared : choice.learning[std::string(group)];
    if (selected)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
        {
            values.emplace_back(value);
        }
    }
    else
    {
        std::erase(values, value);
    }
}

godot::Window *setup_spell_dialog(godot::Node &parent, const godot::String &name,
                                  const godot::Callable &cancel, const godot::Callable &apply);

// A Wizard prepares from a spellbook; a Cleric prepares from the whole class
// list, so only a Wizard's window shows a spellbook.
void spell_known(godot::Window &w, const opengold::rules::SpellAccess &access,
                 const std::vector<opengold::rules::CreationChoice> &preparation);
} // namespace presentation
#endif
