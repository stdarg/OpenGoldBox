#ifndef OPENGOLDBOX_CHARACTER_COLORS_H
#define OPENGOLDBOX_CHARACTER_COLORS_H
#include <array>

namespace presentation
{
inline constexpr std::array<const char *, 16> character_colors{
    "Black",     "Blue",       "Green",     "Cyan",       "Red",         "Magenta",
    "Brown",     "Light gray", "Dark gray", "Light blue", "Light green", "Light cyan",
    "Light red", "Pink",       "Yellow",    "White"};
inline constexpr std::array<const char *, 6> character_regions{"Weapon", "Body", "Hair / Face",
                                                               "Shield", "Arms", "Legs"};

} // namespace presentation
#endif
