#ifndef OPENGOLD_SRD5_AMMUNITION_H
#define OPENGOLD_SRD5_AMMUNITION_H
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// SRD 5.2.1 pp. 89, 91, 96: sling and firearm bullets are distinct supplies.
enum class Ammunition
{
    none,
    arrow,
    bolt,
    sling_bullet,
    firearm_bullet,
    needle
};

struct AmmunitionDefinition
{
    std::string_view key;
    Ammunition type;
    std::string_view label;
};

inline constexpr std::array ammunition_definitions{
    AmmunitionDefinition{"arrow", Ammunition::arrow, "Arrows"},
    AmmunitionDefinition{"bolt", Ammunition::bolt, "Bolts"},
    AmmunitionDefinition{"sling_bullet", Ammunition::sling_bullet, "Sling bullets"},
    AmmunitionDefinition{"firearm_bullet", Ammunition::firearm_bullet, "Firearm bullets"},
    AmmunitionDefinition{"needle", Ammunition::needle, "Needles"},
};

inline constexpr const AmmunitionDefinition *ammunition(std::string_view key)
{
    for (const auto &item : ammunition_definitions)
        if (item.key == key)
            return &item;
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
