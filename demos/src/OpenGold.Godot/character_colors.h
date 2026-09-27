#ifndef OPENGOLDBOX_CHARACTER_COLORS_H
#define OPENGOLDBOX_CHARACTER_COLORS_H
#include "opengold/character_art.h"
#include <godot_cpp/variant/color.hpp>

namespace presentation
{
inline constexpr std::array<const char *, 16> character_colors
{
    "Black",     "Blue",       "Green",     "Cyan",       "Red",         "Magenta",
    "Brown",     "Light gray", "Dark gray", "Light blue", "Light green", "Light cyan",
    "Light red", "Pink",       "Yellow",    "White"};
inline constexpr std::array<const char *, 6> character_regions{"Weapon", "Body", "Hair / Face",
    "Shield", "Arms", "Legs"};

inline godot::Color character_color(unsigned index)
{
    const auto c = opengold::por::character_color(index);
    return {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f};
}
} // namespace presentation
#endif
