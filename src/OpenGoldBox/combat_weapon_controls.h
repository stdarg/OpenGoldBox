#ifndef OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#define OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#include "godot_nodes.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include "opengold/rules.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>

namespace presentation
{
template <class Text>
void setup_weapon_controls(godot::Node &root, Text text, const godot::Callable &selected)
{
    auto *label = &required_node<godot::Label>(root, "WeaponLabel");
    label->set_text(text("Weapon"));
    label->hide();
    auto *choices = &required_node<godot::OptionButton>(root, "Weapons");
    choices->hide();
    choices->connect("item_selected", selected);
}

template <class Render>
bool refresh_weapons(godot::Node &root, const opengold::rules::CombatantView *actor, bool player,
                     Render render)
{
    auto *choices = &required_node<godot::OptionButton>(root, "Weapons");
    const bool visible = actor && actor->weapons.size() > 1;
    const bool changed = choices->is_visible() != visible;
    required_node<godot::Control>(root, "WeaponLabel").set_visible(visible);
    choices->set_visible(visible);
    choices->clear();
    bool enabled = false;
    if (actor)
    {
        for (const auto &weapon : actor->weapons)
        {
            const auto index = choices->get_item_count();
            const auto label = render(weapon.label);
            choices->add_item(label, weapon.item);
            choices->set_item_tooltip(index, label);
            choices->set_item_disabled(index, !weapon.available);
            enabled |= weapon.available && weapon.item != actor->selected_weapon;
            if (weapon.item == actor->selected_weapon)
            {
                choices->select(index);
            }
        }
    }
    choices->set_disabled(!player || !enabled);
    return changed;
}

// A bonus action's label by its verb, for the verbs whose label is fixed; the
// rest carry their option, cost, form or slot and are labelled where offered.
struct BonusActionLabel
{
    std::string_view verb;
    std::string_view label;
};

inline constexpr std::array bonus_action_labels{
    BonusActionLabel{"metamagic_cancel", "Metamagic: cancel"},
    BonusActionLabel{"leave_wild_shape", "Leave Wild Shape"},
    BonusActionLabel{"cunning_dash", "Dash"},
    BonusActionLabel{"cunning_disengage", "Disengage"},
    BonusActionLabel{"cunning_hide", "Hide"},
    BonusActionLabel{"lay_on_hands", "Lay On Hands"},
    BonusActionLabel{"lay_on_hands_cure", "Lay On Hands: cure Poisoned (5)"},
    BonusActionLabel{"lay_on_hands_restore", "Lay On Hands: Restoring Touch (5 per condition)"},
    BonusActionLabel{"rage", "Rage"},
    BonusActionLabel{"ignite_flame_tongue", "Flame Tongue: light"},
    BonusActionLabel{"douse_flame_tongue", "Flame Tongue: douse"},
    BonusActionLabel{"innate_sorcery", "Innate Sorcery"},
    BonusActionLabel{"bardic_inspiration", "Bardic Inspiration"},
    BonusActionLabel{"create_slot_1", "Font of Magic: create a level-1 slot (2 Sorcery Points)"},
    BonusActionLabel{"create_slot_2", "Font of Magic: create a level-2 slot (3 Sorcery Points)"},
    BonusActionLabel{"convert_slot_1", "Font of Magic: a level-1 slot into 1 Sorcery Point"},
    BonusActionLabel{"convert_slot_2", "Font of Magic: a level-2 slot into 2 Sorcery Points"},
    BonusActionLabel{"martial_arts", "Unarmed Strike"},
    BonusActionLabel{"flurry_of_blows", "Flurry of Blows"},
    BonusActionLabel{"flurry_addle", "Flurry of Blows: Addle"},
    BonusActionLabel{"flurry_push", "Flurry of Blows: Push"},
    BonusActionLabel{"flurry_topple", "Flurry of Blows: Topple"},
    BonusActionLabel{"patient_defense", "Patient Defense: Disengage"},
    BonusActionLabel{"patient_defense_focus", "Patient Defense: Disengage and Dodge (1 Focus)"},
    BonusActionLabel{"step_of_the_wind", "Step of the Wind: Dash"},
    BonusActionLabel{"step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)"},
    BonusActionLabel{"step_of_the_wind_carry",
                     "Step of the Wind: Disengage and Dash, carrying an ally (1 Focus)"},
    BonusActionLabel{"extend_rage", "Extend Rage"},
    BonusActionLabel{"intimidating_presence", "Intimidating Presence"},
    BonusActionLabel{"intimidating_presence_rage", "Intimidating Presence (1 Rage)"},
    BonusActionLabel{"natures_veil", "Nature's Veil"},
    BonusActionLabel{"woodland_disengage", "Disengage (Conjure Woodland Beings)"},
    BonusActionLabel{"divine_smite_free", "Divine Smite (Paladin's Smite)"},
    BonusActionLabel{"divine_smite", "Divine Smite"},
    BonusActionLabel{"searing_smite", "Searing Smite"},
    BonusActionLabel{"shining_smite", "Shining Smite"},
    BonusActionLabel{"ensnaring_strike", "Ensnaring Strike"},
    BonusActionLabel{"steady_aim", "Steady Aim"}};

// `offered_label(verb, fallback)` is the label of the offered command with
// that verb, or `fallback`; `wild_shape_label(verb)` names a Wild Shape form.
template <class OfferedLabel, class WildShapeLabel>
std::string bonus_action_label(const std::string &verb, const OfferedLabel &offered_label,
                               const WildShapeLabel &wild_shape_label)
{
    for (const auto &fixed : bonus_action_labels)
    {
        if (verb == fixed.verb)
        {
            return std::string(fixed.label);
        }
    }
    // Metamagic and Wild Shape labels carry their option, cost or form, so
    // they come from the offered command.
    if (verb.starts_with("metamagic_"))
    {
        return offered_label(verb, "Metamagic");
    }
    if (verb.starts_with("wild_shape_"))
    {
        return offered_label(verb, wild_shape_label(verb));
    }
    if (verb.starts_with("divine_smite_"))
    {
        return "Divine Smite (level " + verb.substr(13) + " slot)";
    }
    if (verb.starts_with("shining_smite_"))
    {
        return "Shining Smite (level " + verb.substr(14) + " slot)";
    }
    return offered_label(verb, verb);
}

template <class Text, class Render>
bool refresh_bonus_attacks(godot::Node &root, const opengold::rules::CombatantView *actor,
                           const std::vector<opengold::rules::Command> &offered, bool player,
                           Text text, Render render)
{
    using namespace opengold::rules;
    std::vector<ItemAttackOption> options;
    if (actor)
    {
        // "wild_shape_giant_lizard" reads "Wild Shape: Giant Lizard" when no use
        // is left to offer it.
        const auto wild_shape_label = [](const std::string &verb)
        {
            std::string form = verb.substr(11);
            for (std::size_t i = 0; i < form.size(); ++i)
            {
                if (form[i] == '_')
                {
                    form[i] = ' ';
                }
                else if (i == 0 || form[i - 1] == ' ')
                {
                    form[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(form[i])));
                }
            }
            return "Wild Shape: " + form;
        };
        const auto offered_label = [&](const std::string &verb,
                                       const std::string &fallback) -> std::string
        {
            for (const auto &c : offered)
            {
                if (c.actor == actor->id && c.verb == verb)
                {
                    return c.label;
                }
            }
            return fallback;
        };
        for (const auto &verb : actor->bonus_actions)
        {
            const bool offered_now = std::any_of(offered.begin(), offered.end(),
                                                 [&](const auto &c)
                                                 {
                                                     return c.actor == actor->id && c.verb == verb;
                                                 });
            options.push_back({0,
                               verb,
                               {bonus_action_label(verb, offered_label, wild_shape_label), {}},
                               offered_now});
        }
        options.insert(options.end(), actor->light_attacks.begin(), actor->light_attacks.end());
    }
    auto *choices = &required_node<godot::OptionButton>(root, "CunningAction");
    const bool changed = choices->is_visible() != !options.empty();
    for (const char *name : {"CunningActionLabel", "CunningAction", "UseCunningAction"})
    {
        required_node<godot::Control>(root, name).set_visible(!options.empty());
    }
    required_node<godot::Label>(root, "CunningActionLabel").set_text(text("Bonus Action"));
    required_node<godot::Button>(root, "UseCunningAction").set_text(text("Use Bonus Action"));
    const godot::String previous =
        choices->get_selected() >= 0
            ? godot::String(choices->get_item_metadata(choices->get_selected()))
            : godot::String();
    choices->clear();
    bool any = false;
    int previous_index = -1;
    for (const auto &option : options)
    {
        const auto key = godot::String::utf8(
            (option.verb + (option.item ? "#" + std::to_string(option.item) : "")).c_str());
        const int index = choices->get_item_count();
        const auto label = render(option.label);
        choices->add_item(label);
        choices->set_item_metadata(index, key);
        choices->set_item_tooltip(index, label);
        choices->set_item_disabled(index, !option.available);
        any |= option.available;
        if (key == previous)
        {
            previous_index = index;
        }
    }
    // Select after filling: while no listed item is enabled, Godot's add_item
    // selects each new item, which would replace the choice kept above.
    choices->select(previous_index);
    if (any && (choices->get_selected() < 0 || choices->is_item_disabled(choices->get_selected())))
    {
        for (int i = 0; i < choices->get_item_count(); ++i)
        {
            if (!choices->is_item_disabled(i))
            {
                choices->select(i);
                break;
            }
        }
    }
    choices->set_disabled(!player || !any);
    const auto selected = choices->get_selected();
    choices->set_tooltip_text(selected >= 0 ? choices->get_item_text(selected) : godot::String());
    required_node<godot::Button>(root, "UseCunningAction")
        .set_disabled(!player || selected < 0 || !options[selected].available);
    return changed;
}
} // namespace presentation
#endif
