#ifndef OPENGOLDBOX_CLASS_TEXT_H
#define OPENGOLDBOX_CLASS_TEXT_H
#include "localization.h"
#include "opengold/character_rules.h"
#include <span>
#include <string>
#include <string_view>

namespace i18n
{
// A class's label by its stable ID ("rogue"), translated.
[[nodiscard]] godot::String class_label(std::string_view id);

// A character's classes as the sheet names them (MULTI-1): "Fighter 3 /
// Rogue 1" for several, in the order taken; the class's label alone for one.
[[nodiscard]] godot::String class_summary(std::span<const std::string> level_classes,
                                          const godot::String &separator = " / ");

[[nodiscard]] godot::String class_summary(const opengold::rules::CharacterSheet &sheet);
} // namespace i18n
#endif
