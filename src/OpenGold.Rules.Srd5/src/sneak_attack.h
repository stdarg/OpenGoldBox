#ifndef OPENGOLD_SRD5_SNEAK_ATTACK_H
#define OPENGOLD_SRD5_SNEAK_ATTACK_H
#include "damage_roll.h"
#include <stdexcept>

namespace opengold::srd5::detail
{
// SRD 5.2.1 pp.61–62. Weapon category is distinct from attack delivery:
// throwing a Handaxe does not turn it into a Ranged weapon. The caller supplies
// net Advantage/Disadvantage and an ally other than the attacker who is within
// five feet of the target and is not Incapacitated. No sight/reach requirement
// is added to that ally clause. Hit, entitlement and per-turn use are separate
// gates owned by the eventual combat decision state machine.
struct SneakAttackContext
{
    bool weapon_attack{}, finesse{}, ranged_weapon{};
    int attack_mode{}; // -1 Disadvantage, 0 normal/cancelled, +1 Advantage.
    bool allied_distraction{};
};

inline bool sneak_attack_eligible(const SneakAttackContext &context)
{
    if (context.attack_mode < -1 || context.attack_mode > 1)
        throw std::runtime_error("Invalid net attack mode");
    return context.weapon_attack && (context.finesse || context.ranged_weapon) &&
           (context.attack_mode == 1 || (context.attack_mode == 0 && context.allied_distraction));
}

// Full source progression is data support, not authorization for unsupported
// character advancement. Pass Rogue class level, never total multiclass level.
inline DamageDice sneak_attack_dice(unsigned rogue_level)
{
    if (rogue_level < 1 || rogue_level > 20)
        throw std::runtime_error("Invalid Rogue level");
    return {int((rogue_level + 1) / 2), 6, 0};
}
} // namespace opengold::srd5::detail
#endif
