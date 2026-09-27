#ifndef OPENGOLDBOX_TRAINING_CONTROL_H
#define OPENGOLDBOX_TRAINING_CONTROL_H
#include "godot_nodes.h"
#include "opengold/character_creator.h"
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <algorithm>
#ifndef N_
#define N_(message) message
#endif

namespace presentation
{
inline godot::String training_string(std::string_view s)
{
    return godot::String::utf8(s.data(), s.size());
}

template <class Translate> godot::String training_source(std::string_view id, const Translate &tr)
{
    if (id == "origin:languages")
        return tr(N_("Starting languages"));
    if (id == "class:fighter:fighting_style")
        return tr(N_("Fighter Fighting Style"));
    if (id == "class:barbarian")
        return tr(N_("Barbarian class"));
    if (id == "class:bard:instruments")
        return tr(N_("Bard class"));
    if (id == "class:bard")
        return tr(N_("Bard class"));
    if (id == "class:cleric")
        return tr(N_("Cleric class"));
    if (id == "class:druid")
        return tr(N_("Druid class"));
    if (id.ends_with(":weapon_mastery:4"))
        return training_source(id.substr(0, id.size() - 17), tr) + " / " + tr(N_("Weapon Mastery"));
    if (id.ends_with(":weapon_mastery"))
        return training_source(id.substr(0, id.size() - 15), tr) + " / " + tr(N_("Weapon Mastery"));
    if (id == "class:fighter")
        return tr(N_("Fighter class"));
    if (id == "class:monk:tools")
        return tr(N_("Monk class"));
    if (id == "class:monk")
        return tr(N_("Monk class"));
    if (id == "class:paladin")
        return tr(N_("Paladin class"));
    if (id == "class:ranger")
        return tr(N_("Ranger class"));
    if (id == "class:sorcerer")
        return tr(N_("Sorcerer class"));
    if (id == "class:warlock")
        return tr(N_("Warlock class"));
    if (id == "class:wizard")
        return tr(N_("Wizard class"));
    if (id == "class:rogue")
        return tr(N_("Rogue class"));
    if (id == "class:rogue:expertise")
        return tr(N_("Rogue Expertise"));
    if (id == "class:rogue:thieves_cant")
        return tr(N_("Rogue / Thieves' Cant"));
    if (id == "background:acolyte")
        return tr(N_("Acolyte background"));
    if (id == "background:soldier:gaming_set")
        return tr(N_("Soldier background"));
    if (id == "background:soldier")
        return tr(N_("Soldier background"));
    if (id == "background:sage")
        return tr(N_("Sage background"));
    if (id == "class:wizard:scholar")
        return tr(N_("Wizard Scholar"));
    if (id == "background:criminal")
        return tr(N_("Criminal background"));
    return training_string(id);
}

template <class Translate>
godot::String training_sources(const std::vector<opengold::rules::FeatureGrant> &grants,
                               const Translate &tr)
{
    godot::String result;
    for (const auto &g : grants)
    {
        if (!result.is_empty())
            result += ", ";
        result += training_source(g.source_id, tr);
    }
    return result;
}

template <class Translate>
godot::String training_summary(const opengold::rules::TrainingProfile &profile, const Translate &tr,
                               bool fixed = false)
{
    godot::String text;
    if (!fixed)
        text = "\n\n[b]" + tr(N_("Training")) + "[/b]\n" +
               tr(profile.complete ? N_("Supported training choices complete")
                                   : N_("Training choices pending")) +
               "\n";
    for (const auto &s : profile.skills)
    {
        if (fixed && !s.proficient)
            continue;
        text += tr(s.label);
        if (!fixed)
        {
            text +=
                " " + godot::String(s.bonus >= 0 ? "+" : "") + godot::String::num_int64(s.bonus);
            if (s.expertise)
                text += " / " + tr(N_("Expertise"));
            else if (s.proficient)
                text += " / " + tr(N_("Proficient"));
        }
        const auto sources = training_sources(s.sources, tr);
        if (!sources.is_empty())
            text += " (" + sources + ")";
        text += "\n";
    }
    for (const auto &t : profile.tools)
        text += tr(t.label) + " (" + training_sources(t.sources, tr) + ")\n";
    for (const auto &l : profile.languages)
        text += tr(l.label) + " (" + training_sources(l.sources, tr) + ")\n";
    for (const auto &m : profile.masteries)
        text += tr(N_("Weapon Mastery")) + ": " + tr(m.label) + " (" +
                training_sources(m.sources, tr) + ")\n";
    return text;
}

inline void style_choice(godot::CheckBox &control)
{
    using namespace godot;
    auto *check = &control;
    for (const char *state : {"normal", "hover", "pressed", "hover_pressed", "disabled", "focus"})
    {
        Ref<StyleBoxFlat> style;
        style.instantiate();
        style->set_bg_color(
            Color(state == std::string_view("pressed") || state == std::string_view("hover_pressed")
                      ? "304851"
                      : "19262e"));
        style->set_border_color(Color(state == std::string_view("focus")   ? "ebcb80"
                                      : state == std::string_view("hover") ? "b0c5cc"
                                                                           : "506570"));
        style->set_border_width_all(state == std::string_view("focus") ? 2 : 1);
        style->set_corner_radius_all(3);
        style->set_content_margin_all(7);
        if (state == std::string_view("focus"))
            style->set_draw_center(false);
        check->add_theme_stylebox_override(state, style);
    }
}

inline void setup_training_controls(godot::Node &parent)
{
    auto *fixed = add_control<godot::RichTextLabel>(parent, "TrainingFixed", {});
    fixed->set_use_bbcode(true);
    fixed->set_auto_translate_mode(godot::Node::AUTO_TRANSLATE_MODE_DISABLED);
    fixed->add_theme_font_size_override("normal_font_size", 14);
    auto *scroll = add_control<godot::ScrollContainer>(parent, "Training", {});
    scroll->set_horizontal_scroll_mode(godot::ScrollContainer::SCROLL_MODE_DISABLED);
    scroll->set_follow_focus(true);
    auto owned = make_node<godot::VBoxContainer>();
    owned->set_name("Rows");
    owned->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
    owned->add_theme_constant_override("separation", 12);
    attach_child(*scroll, std::move(owned));
}

// The scene owns every node. Reuse controls across refreshes so toggling does
// not destroy the focused checkbox or its keyboard navigation position.
template <class Translate>
void refresh_training_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                               const godot::Callable &toggled, const godot::Callable &selected,
                               const Translate &tr,
                               const opengold::rules::TrainingChoices &locked = {})
{
    using namespace godot;
    auto *rows = parent.get_node<VBoxContainer>("Training/Rows");
    auto fixed = creator.draft();
    fixed.training.clear();
    parent.get_node<RichTextLabel>("TrainingFixed")
        ->set_text("[b]" + tr(N_("Fixed training")) + "[/b]\n" +
                   training_summary(creator.rules().evaluate(fixed, false).training, tr, true));
    const auto groups = creator.training_options();
    for (unsigned i = groups.size(); i < static_cast<unsigned>(rows->get_child_count()); ++i)
        rows->get_node<Control>(String("Group") + String::num_uint64(i))->hide();
    for (unsigned i = 0; i < groups.size(); ++i)
    {
        const auto &group = groups[i];
        const auto original = locked.find(group.id);
        const bool has_locked = original != locked.end() && !original->second.empty();
        const auto name = String("Group") + String::num_uint64(i);
        auto *box = Object::cast_to<VBoxContainer>(rows->get_node_or_null(name));
        if (!box)
        {
            auto owned = make_node<VBoxContainer>();
            owned->set_name(name);
            box = attach_child(*rows, std::move(owned));
            box->add_theme_constant_override("separation", 5);
            auto label = make_node<Label>();
            label->set_name("Title");
            attach_child(*box, std::move(label));
        }
        box->show();
        rows->move_child(box, i);
        const auto found = creator.draft().training.find(group.id);
        const std::vector<std::string> empty;
        const auto &picked = found == creator.draft().training.end() ? empty : found->second;
        auto *title = box->get_node<Label>("Title");
        title->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
        title->set_text(tr(group.label) + " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        auto *choice = Object::cast_to<OptionButton>(box->get_node_or_null("Choice"));
        if (choice && group.control != opengold::rules::TrainingChoiceControl::single_selection)
        {
            choice->hide();
            choice->set_disabled(true);
        }
        if (group.control == opengold::rules::TrainingChoiceControl::single_selection)
        {
            for (int j = 0; j < box->get_child_count(); ++j)
                if (auto *check = Object::cast_to<CheckBox>(box->get_child(j)))
                {
                    check->hide();
                    check->set_disabled(true);
                }
            if (!choice)
            {
                auto owned = make_node<OptionButton>();
                owned->set_name("Choice");
                choice = attach_child(*box, std::move(owned));
                choice->set_focus_mode(Control::FOCUS_ALL);
                choice->set_custom_minimum_size(Vector2(0, 38));
                choice->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
            }
            if (choice->has_meta("training_callback"))
            {
                const Callable previous = choice->get_meta("training_callback");
                choice->disconnect("item_selected", previous);
            }
            const auto callback = selected.bind(training_string(group.id));
            choice->connect("item_selected", callback);
            choice->set_meta("training_callback", callback);
            choice->clear();
            choice->add_item(tr(N_("Choose an option")));
            choice->set_item_disabled(0, true);
            int index = 0;
            for (unsigned n = 0; n < group.options.size(); ++n)
            {
                const auto &option = group.options[n];
                choice->add_item(tr(option.label));
                choice->set_item_tooltip(n + 1, tr(option.description));
                if (std::find(picked.begin(), picked.end(), option.id) != picked.end())
                    index = n + 1;
            }
            choice->select(index);
            choice->set_tooltip_text(training_source(group.id, tr));
            choice->set_disabled(has_locked);
            choice->show();
            continue;
        }
        for (int j = 0; j < box->get_child_count(); ++j)
            if (auto *check = Object::cast_to<CheckBox>(box->get_child(j)))
            {
                if (std::none_of(group.options.begin(), group.options.end(),
                                 [&](const auto &o)
                                 {
                                     return training_string(o.id) == String(check->get_name());
                                 }))
                {
                    check->hide();
                    check->set_disabled(true);
                }
            }
        int option_index = 1;
        for (const auto &option : group.options)
        {
            const auto node_name = training_string(option.id);
            auto *check = Object::cast_to<CheckBox>(box->get_node_or_null(node_name));
            if (!check)
            {
                auto owned = make_node<CheckBox>();
                owned->set_name(node_name);
                check = attach_child(*box, std::move(owned));
                check->set_focus_mode(Control::FOCUS_ALL);
                check->set_custom_minimum_size(Vector2(0, 34));
                check->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
                style_choice(*check);
            }
            // A reused row can now belong to another class's skill group.
            if (check->has_meta("training_callback"))
            {
                const Callable previous = check->get_meta("training_callback");
                check->disconnect("toggled", previous);
            }
            const auto callback = toggled.bind(training_string(group.id), node_name);
            check->connect("toggled", callback);
            check->set_meta("training_callback", callback);
            box->move_child(check, option_index++);
            const bool selected =
                std::find(picked.begin(), picked.end(), option.id) != picked.end();
            check->set_text(tr(option.label) + (option.description.empty()
                                                    ? String()
                                                    : String(" / ") + tr(option.description)));
            check->set_pressed_no_signal(selected);
            check->set_disabled(
                (has_locked && std::find(original->second.begin(), original->second.end(),
                                         option.id) != original->second.end()) ||
                (!selected && picked.size() >= group.count));
            check->show();
            check->set_tooltip_text(training_source(group.id, tr));
        }
    }
    String signature;
    for (const auto &group : groups)
        signature += training_string(group.id) + ";";
    if (!rows->has_meta("training_groups") ||
        String(rows->get_meta("training_groups")) != signature)
        parent.get_node<ScrollContainer>("Training")->set_v_scroll(0);
    rows->set_meta("training_groups", signature);
    unsigned first = 0;
    for (unsigned i = 0; i < groups.size(); ++i)
        if (groups[i].control == opengold::rules::TrainingChoiceControl::single_selection)
            rows->move_child(rows->get_node<VBoxContainer>(String("Group") + String::num_uint64(i)),
                             first++);
}
} // namespace presentation
#endif
