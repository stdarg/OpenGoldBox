#ifndef OPENGOLDBOX_CANTRIP_CONTROL_H
#define OPENGOLDBOX_CANTRIP_CONTROL_H
#include "opengold/character_creator.h"
#include <godot_cpp/variant/callable.hpp>

namespace godot
{
class Node;
}

namespace presentation
{
void refresh_cantrip_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                              const godot::Callable &toggled);
} // namespace presentation
#endif
