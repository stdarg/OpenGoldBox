#ifndef OPENGOLD_EQUIPMENT_H
#define OPENGOLD_EQUIPMENT_H

#include "opengold/pool_radiance.h"
#include <cstddef>
#include <optional>
#include <string>

// An original item as carried: its record, its template and what it adds. Kept
// apart from the creature catalog so the campaign party can name equipment
// without compiling the catalog (Effective C++ Item 31).
namespace opengold::por
{
struct EquipmentBonuses
{
    // Per-item contributions. Positive to-hit/damage/save bonuses help the wearer;
    // negative AC adjustments help. No stacking or equipped-state inference.
    std::optional<int> weapon_to_hit, weapon_damage;
    std::optional<int> armor_base_ac, ac_adjustment;
    int save_bonus{};
};

// An original item's per-item contributions, read from its record and template.
[[nodiscard]] EquipmentBonuses equipment_bonuses(const ItemRecord &item, const ItemTemplate &base);

struct Equipment
{
    std::size_t index{}; // Position in this creature's MONnITM record.
    ItemRecord stored;
    ItemTemplate base;
    EquipmentBonuses bonuses;
    // Full name decoded from START.EXE when the item type has no generic label.
    std::string decoded_name;
    // The name decoded_name shows until the item is identified, without the
    // words its record hides (#262).
    std::string unidentified_name;
    // Whether every name word is known: the shop's ID or Identify reveals them.
    [[nodiscard]] bool identified() const noexcept;
    // The name as the party knows it.
    [[nodiscard]] std::string label() const;
    void identify() noexcept;
};
} // namespace opengold::por
#endif
