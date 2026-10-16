#ifndef OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#define OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#include "training_control.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>

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

template <class Translate>
void refresh_spell_groups(godot::VBoxContainer &rows,
                          const opengold::rules::SpellChoiceOptions &options,
                          const opengold::rules::SpellChoices &choices,
                          const godot::Callable &toggled, const Translate &tr,
                          bool level_up_theme = false)
{
    using namespace godot;
    for (int i = 0; i < rows.get_child_count(); ++i)
        if (auto *control = Object::cast_to<Control>(rows.get_child(i)))
            control->hide();
    auto groups = options.learning;
    if (options.may_prepare)
        groups.push_back(
            {"prepared", "Prepared spells", options.prepared_count, options.preparation});
    int position = 0;
    for (const auto &group : groups)
    {
        const auto section_name = training_string(group.id).replace(":", "_");
        auto *section = Object::cast_to<VBoxContainer>(rows.get_node_or_null(section_name));
        if (!section)
        {
            auto owned = instantiate_control<VBoxContainer>(
                "res://scenes/control_templates/spell_group.tscn");
            owned->set_name(section_name);
            section = attach_child(rows, std::move(owned));
        }
        section->call("set_level_up", level_up_theme);
        rows.move_child(section, position++);
        section->show();
        auto *label = &required_node<Label>(*section, "Count");
        const auto it = choices.learning.find(group.id);
        static const std::vector<std::string> none;
        const auto &picked = group.id == "prepared" ? (choices.prepared ? *choices.prepared : none)
                             : it == choices.learning.end() ? none
                                                            : it->second;
        label->set_text(tr(group.label) +
                        (group.id == "prepared" ? String()
                                                : " / " + tr(N_("Level")) + " " +
                                                      String::num_uint64(group.acquired_level)) +
                        " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        auto *pending = &required_node<Label>(*section, "Pending");
        pending->set_text(tr(N_("Unsupported choices remain pending.")));
        pending->set_visible(group.options.size() < group.count);
        for (int i = 0; i < section->get_child_count(); ++i)
            if (auto *box = Object::cast_to<CheckBox>(section->get_child(i)))
                box->hide();
        for (const auto &option : group.options)
        {
            auto *box =
                Object::cast_to<CheckBox>(section->get_node_or_null(training_string(option.id)));
            if (!box)
            {
                auto owned = instantiate_control<CheckBox>(
                    "res://scenes/control_templates/spell_choice.tscn");
                owned->set_name(training_string(option.id));
                box = attach_child(*section, std::move(owned));
                box->connect("toggled",
                             toggled.bind(training_string(group.id), training_string(option.id)));
            }
            const bool selected =
                std::find(picked.begin(), picked.end(), option.id) != picked.end();
            const bool locked =
                group.id == "prepared" &&
                std::find(options.locked_prepared.begin(), options.locked_prepared.end(),
                          option.id) != options.locked_prepared.end();
            box->show();
            box->set_text(tr(option.label));
            box->set_tooltip_text(tr(option.description));
            box->set_pressed_no_signal(selected);
            box->set_disabled(locked || (!selected && picked.size() >= group.count));
        }
    }
}

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
            pending.push_back(node->get_child(i));
        auto *choice = Object::cast_to<CheckBox>(node);
        if (choice && choice->is_visible())
            choices.push_back(choice);
    }
    real_t widest = 0;
    for (auto *choice : choices)
    {
        choice->set_h_size_flags(Control::SIZE_SHRINK_BEGIN);
        choice->set_custom_minimum_size({0, choice->get_custom_minimum_size().y});
        widest = std::max(widest, choice->get_combined_minimum_size().x);
    }
    for (auto *choice : choices)
        choice->set_custom_minimum_size({widest, choice->get_custom_minimum_size().y});
}

inline void toggle_spell(opengold::rules::SpellChoices &choice, opengold::ChoiceToggle toggle)
{
    const auto [group, value, selected] = toggle;
    if (group == "prepared" && !choice.prepared)
        choice.prepared.emplace();
    auto &values = group == "prepared" ? *choice.prepared : choice.learning[std::string(group)];
    if (selected)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
            values.emplace_back(value);
    }
    else
        std::erase(values, value);
}

template <class Translate>
godot::Window *setup_spell_dialog(godot::Node &parent, const godot::String &name,
                                  const godot::Callable &cancel, const godot::Callable &apply,
                                  const Translate &tr)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name(name);
    owned->set_title(tr(N_("Spellbook")));
    attach_dialog_layout(*owned);
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(parent, std::move(owned));
    w->connect("close_requested", cancel);
    dialog_control<Label>(*w, "Title");
    auto *known = dialog_control<RichTextLabel>(*w, "Known");
    known->set_scroll_active(true);
    auto *scroll = dialog_control<ScrollContainer>(*w, "Choices");
    spell_rows(*scroll, "Rows");
    dialog_control<Label>(*w, "ReplaceLabel")->set_text(tr(N_("Replace cantrip")));
    dialog_control<Label>(*w, "WithLabel")->set_text(tr(N_("With")));
    dialog_control<OptionButton>(*w, "Replace");
    dialog_control<OptionButton>(*w, "With");
    dialog_control<Label>(*w, "Error");
    auto *back = dialog_control<Button>(*w, "Cancel");
    back->set_text(tr(N_("Cancel")));
    back->connect("pressed", cancel);
    auto *ok = dialog_control<Button>(*w, "Apply");
    ok->set_text(tr(N_("Apply spell choices")));
    ok->connect("pressed", apply);
    return w;
}

// A Wizard prepares from a spellbook; a Cleric prepares from the whole class
// list, so only a Wizard's window shows a spellbook.
template <class Translate>
void spell_known(godot::Window &w, const opengold::rules::SpellAccess &access,
                 const std::vector<opengold::rules::CreationChoice> &preparation,
                 const Translate &tr)
{
    const bool spellbook = access.spellbook_choices > 0;
    w.set_title(tr(spellbook ? N_("Spellbook") : N_("Prepared spells")));
    godot::String text = tr(N_("Known cantrips")) + ": ";
    for (const auto &s : access.cantrips)
        text += tr(s.label) + "; ";
    if (spellbook)
    {
        text += "\n" + tr(N_("Spellbook")) + ": ";
        for (const auto &s : access.spellbook)
            text += tr(s.label) + "; ";
    }
    text += "\n" + tr(N_("Prepared spells")) + ": ";
    for (const auto &id : access.prepared)
        for (const auto &s : preparation)
            if (s.id == id)
                text += tr(s.label) + "; ";
    required_node<godot::RichTextLabel>(w, "Known").set_text(text);
}
} // namespace presentation
#endif
