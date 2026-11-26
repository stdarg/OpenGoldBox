#ifndef OPENGOLDBOX_ALLY_ATTACK_CONTROLS_H
#define OPENGOLDBOX_ALLY_ATTACK_CONTROLS_H
#include <godot_cpp/variant/callable.hpp>

namespace godot
{
class Node;
}

namespace presentation
{
// The original's "Attack Ally" question, asked before a member under the
// player's command attacks its own side: yes and the party's companions and
// the creatures beside it turn against it.
void setup_ally_attack(godot::Node &root, const godot::Callable &attack,
                       const godot::Callable &spare, const godot::Callable &input);
} // namespace presentation
#endif
