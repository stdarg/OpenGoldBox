#ifndef OPENGOLD_SRD5_DAMAGE_ROLL_H
#define OPENGOLD_SRD5_DAMAGE_ROLL_H
#include "dice.h"
#include <algorithm>

namespace opengold::srd5::detail
{
struct DamageDice
{
    int count{}, sides{}, bonus{};
};
enum class DamageDieRule
{
    normal,
    great_weapon_fighting
};
// SRD 5.2.1 p. 88. Apply only after the caller establishes eligibility and
// chooses to use the feat. This changes a die's value, never draws another die.
inline int damage_die_value(int rolled, DamageDieRule rule)
{
    return rule == DamageDieRule::great_weapon_fighting && rolled <= 2 ? 3 : rolled;
}
// Dice and modifiers are validated by the owning rules profile. Critical hits
// double dice, not the flat modifier. Keep components signed until the whole
// attack is assembled; the public complete-roll helper retains its zero floor.
inline int roll_damage_component(std::uint64_t &state, DamageDice dice, bool critical = false,
                                 DamageDieRule rule = DamageDieRule::normal)
{
    int total = dice.bonus;
    for (int i = 0; i < dice.count * (critical ? 2 : 1); ++i)
        total += damage_die_value(roll_die(state, dice.sides), rule);
    return total;
}
inline int roll_damage(std::uint64_t &state, DamageDice dice, bool critical = false,
                       DamageDieRule rule = DamageDieRule::normal)
{
    return std::max(0, roll_damage_component(state, dice, critical, rule));
}
} // namespace opengold::srd5::detail
#endif
