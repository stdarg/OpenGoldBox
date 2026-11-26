#ifndef OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#define OPENGOLDBOX_COMBAT_WEAPON_CONTROLS_H
#include "opengold/rules.h"
#include <godot_cpp/variant/callable.hpp>
#include <vector>

namespace godot
{
class Node;
}

namespace presentation
{
void setup_weapon_controls(godot::Node &root, const godot::Callable &selected);

bool refresh_weapons(godot::Node &root, const opengold::rules::CombatantView *actor, bool player);

bool refresh_bonus_attacks(godot::Node &root, const opengold::rules::CombatantView *actor,
                           const std::vector<opengold::rules::Command> &offered, bool player);
} // namespace presentation
#endif
