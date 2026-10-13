#ifndef OPENGOLDBOX_TRAINING_CONTROL_H
#define OPENGOLDBOX_TRAINING_CONTROL_H
#include "godot_nodes.h"
#include "localization.h"
#include "opengold/character_creator.h"
#include <godot_cpp/classes/button_group.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <algorithm>
#include <vector>

namespace presentation
{
inline godot::String training_string(std::string_view s)
{
    return godot::String::utf8(s.data(), s.size());
}

template <class Translate> godot::String training_source(std::string_view id, const Translate &tr)
{
    if (id == "class:fighter:fighting_style")
        return tr(N_("Fighter Fighting Style"));
    if (id == "class:barbarian")
        return tr(N_("Barbarian class"));
    if (id == "class:bard")
        return tr(N_("Bard class"));
    if (id == "class:cleric")
        return tr(N_("Cleric class"));
    if (id == "class:cleric:divine_order")
        return tr(N_("Cleric Divine Order"));
    if (id == "class:druid")
        return tr(N_("Druid class"));
    if (id == "class:druid:primal_order")
        return tr(N_("Druid Primal Order"));
    if (id.ends_with(":weapon_mastery:4"))
        return training_source(id.substr(0, id.size() - 17), tr) + " / " + tr(N_("Weapon Mastery"));
    if (id.ends_with(":weapon_mastery"))
        return training_source(id.substr(0, id.size() - 15), tr) + " / " + tr(N_("Weapon Mastery"));
    if (id == "class:fighter")
        return tr(N_("Fighter class"));
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
    if (id == "background:acolyte")
        return tr(N_("Acolyte background"));
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
        text = "\n\n[theme_title]" + tr(N_("Training")) + "[/theme_title]\n" +
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
    for (const auto &m : profile.masteries)
        text += tr(N_("Weapon Mastery")) + ": " + tr(m.label) + " (" +
                training_sources(m.sources, tr) + ")\n";
    return text;
}

// What a weapon's mastery property does, keyed by its SRD name.
inline const char *mastery_effect(std::string_view mastery)
{
    if (mastery == "Cleave")
        return N_("After a melee hit, attack a second creature within 5 feet of the first with the same weapon. Once per turn.");
    if (mastery == "Graze")
        return N_("A miss still deals damage equal to the ability modifier used for the attack.");
    if (mastery == "Nick")
        return N_("The Light weapon's extra attack comes with the Attack action instead of a Bonus Action. Once per turn.");
    if (mastery == "Push")
        return N_("On a hit, push a Large or smaller creature up to 10 feet straight away from you.");
    if (mastery == "Sap")
        return N_("On a hit, the target has Disadvantage on its next attack roll before your next turn.");
    if (mastery == "Slow")
        return N_("On a hit that deals damage, the target's Speed drops by 10 feet until your next turn.");
    if (mastery == "Topple")
        return N_("On a hit, the target makes a Constitution save (DC 8 + the attack's ability modifier + proficiency bonus) or falls Prone.");
    if (mastery == "Vex")
        return N_("On a hit that deals damage, your next attack against that target before the end of your next turn has Advantage.");
    return nullptr;
}

inline godot::String signed_bonus(int value)
{
    return godot::String(value >= 0 ? "+" : "") + godot::String::num_int64(value);
}

// What choosing a training option gives: a skill's check bonus once chosen,
// or what a weapon's mastery property does.
template <class Translate>
godot::String training_benefit(const opengold::rules::TrainingChoiceGroup &group,
                               const opengold::rules::CreationChoice &option,
                               const opengold::rules::CharacterSheet &sheet, const Translate &tr)
{
    const auto &skills = sheet.training.skills;
    const auto skill = std::find_if(skills.begin(), skills.end(), [&](const auto & s)
    {
        return s.id == option.id;
    });
    if (skill != skills.end())
    {
        const int proficiency = 2 + (sheet.level - 1) / 4;
        const bool expertise = group.id.ends_with(":expertise");
        const bool gained = expertise ? !skill->expertise : !skill->proficient;
        const auto name = tr(skill->label);
        const auto chosen = signed_bonus(skill->bonus + (gained ? proficiency : 0));
        if (!gained)
            return name + godot::String(" ") + chosen;
        // The demo shares this header without the game's formatter.
        return tr(expertise ? N_("{skill} {bonus} with Expertise (now {current})")
                  : N_("{skill} {bonus} if chosen (now {current})"))
               .replace("{skill}", name)
               .replace("{bonus}", chosen)
               .replace("{current}", signed_bonus(skill->bonus));
    }
    if (const auto *effect = mastery_effect(option.description))
        return tr(option.description) + godot::String(": ") + tr(effect);
    return tr(option.description);
}

// The scene owns every node. Reuse controls across refreshes so toggling does
// not destroy the focused checkbox or its keyboard navigation position.
template <class Translate>
void refresh_training_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                               const godot::Callable &toggled, const Translate &tr,
                               const opengold::rules::TrainingChoices &locked = {})
{
    using namespace godot;
    auto *rows = &required_node<VBoxContainer>(parent, "Training/Rows");
    auto fixed = creator.draft();
    fixed.training.clear();
    required_node<Label>(parent, "TrainingFixedTitle").set_text(tr(N_("Fixed training")));
    required_node<RichTextLabel>(parent, "TrainingFixed")
    .call("render", training_summary(
                   creator.rules().evaluate(fixed, opengold::rules::NameRequirement::optional).training,
                   tr, true));
    const auto groups = creator.training_options();
    const auto sheet = creator.sheet();
    for (auto i = static_cast<unsigned>(groups.size()); i < static_cast<unsigned>(rows->get_child_count()); ++i)
        required_node<Control>(*rows, String("Group") + String::num_uint64(i)).hide();
    for (unsigned i = 0; i < groups.size(); ++i)
    {
        const auto &group = groups[i];
        const auto original = locked.find(group.id);
        const bool has_locked = original != locked.end() && !original->second.empty();
        const auto name = String("Group") + String::num_uint64(i);
        // Each group is a two-column grid: a choice sized to the longest one,
        // then what it gives.
        auto *box = Object::cast_to<GridContainer>(rows->get_node_or_null(name));
        if (!box)
        {
            auto owned = instantiate_control<GridContainer>(
                "res://scenes/control_templates/training_group.tscn");
            owned->set_name(name);
            box = attach_child(*rows, std::move(owned));
        }
        box->show();
        rows->move_child(box, i);
        const auto found = creator.draft().training.find(group.id);
        const std::vector<std::string> empty;
        const auto &picked = found == creator.draft().training.end() ? empty : found->second;
        // The title sits in a zero-width slot, so it does not widen the choices'
        // column; its text extends past the slot.
        auto *title = &required_node<Label>(*box, "TitleSlot/Title");
        title->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
        title->set_text(tr(group.label) + " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        // A reused group can hold another class's options.
        for (int j = 0; j < box->get_child_count(); ++j)
        {
            auto *row = Object::cast_to<Control>(box->get_child(j));
            const String row_name = row->get_name();
            const bool used = row_name == "TitleSlot" || row_name == "TitleSpacer" ||
                              std::any_of(group.options.begin(), group.options.end(),
                                          [&](const auto & o)
            {
                const auto id = training_string(o.id);
                return row_name == id || row_name == id + String("Benefit");
            });
            if (used)
                continue;
            row->hide();
            if (auto *check = Object::cast_to<CheckBox>(row))
                check->set_disabled(true);
        }
        // A single choice is a set of radio buttons sharing one group.
        const bool single =
            group.control == opengold::rules::TrainingChoiceControl::single_selection;
        if (single && !box->has_meta("radios"))
            box->set_meta("radios", Ref<ButtonGroup>(memnew(ButtonGroup)));
        const Ref<ButtonGroup> radios = single ? Ref<ButtonGroup>(box->get_meta("radios"))
                                        : Ref<ButtonGroup>();
        int row_index = 2; // after the title's slot and its spacer
        for (const auto &option : group.options)
        {
            const auto node_name = training_string(option.id);
            auto *check = Object::cast_to<CheckBox>(box->get_node_or_null(node_name));
            if (!check)
            {
                auto owned = instantiate_control<CheckBox>(
                    "res://scenes/control_templates/training_checkbox.tscn");
                owned->set_name(node_name);
                check = attach_child(*box, std::move(owned));
            }
            const auto benefit_name = node_name + String("Benefit");
            auto *benefit = Object::cast_to<Label>(box->get_node_or_null(benefit_name));
            if (!benefit)
            {
                auto owned = instantiate_control<Label>(
                    "res://scenes/control_templates/training_benefit.tscn");
                owned->set_name(benefit_name);
                benefit = attach_child(*box, std::move(owned));
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
            box->move_child(check, row_index++);
            box->move_child(benefit, row_index++);
            check->set_button_group(radios);
            const bool selected =
                std::find(picked.begin(), picked.end(), option.id) != picked.end();
            check->set_text(tr(option.label));
            check->set_pressed_no_signal(selected);
            check->set_disabled(
                (has_locked && std::find(original->second.begin(), original->second.end(),
                                         option.id) != original->second.end()) ||
                (!single && !selected && picked.size() >= group.count));
            check->show();
            check->set_tooltip_text(training_source(group.id, tr));
            benefit->set_text(training_benefit(group, option, sheet, tr));
            benefit->show();
        }
    }
    // Every choice is as wide as the widest one on the page, so the choices and
    // their benefits line up across groups.
    std::vector<CheckBox *> choices;
    for (unsigned i = 0; i < groups.size(); ++i)
    {
        auto &box = required_node<GridContainer>(*rows, String("Group") + String::num_uint64(i));
        for (int j = 0; j < box.get_child_count(); ++j)
        {
            auto *choice = Object::cast_to<CheckBox>(box.get_child(j));
            if (choice && choice->is_visible())
                choices.push_back(choice);
        }
    }
    real_t widest = 0;
    for (auto *choice : choices)
    {
        choice->set_custom_minimum_size({0, choice->get_custom_minimum_size().y});
        widest = std::max(widest, choice->get_combined_minimum_size().x);
    }
    for (auto *choice : choices)
        choice->set_custom_minimum_size({widest, choice->get_custom_minimum_size().y});

    String signature;
    for (const auto &group : groups)
        signature += training_string(group.id) + ";";
    required_node<ScrollContainer>(parent, "Training").call("reset_for", signature);
    unsigned first = 0;
    for (unsigned i = 0; i < groups.size(); ++i)
        if (groups[i].control == opengold::rules::TrainingChoiceControl::single_selection)
            rows->move_child(
                &required_node<GridContainer>(*rows, String("Group") + String::num_uint64(i)),
                first++);
}
} // namespace presentation
#endif
