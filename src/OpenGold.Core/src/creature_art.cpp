#include "opengold/creature_art.h"
#include "opengold/campaign_party.h"
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
                     weapon_set(attacks.ranged)};
}

void arm_as_exported(rules::Participant &fighter, const Creature &creature,
                     const ExportedMonster *exported)
{
    if (!exported || !human_shaped(*exported))
    {
        return;
    }
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
} // namespace opengold::por
