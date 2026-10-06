#ifndef OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#define OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#include "godot_nodes.h"
#include <algorithm>
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
    auto *choices = root.get_node<godot::OptionButton>("Weapons");
    const bool visible = actor && actor->weapons.size() > 1;
    const bool changed = choices->is_visible() != visible;
    root.get_node<godot::Control>("WeaponLabel")->set_visible(visible);
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
        for (const auto &verb : actor->bonus_actions)
            options.push_back({0,
                               verb,
        {
            verb == "cunning_dash"        ? "Dash"
            : verb == "cunning_disengage" ? "Disengage"
            : verb == "lay_on_hands"      ? "Lay On Hands"
            : verb == "rage"              ? "Rage"
            : verb == "martial_arts"      ? "Unarmed Strike"
            : verb == "extend_rage"       ? "Extend Rage"
            : verb == "divine_smite_free" ? "Divine Smite (Paladin's Smite)"
            : verb == "divine_smite"      ? "Divine Smite"
            : verb == "searing_smite"     ? "Searing Smite"
            : verb == "ensnaring_strike"  ? "Ensnaring Strike"
            : "Steady Aim",
            {}
        },
        std::any_of(offered.begin(), offered.end(),
                    [&](const auto & c)
        {
            return c.actor == actor->id && c.verb == verb;
        })});
        options.insert(options.end(), actor->light_attacks.begin(), actor->light_attacks.end());
    }
    auto *choices = root.get_node<godot::OptionButton>("CunningAction");
    choices->set_fit_to_longest_item(false);
    choices->set_clip_text(true);
    const bool changed = choices->is_visible() != !options.empty();
    for (const char *name :
            {"CunningActionLabel", "CunningAction", "UseCunningAction"
            })
        root.get_node<godot::Control>(name)->set_visible(!options.empty());
    root.get_node<godot::Label>("CunningActionLabel")->set_text(text("Bonus Action"));
    root.get_node<godot::Button>("UseCunningAction")->set_text(text("Use Bonus Action"));
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
    root.get_node<godot::Button>("UseCunningAction")
    ->set_disabled(!player || selected < 0 || !options[selected].available);
    return changed;
}
} // namespace presentation
#endif
