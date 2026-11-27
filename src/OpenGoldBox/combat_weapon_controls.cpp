#include "combat_weapon_controls.h"
#include "godot_nodes.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <algorithm>
#include <string>

namespace presentation
{
void setup_weapon_controls(godot::Node &root, const godot::Callable &selected)
{
    auto *label = &required_node<godot::Label>(root, "WeaponLabel");
    label->set_text(i18n::text("Weapon"));
    label->hide();
    auto *choices = &required_node<godot::OptionButton>(root, "Weapons");
    choices->hide();
    choices->connect("item_selected", selected);
}

bool refresh_weapons(godot::Node &root, const opengold::rules::CombatantView *actor, bool player)
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
            const auto label = i18n::render(weapon.label);
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

bool refresh_bonus_attacks(godot::Node &root, const opengold::rules::CombatantView *actor,
                           const std::vector<opengold::rules::Command> &offered, bool player)
{
    using namespace opengold::rules;
    std::vector<ItemAttackOption> options;
    if (actor)
    {
        for (const auto &action : actor->bonus_actions)
        {
            const bool offered_now =
                std::any_of(offered.begin(), offered.end(),
                            [&](const auto &c)
                            {
                                return c.actor == actor->id && c.verb == action.verb;
                            });
            options.push_back({0, action.verb, action.label, offered_now});
        }
        options.insert(options.end(), actor->light_attacks.begin(), actor->light_attacks.end());
    }
    auto *choices = &required_node<godot::OptionButton>(root, "CunningAction");
    const bool changed = choices->is_visible() != !options.empty();
    for (const char *name : {"CunningActionLabel", "CunningAction", "UseCunningAction"})
    {
        required_node<godot::Control>(root, name).set_visible(!options.empty());
    }
    required_node<godot::Label>(root, "CunningActionLabel").set_text(i18n::text("Bonus Action"));
    required_node<godot::Button>(root, "UseCunningAction").set_text(i18n::text("Use Bonus Action"));
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
        const auto label = i18n::render(option.label);
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
