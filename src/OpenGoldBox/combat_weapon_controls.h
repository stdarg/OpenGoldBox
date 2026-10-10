#ifndef OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#define OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#include "godot_nodes.h"
#include <algorithm>
#include <cctype>
#include "opengold/rules.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>

namespace presentation
{
template <class Text>
void setup_weapon_controls(godot::Node &root, Text text, const godot::Callable &selected)
{
    auto *label = add_control<godot::Label>(root, "WeaponLabel", {});
    label->set_text(text("Weapon"));
    label->hide();
    auto *choices = add_control<godot::OptionButton>(root, "Weapons", {});
    choices->hide();
    choices->set_fit_to_longest_item(false);
    choices->set_clip_text(true);
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
        for (const auto &weapon : actor->weapons)
        {
            const auto index = choices->get_item_count();
            const auto label = render(weapon.label);
            choices->add_item(label, weapon.item);
            choices->set_item_tooltip(index, label);
            choices->set_item_disabled(index, !weapon.available);
            enabled |= weapon.available && weapon.item != actor->selected_weapon;
            if (weapon.item == actor->selected_weapon)
                choices->select(index);
        }
    choices->set_disabled(!player || !enabled);
    return changed;
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
        const auto wild_shape_label = [](const std::string & verb)
        {
            std::string form = verb.substr(11);
            for (std::size_t i = 0; i < form.size(); ++i)
                if (form[i] == '_')
                    form[i] = ' ';
                else if (i == 0 || form[i - 1] == ' ')
                    form[i] = char(std::toupper(static_cast<unsigned char>(form[i])));
            return "Wild Shape: " + form;
        };
        // Metamagic and Wild Shape labels carry their option, cost or form, so
        // they come from the offered command.
        const auto offered_label = [&](const std::string & verb,
                                       const std::string & fallback) -> std::string
        {
            for (const auto &c : offered)
                if (c.actor == actor->id && c.verb == verb)
                    return c.label;
            return fallback;
        };
        for (const auto &verb : actor->bonus_actions)
            options.push_back({0,
                               verb,
        {
            verb == "metamagic_cancel"     ? "Metamagic: cancel"
            : verb.starts_with("metamagic_") ? offered_label(verb, "Metamagic")
            : verb.starts_with("wild_shape_") ? offered_label(verb, wild_shape_label(verb))
            : verb == "leave_wild_shape"  ? "Leave Wild Shape"
            : verb == "cunning_dash"        ? "Dash"
            : verb == "cunning_disengage" ? "Disengage"
            : verb == "lay_on_hands"      ? "Lay On Hands"
            : verb == "rage"              ? "Rage"
            : verb == "innate_sorcery"    ? "Innate Sorcery"
            : verb == "bardic_inspiration" ? "Bardic Inspiration"
            : verb == "create_slot_1"     ? "Font of Magic: create a level-1 slot (2 Sorcery Points)"
            : verb == "create_slot_2"     ? "Font of Magic: create a level-2 slot (3 Sorcery Points)"
            : verb == "convert_slot_1"    ? "Font of Magic: a level-1 slot into 1 Sorcery Point"
            : verb == "convert_slot_2"    ? "Font of Magic: a level-2 slot into 2 Sorcery Points"
            : verb == "martial_arts"      ? "Unarmed Strike"
            : verb == "flurry_of_blows"   ? "Flurry of Blows"
            : verb == "flurry_addle"      ? "Flurry of Blows: Addle"
            : verb == "flurry_push"       ? "Flurry of Blows: Push"
            : verb == "flurry_topple"     ? "Flurry of Blows: Topple"
            : verb == "patient_defense"   ? "Patient Defense: Disengage"
            : verb == "patient_defense_focus" ? "Patient Defense: Disengage and Dodge (1 Focus)"
            : verb == "step_of_the_wind"  ? "Step of the Wind: Dash"
            : verb == "step_of_the_wind_focus" ? "Step of the Wind: Disengage and Dash (1 Focus)"
            : verb == "extend_rage"       ? "Extend Rage"
            : verb == "divine_smite_free" ? "Divine Smite (Paladin's Smite)"
            : verb == "divine_smite"      ? "Divine Smite"
            : verb == "searing_smite"     ? "Searing Smite"
            : verb == "ensnaring_strike"  ? "Ensnaring Strike"
            : verb == "steady_aim"        ? "Steady Aim"
            : offered_label(verb, verb),
            {}
        },
        std::any_of(offered.begin(), offered.end(),
                    [&](const auto & c)
        {
            return c.actor == actor->id && c.verb == verb;
        })});
        options.insert(options.end(), actor->light_attacks.begin(), actor->light_attacks.end());
    }
    auto *choices = &required_node<godot::OptionButton>(root, "CunningAction");
    choices->set_fit_to_longest_item(false);
    choices->set_clip_text(true);
    const bool changed = choices->is_visible() != !options.empty();
    for (const char *name :
            {"CunningActionLabel", "CunningAction", "UseCunningAction"
            })
        required_node<godot::Control>(root, name).set_visible(!options.empty());
    required_node<godot::Label>(root, "CunningActionLabel").set_text(text("Bonus Action"));
    required_node<godot::Button>(root, "UseCunningAction").set_text(text("Use Bonus Action"));
    const godot::String previous =
        choices->get_selected() >= 0
        ? godot::String(choices->get_item_metadata(choices->get_selected()))
        : godot::String();
    choices->clear();
    bool any = false;
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
            choices->select(index);
    }
    if (any && (choices->get_selected() < 0 || choices->is_item_disabled(choices->get_selected())))
        for (int i = 0; i < choices->get_item_count(); ++i)
            if (!choices->is_item_disabled(i))
            {
                choices->select(i);
                break;
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
