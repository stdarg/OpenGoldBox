#include "training_control.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/button_group.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <algorithm>
#include <optional>

namespace presentation
{
namespace
{
godot::String training_source(std::string_view id)
{
    for (const auto &source : training_source_labels)
    {
        if (source.id == id)
        {
            return i18n::text(source.label);
        }
    }
    // A weapon mastery group is its class's, at the fourth level or the first.
    for (const std::string_view mastery : {":weapon_mastery:4", ":weapon_mastery"})
    {
        if (id.ends_with(mastery))
        {
            return training_source(id.substr(0, id.size() - mastery.size())) + " / " +
                   i18n::text(N_("Weapon Mastery"));
        }
    }
    if (id.starts_with("species:") && id.ends_with(":kept"))
    {
        return i18n::text(N_("Kept from the former body"));
    }
    return training_string(id);
}

godot::String training_sources(const std::vector<opengold::rules::FeatureGrant> &grants)
{
    godot::String result;
    for (const auto &g : grants)
    {
        if (!result.is_empty())
        {
            result += ", ";
        }
        result += training_source(g.source_id);
    }
    return result;
}
} // namespace

godot::String training_summary(const opengold::rules::TrainingProfile &profile, bool fixed)
{
    godot::String text;
    if (!fixed)
    {
        text = "\n\n[theme_title]" + i18n::text(N_("Training")) + "[/theme_title]\n" +
               i18n::text(profile.complete ? N_("Supported training choices complete")
                                           : N_("Training choices pending")) +
               "\n";
    }
    for (const auto &s : profile.skills)
    {
        if (fixed && !s.proficient)
        {
            continue;
        }
        text += i18n::text(s.label);
        if (!fixed)
        {
            text +=
                " " + godot::String(s.bonus >= 0 ? "+" : "") + godot::String::num_int64(s.bonus);
            if (s.expertise)
            {
                text += " / " + i18n::text(N_("Expertise"));
            }
            else if (s.proficient)
            {
                text += " / " + i18n::text(N_("Proficient"));
            }
        }
        const auto sources = training_sources(s.sources);
        if (!sources.is_empty())
        {
            text += " (" + sources + ")";
        }
        text += "\n";
    }
    for (const auto &m : profile.masteries)
    {
        text += i18n::text(N_("Weapon Mastery")) + ": " + i18n::text(m.label) + " (" +
                training_sources(m.sources) + ")\n";
    }
    return text;
}

namespace
{
// What choosing a training option gives: a skill's check bonus once chosen,
// or what a weapon's mastery property does.
godot::String training_benefit(const opengold::rules::TrainingChoiceGroup &group,
                               const opengold::rules::CreationChoice &option,
                               const opengold::rules::CharacterSheet &sheet)
{
    const auto &skills = sheet.training.skills;
    const auto skill = std::find_if(skills.begin(), skills.end(),
                                    [&](const auto &s)
                                    {
                                        return s.id == option.id;
                                    });
    if (skill != skills.end())
    {
        const int proficiency = 2 + (sheet.level - 1) / 4;
        const bool expertise = group.id.ends_with(":expertise");
        const bool gained = expertise ? !skill->expertise : !skill->proficient;
        const auto name = i18n::text(skill->label);
        const auto chosen = signed_bonus(skill->bonus + (gained ? proficiency : 0));
        if (!gained)
        {
            return name + godot::String(" ") + chosen;
        }
        return i18n::text(expertise ? N_("{skill} {bonus} with Expertise (now {current})")
                                    : N_("{skill} {bonus} if chosen (now {current})"))
            .replace("{skill}", name)
            .replace("{bonus}", chosen)
            .replace("{current}", signed_bonus(skill->bonus));
    }
    if (const auto *effect = mastery_effect(option.description))
    {
        return i18n::text(option.description) + godot::String(": ") + i18n::text(effect);
    }
    return i18n::text(option.description);
}
} // namespace

void refresh_training_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                               const godot::Callable &toggled,
                               const opengold::rules::TrainingChoices &locked, bool species)
{
    using namespace godot;
    const String container = species ? "SpeciesChoices" : "Training";
    auto *rows = &required_node<VBoxContainer>(parent, container + String("/Rows"));
    if (!species)
    {
        // What the character has without choosing: its spell picks go with
        // the training choices, as Magic Initiate's come from one.
        auto fixed = creator.draft();
        fixed.training.clear();
        fixed.spells.reset();
        fixed.cantrips.reset();
        required_node<Label>(parent, "TrainingFixedTitle")
            .set_text(i18n::text(N_("Fixed training")));
        required_node<RichTextLabel>(parent, "TrainingFixed")
            .call("render",
                  training_summary(creator.rules()
                                       .evaluate(fixed, opengold::rules::NameRequirement::optional)
                                       .training,
                                   true));
    }
    auto groups = creator.training_options();
    std::erase_if(groups,
                  [&](const auto &group)
                  {
                      return group.id.starts_with("species:") != species;
                  });
    // The race page comes before the scores are rolled, so a species choice
    // shows its own description rather than what it adds to the sheet.
    const auto sheet =
        species ? std::optional<opengold::rules::CharacterSheet>{} : std::optional(creator.sheet());
    for (auto i = static_cast<unsigned>(groups.size());
         i < static_cast<unsigned>(rows->get_child_count()); ++i)
    {
        required_node<Control>(*rows, String("Group") + String::num_uint64(i)).hide();
    }
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
        title->set_text(i18n::text(group.label) + " (" + String::num_uint64(picked.size()) + " / " +
                        String::num_uint64(group.count) + ")");
        // A reused group can hold another class's options.
        for (int j = 0; j < box->get_child_count(); ++j)
        {
            auto *row = Object::cast_to<Control>(box->get_child(j));
            const String row_name = row->get_name();
            const bool used =
                row_name == "TitleSlot" || row_name == "TitleSpacer" ||
                std::any_of(group.options.begin(), group.options.end(),
                            [&](const auto &o)
                            {
                                const auto id = training_string(o.id);
                                return row_name == id || row_name == id + String("Benefit");
                            });
            if (used)
            {
                continue;
            }
            row->hide();
            // A hidden row left in the radio group, still ticked, is unticked
            // when the player picks a shown radio, and that tells the game to
            // drop a choice from a group this character does not have.
            if (auto *check = Object::cast_to<CheckBox>(row))
            {
                check->set_disabled(true);
                check->set_button_group(Ref<ButtonGroup>());
                check->set_pressed_no_signal(false);
            }
        }
        // A single choice is a set of radio buttons sharing one group.
        const bool single =
            group.control == opengold::rules::TrainingChoiceControl::single_selection;
        if (single && !box->has_meta("radios"))
        {
            box->set_meta("radios", Ref<ButtonGroup>(memnew(ButtonGroup)));
        }
        const Ref<ButtonGroup> radios =
            single ? Ref<ButtonGroup>(box->get_meta("radios")) : Ref<ButtonGroup>();
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
            check->set_text(i18n::text(option.label));
            check->set_pressed_no_signal(selected);
            check->set_disabled(
                (has_locked && std::find(original->second.begin(), original->second.end(),
                                         option.id) != original->second.end()) ||
                (!single && !selected && picked.size() >= group.count));
            check->show();
            check->set_tooltip_text(training_source(group.id));
            benefit->set_text(sheet                        ? training_benefit(group, option, *sheet)
                              : option.description.empty() ? String()
                                                           : i18n::text(option.description));
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
            {
                choices.push_back(choice);
            }
        }
    }
    real_t widest = 0;
    for (auto *choice : choices)
    {
        choice->set_custom_minimum_size({0, choice->get_custom_minimum_size().y});
        widest = std::max(widest, choice->get_combined_minimum_size().x);
    }
    for (auto *choice : choices)
    {
        choice->set_custom_minimum_size({widest, choice->get_custom_minimum_size().y});
    }

    String signature;
    for (const auto &group : groups)
    {
        signature += training_string(group.id) + ";";
    }
    auto &scroll = required_node<ScrollContainer>(parent, container);
    if (scroll.has_method("reset_for"))
    {
        scroll.call("reset_for", signature);
    }
    unsigned first = 0;
    for (unsigned i = 0; i < groups.size(); ++i)
    {
        if (groups[i].control == opengold::rules::TrainingChoiceControl::single_selection)
        {
            rows->move_child(
                &required_node<GridContainer>(*rows, String("Group") + String::num_uint64(i)),
                first++);
        }
    }
}
} // namespace presentation
