#ifndef OPENGOLD_SRD5_THROWN_GEAR_H
#define OPENGOLD_SRD5_THROWN_GEAR_H
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// SRD 5.2.1 adventuring gear thrown in place of one attack of the Attack action
// at a creature within 20 feet, which makes a Dexterity save (DC 8 plus the
// thrower's Dexterity modifier and Proficiency Bonus). Each throw uses one up.
enum class ThrownGearEffect
{
    acid,             // 2d6 Acid damage on a failed save
    alchemists_fire,  // 1d4 Fire damage and Burning on a failed save
    oil               // Covered in oil: its next Fire damage within 1 minute deals 5 more
};

struct ThrownGear
{
    std::string_view key;
    ThrownGearEffect effect;
    unsigned cost_cp{};
    std::string_view label;
};

inline constexpr int thrown_gear_range = 20;

inline constexpr std::array thrown_gear_items
{
    ThrownGear{"acid", ThrownGearEffect::acid, 2500, "Acid"},
    ThrownGear{"alchemists_fire", ThrownGearEffect::alchemists_fire, 5000, "Alchemist's Fire"},
    ThrownGear{"oil", ThrownGearEffect::oil, 10, "Oil"},
};

inline constexpr const ThrownGear *thrown_gear(std::string_view key)
{
    for (const auto &item : thrown_gear_items)
        if (item.key == key)
            return &item;
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
