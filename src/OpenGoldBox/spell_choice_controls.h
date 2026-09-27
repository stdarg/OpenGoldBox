#ifndef OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#define OPENGOLDBOX_SPELL_CHOICE_CONTROLS_H
#include "training_control.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/window.hpp>

namespace presentation
{
inline godot::VBoxContainer *spell_rows(godot::Node &parent, const godot::String &name)
{
    auto *rows = godot::Object::cast_to<godot::VBoxContainer>(parent.get_node_or_null(name));
    if (!rows)
    {
        auto owned = make_node<godot::VBoxContainer>();
        owned->set_name(name);
        owned->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
        owned->add_theme_constant_override("separation", 10);
        rows = attach_child(parent, std::move(owned));
    }
    return rows;
}

template <class Translate>
void refresh_spell_groups(godot::VBoxContainer &rows,
                          const opengold::rules::SpellChoiceOptions &options,
                          const opengold::rules::SpellChoices &choices,
                          const godot::Callable &toggled, const Translate &tr)
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
        auto *section = spell_rows(rows, training_string(group.id).replace(":", "_"));
        rows.move_child(section, position++);
        section->show();
        auto *label = Object::cast_to<Label>(section->get_node_or_null("Count"));
        if (!label)
        {
            label = add_control<Label>(*section, "Count", {});
            label->set("autowrap_mode", 3);
        }
        const auto it = choices.learning.find(group.id);
        const auto picked = group.id == "prepared"
                                ? choices.prepared.value_or(std::vector<std::string>{})
                            : it == choices.learning.end() ? std::vector<std::string>{}
                                                           : it->second;
        label->set_text(tr(group.label) +
                        (group.id == "prepared" ? String()
                                                : " / " + tr(N_("Level")) + " " +
                                                      String::num_uint64(group.acquired_level)) +
                        " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        auto *pending = Object::cast_to<Label>(section->get_node_or_null("Pending"));
        if (!pending)
        {
            pending = add_control<Label>(*section, "Pending", {});
            pending->set("autowrap_mode", 3);
            pending->add_theme_font_size_override("font_size", 14);
        }
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
                auto owned = make_node<CheckBox>();
                owned->set_name(training_string(option.id));
                box = attach_child(*section, std::move(owned));
                box->set_custom_minimum_size(Vector2(0, 40));
                box->set_focus_mode(Control::FOCUS_ALL);
                style_choice(*box);
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

inline void toggle_spell(opengold::rules::SpellChoices &choice, bool selected, std::string group,
                         std::string value)
{
    if (group == "prepared" && !choice.prepared)
        choice.prepared.emplace();
    auto &values = group == "prepared" ? *choice.prepared : choice.learning[group];
    if (selected)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(std::move(value));
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
    owned->set_size(Vector2i(700, 700));
    owned->set_min_size(Vector2i(700, 700));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(parent, std::move(owned));
    w->connect("close_requested", cancel);
    add_control<Label>(*w, "Title", Rect2(24, 18, 652, 34));
    auto *known = add_control<RichTextLabel>(*w, "Known", Rect2(24, 58, 652, 126));
    known->set_scroll_active(true);
    auto *scroll = add_control<ScrollContainer>(*w, "Choices", Rect2(24, 196, 652, 310));
    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    scroll->set_follow_focus(true);
    spell_rows(*scroll, "Rows");
    add_control<Label>(*w, "ReplaceLabel", Rect2(24, 518, 318, 26))
        ->set_text(tr(N_("Replace cantrip")));
    add_control<Label>(*w, "WithLabel", Rect2(358, 518, 318, 26))->set_text(tr(N_("With")));
    add_control<OptionButton>(*w, "Replace", Rect2(24, 550, 318, 36));
    add_control<OptionButton>(*w, "With", Rect2(358, 550, 318, 36));
    auto *error = add_control<Label>(*w, "Error", Rect2(24, 595, 652, 40));
    error->set("autowrap_mode", 3);
    error->add_theme_font_size_override("font_size", 14);
    auto *back = add_control<Button>(*w, "Cancel", Rect2(280, 644, 150, 40));
    back->set_text(tr(N_("Cancel")));
    back->connect("pressed", cancel);
    auto *ok = add_control<Button>(*w, "Apply", Rect2(442, 644, 234, 40));
    ok->set_text(tr(N_("Apply spell choices")));
    ok->connect("pressed", apply);
    return w;
}

template <class Translate>
void spell_known(godot::Window &w, const opengold::rules::SpellAccess &access, const Translate &tr)
{
    godot::String text = tr(N_("Known cantrips")) + ": ";
    for (const auto &s : access.cantrips)
        text += tr(s.label) + "; ";
    text += "\n" + tr(N_("Spellbook")) + ": ";
    for (const auto &s : access.spellbook)
        text += tr(s.label) + "; ";
    text += "\n" + tr(N_("Prepared spells")) + ": ";
    for (const auto &id : access.prepared)
        for (const auto &s : access.spellbook)
            if (s.id == id)
                text += tr(s.label) + "; ";
    w.get_node<godot::RichTextLabel>("Known")->set_text(text);
}
} // namespace presentation
#endif
