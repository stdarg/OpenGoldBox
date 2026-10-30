#include "opengold/creature_art.h"
#include "opengold/formats.h"
#include <utility>

namespace opengold::por
{
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
} // namespace opengold::por
