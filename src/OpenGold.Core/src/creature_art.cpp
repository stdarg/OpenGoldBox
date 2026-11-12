#include "opengold/creature_art.h"
#include "opengold/campaign_party.h"
#include "opengold/scroll_key.h"
#include "opengold/formats.h"
#include <algorithm>
#include <utility>

namespace opengold::por
{
namespace
{
std::string wielded_key(const Creature &creature, const MonsterItem *item)
{
    if (!item || (item->source_index < 0 && item->source_type < 0))
    {
        return {};
    }
    if (item->source_index < 0)
    {
        Equipment added;
        added.stored.type = static_cast<std::uint8_t>(item->source_type);
        return equipment_conversion(added);
    }
    if (static_cast<std::size_t>(item->source_index) >= creature.equipment.size())
    {
        return {};
    }
    const auto &original = creature.equipment[static_cast<std::size_t>(item->source_index)];
    return original.stored.type == item->source_type ? equipment_conversion(original)
                                                     : std::string{};
}
} // namespace

std::vector<std::optional<Equipment>> carried_gear(const Creature &creature,
                                                   const ExportedMonster *exported)
{
    std::vector<std::optional<Equipment>> gear(creature.equipment.begin(),
                                               creature.equipment.end());
    if (!exported)
    {
        return gear;
    }
    for (std::size_t n = 0; n < gear.size(); ++n)
    {
        const bool kept = std::ranges::any_of(exported->equipment,
                                              [&](const auto &item)
                                              {
                                                  return item.source_index == static_cast<int>(n) &&
                                                         item.source_type == gear[n]->stored.type;
                                              });
        if (!kept)
        {
            gear[n].reset();
        }
    }
    return gear;
}

std::optional<CombatArt> creature_art(const std::vector<std::uint8_t> &archive, unsigned icon,
                                      const ExportedMonster *exported, bool archer)
{
    auto ready = decode_ega_combat_icon(archive, static_cast<std::uint8_t>(icon), 0);
    if (!ready)
    {
        return std::nullopt;
    }
    CombatArt art{0, std::move(ready.image), {}, {}, {}, {}};
    if (icon < 128)
    {
        if (auto action = decode_ega_combat_icon(archive, static_cast<std::uint8_t>(icon + 128), 0))
        {
            art.action = std::move(action.image);
        }
    }
    const auto *variant = exported ? readied_variant(*exported, archer) : nullptr;
    if (!variant)
    {
        return art;
    }
    const auto planted = [&](const MonsterVariant &set)
    {
        return PlantedArt{art_scale(*exported), planted_ground(set.ready)};
    };
    const auto weapon_set = [&](const MonsterVariant *set) -> std::optional<WeaponSetArt>
    {
        if (!set)
        {
            return std::nullopt;
        }
        return WeaponSetArt{set->ready, set->action, planted(*set)};
    };
    const auto attacks = attack_variants(*exported);
    return CombatArt{0,
                     variant->ready,
                     variant->action,
                     {},
                     planted(*variant),
                     OriginalArt{std::move(art.image), std::move(art.action)},
                     weapon_set(attacks.melee),
                     weapon_set(attacks.ranged),
                     weapon_set(attacks.melee_without_shield)};
}

std::vector<rules::CarriedEquipment> fighting_gear(const Creature &creature,
                                                   const ExportedMonster *exported)
{
    std::vector<rules::CarriedEquipment> gear;
    const auto carried = carried_gear(creature, exported);
    for (std::size_t n = 0; n < carried.size(); ++n)
    {
        if (!carried[n])
        {
            continue;
        }
        const auto &item = *carried[n];
        const auto key = equipment_conversion(item);
        if (key.starts_with("potion_") || key.starts_with("wand:") || rules::parse_scroll_key(key))
        {
            gear.push_back({n + 1, key, carried_quantity(item), static_cast<int>(n)});
        }
    }
    return gear;
}

rules::Participant staged_fighter(rules::EntityId id, std::string definition, std::string name,
                                  rules::Side side, const Creature &creature,
                                  const ExportedMonster *exported, Difficulty difficulty)
{
    rules::Participant fighter{id, std::move(definition), std::move(name), side, {}};
    fighter.footprint = combat_footprint(creature.stored);
    // Morale is the record's NPC byte (Pool of Radiance's 0x84): from 0x80
    // up a creature's own is (byte & 0x7F) * 2, and above 102 it has none
    // (the encounter's decides). Most monsters have none; a few never break.
    const auto byte = creature.stored.npc_flag;
    const unsigned own = byte >= 0x80 ? (byte & 0x7Fu) * 2 : 0;
    fighter.morale = own > 102 ? 0 : std::min(own, 100u);
    fighter.intelligence = std::min<unsigned>(creature.stored.abilities.intelligence, 30);
    if (exported && human_shaped(*exported))
    {
        const auto wielded = wielded_items(*exported);
        fighter.melee_weapon = wielded_key(creature, wielded.melee);
        fighter.ranged_weapon = wielded_key(creature, wielded.ranged);
        if (fighter.ranged_weapon.empty())
        {
            fighter.ranged_weapon = rules::no_ranged_weapon;
        }
        fighter.holds_shield = std::ranges::any_of(exported->equipment,
                                                   [](const MonsterItem &item)
                                                   {
                                                       return item.kind == "shield" && item.readied;
                                                   });
    }
    if (monsters_use_items(difficulty))
    {
        fighter.inventory = fighting_gear(creature, exported);
    }
    return fighter;
}
} // namespace opengold::por
