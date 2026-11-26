#ifndef OPENGOLDBOX_NICK_CONTROLS_H
#define OPENGOLDBOX_NICK_CONTROLS_H
#include "opengold/rules.h"
#include <godot_cpp/variant/callable.hpp>

namespace godot
{
class Node;
}

namespace presentation
{
void setup_nick(godot::Node &root, const godot::Callable &open, const godot::Callable &chosen,
                const godot::Callable &target, const godot::Callable &cancel,
                const godot::Callable &input);

void refresh_nick(godot::Node &root, const opengold::rules::CombatantView *actor,
                  bool available_turn);
} // namespace presentation
#endif
