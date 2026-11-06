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
    {
        if (auto *control = Object::cast_to<Control>(rows.get_child(i)))
        {
            control->hide();
        }
    }
    auto groups = options.learning;
    // Each preparation section's spells are recorded with its class's mark
    // (MULTI-1); "prepared" is the starting class's, "prepared@cleric" a
    // later Cleric's.
    std::vector<std::pair<std::string, const std::vector<std::string> *>> sections;
    if (options.may_prepare && options.class_preparation.empty())
    {
        groups.push_back(
            {"prepared", "Prepared spells", options.prepared_count, options.preparation});
        sections.emplace_back("", &options.locked_prepared);
    }
    for (const auto &section : options.class_preparation)
    {
        groups.push_back({"prepared" + section.tag, section.label, section.count,
                          section.preparation});
        sections.emplace_back(section.tag, &section.locked);
    }
    int position = 0;
    for (const auto &group : groups)
    {
        const bool preparing = group.id.starts_with("prepared");
        const auto tag = preparing ? group.id.substr(8) : std::string();
        const auto *locked_spells = preparing
                                        ? std::find_if(sections.begin(), sections.end(),
                                                       [&](const auto &section)
                                                       {
                                                           return section.first == tag;
                                                       })
                                              ->second
                                        : nullptr;
        // Node names allow neither ':' nor '@'.
        const auto section_name = training_string(group.id).replace(":", "_").replace("@", "_");
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
        // A section's own picks, without their class's mark.
        std::vector<std::string> marked;
        if (preparing && choices.prepared)
        {
            for (const auto &entry : *choices.prepared)
            {
                const auto at = entry.find('@');
                if (tag.empty() ? at == std::string::npos
                                : at != std::string::npos && entry.substr(at) == tag)
                {
                    marked.push_back(entry.substr(0, at));
                }
            }
        }
        const auto &picked = preparing                     ? marked
                             : it == choices.learning.end() ? none
                                                            : it->second;
        label->set_text((preparing && group.id != "prepared"
                             ? tr(N_("Prepared spells")) + " / " + tr(group.label)
                             : tr(group.label)) +
                        (preparing ? String()
                                   : " / " + tr(N_("Level")) + " " +
                                         String::num_uint64(group.acquired_level)) +
                        " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        auto *pending = &required_node<Label>(*section, "Pending");
        pending->set_text(tr(N_("Unsupported choices remain pending.")));
        pending->set_visible(group.options.size() < group.count);
        for (int i = 0; i < section->get_child_count(); ++i)
        {
            if (auto *box = Object::cast_to<CheckBox>(section->get_child(i)))
            {
                box->hide();
            }
        }
        // The screen is reused, and a section's list changes (another class,
        // another spellbook), so each box moves to its place in the list.
        auto box_position = pending->get_index() + 1;
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
            section->move_child(box, box_position++);
            const bool selected =
                std::find(picked.begin(), picked.end(), option.id) != picked.end();
            const bool locked =
                locked_spells && std::find(locked_spells->begin(), locked_spells->end(),
                                           option.id) != locked_spells->end();
            // A spell learned in another group (a class's cantrip and Magic
            // Initiate's) is learned once.
            const bool learned_elsewhere =
                !preparing &&
                std::any_of(choices.learning.begin(), choices.learning.end(),
                            [&](const auto &entry)
                            {
                                return entry.first != group.id &&
                                       std::find(entry.second.begin(), entry.second.end(),
                                                 option.id) != entry.second.end();
                            });
            box->show();
            box->set_text(tr(option.label));
            box->set_tooltip_text(tr(option.description));
            box->set_pressed_no_signal(selected);
            box->set_disabled(locked || learned_elsewhere ||
                              (!selected && picked.size() >= group.count));
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
    const auto value =
        preparing ? std::string(picked) + std::string(std::string_view(group).substr(8))
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
    {
        text += tr(s.label) + "; ";
    }
    if (spellbook)
    {
        text += "\n" + tr(N_("Spellbook")) + ": ";
        for (const auto &s : access.spellbook)
        {
            text += tr(s.label) + "; ";
        }
    }
    text += "\n" + tr(N_("Prepared spells")) + ": ";
    for (const auto &id : access.prepared)
    {
        for (const auto &s : preparation)
        {
            if (s.id == id)
            {
                text += tr(s.label) + "; ";
            }
        }
    }
    required_node<godot::RichTextLabel>(w, "Known").set_text(text);
}
} // namespace presentation
#endif
