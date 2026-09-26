#ifndef OPENGOLD_SRD5_ARMOR_H
#define OPENGOLD_SRD5_ARMOR_H
#include <algorithm>
#include <array>
#include <string_view>
namespace opengold::srd5::detail
{
enum class ArmorCategory
{
    light,
    medium,
    heavy,
    shield
};
struct Armor
{
    std::string_view key, label;
    ArmorCategory category;
    int base_ac, strength{};
    bool stealth_disadvantage{};
    unsigned weight_pounds{}, cost_cp{}, don_seconds{}, doff_seconds{};
    int dexterity_contribution(int modifier) const
    {
        return category == ArmorCategory::light    ? modifier
               : category == ArmorCategory::medium ? std::min(2, modifier)
                                                   : 0;
    }
};
// SRD 5.2.1 p.92. Don/doff durations are catalog metadata; #198 owns the
// campaign activity and shield Utilize action, including interruptions.
inline constexpr std::array armors{
    Armor{"padded", "Padded Armor", ArmorCategory::light, 11, 0, true, 8, 500, 60, 60},
    Armor{"leather", "Leather Armor", ArmorCategory::light, 11, 0, false, 10, 1000, 60, 60},
    Armor{"studded_leather", "Studded Leather Armor", ArmorCategory::light, 12, 0, false, 13, 4500,
          60, 60},
    Armor{"hide", "Hide Armor", ArmorCategory::medium, 12, 0, false, 12, 1000, 300, 60},
    Armor{"chain_shirt", "Chain Shirt", ArmorCategory::medium, 13, 0, false, 20, 5000, 300, 60},
    Armor{"scale_mail", "Scale Mail", ArmorCategory::medium, 14, 0, true, 45, 5000, 300, 60},
    Armor{"breastplate", "Breastplate", ArmorCategory::medium, 14, 0, false, 20, 40000, 300, 60},
    Armor{"half_plate", "Half Plate Armor", ArmorCategory::medium, 15, 0, true, 40, 75000, 300, 60},
    Armor{"ring_mail", "Ring Mail", ArmorCategory::heavy, 14, 0, true, 40, 3000, 600, 300},
    Armor{"chain_mail", "Chain Mail", ArmorCategory::heavy, 16, 13, true, 55, 7500, 600, 300},
    Armor{"splint", "Splint Armor", ArmorCategory::heavy, 17, 15, true, 60, 20000, 600, 300},
    Armor{"plate", "Plate Armor", ArmorCategory::heavy, 18, 15, true, 65, 150000, 600, 300},
    // Zero duration denotes a Utilize action, not a free equipment change.
    Armor{"shield", "Shield", ArmorCategory::shield, 2, 0, false, 6, 1000, 0, 0}};
inline const Armor *armor(std::string_view key)
{
    for (const auto &item : armors)
        if (item.key == key)
            return &item;
    return nullptr;
}
inline bool armor_trained(std::string_view klass, ArmorCategory category)
{
    const bool heavy = klass == "Fighter" || klass == "Paladin";
    const bool medium = heavy || klass == "Barbarian" || klass == "Cleric" || klass == "Ranger";
    const bool shield = medium || klass == "Druid";
    switch (category)
    {
    case ArmorCategory::heavy:
        return heavy;
    case ArmorCategory::medium:
        return medium;
    case ArmorCategory::shield:
        return shield;
    case ArmorCategory::light:
        return shield || klass == "Bard" || klass == "Rogue" || klass == "Warlock";
    }
    return false;
}
inline std::string_view armor_category_label(ArmorCategory category)
{
    switch (category)
    {
    case ArmorCategory::light:
        return "Light armor";
    case ArmorCategory::medium:
        return "Medium armor";
    case ArmorCategory::heavy:
        return "Heavy armor";
    case ArmorCategory::shield:
        return "Shield";
    }
    return {};
}
} // namespace opengold::srd5::detail
#endif
