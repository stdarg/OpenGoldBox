#ifndef OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#define OPENGOLDBOX_TRAINING_REPLACEMENT_CONTROLS_H
#include "opengold/rules.h"
#include <godot_cpp/variant/callable.hpp>
#include <span>
#include <string>

namespace godot
{
class Node;
class Window;
} // namespace godot

namespace presentation
{
godot::Window *setup_training_replacement(godot::Node &parent, const godot::Callable &keep,
                                          const godot::Callable &apply);

// One section per choice the rest can change. Weapon Mastery checks several
// boxes; a single pick (the Hunter's Prey, the land) works like radio buttons.
void refresh_training_replacement(
    godot::Window &w, std::span<const opengold::rules::TrainingReplacementOptions> choices,
    std::span<const std::string> selected, const godot::Callable &toggled);
} // namespace presentation
#endif
