#ifndef OPENGOLD_SRD5_ACTION_BUDGET_H
#define OPENGOLD_SRD5_ACTION_BUDGET_H

namespace opengold::srd5::detail
{
// Keep a restricted Action Surge allowance separate from the ordinary action.
// Spend the restricted allowance first when possible, preserving Magic access.
struct ActionBudget
{
    bool normal{true}, surge{};
    // Haste's action: one weapon attack, Dash, Disengage, Hide or Utilize.
    bool haste{};
    // Extra Attack: the current Attack action's attacks after its first.
    // Spending another action ends that Attack action.
    unsigned extra_attacks{};
    // Loading: a crossbow or other Loading weapon fired in this action cannot
    // fire again until another action, whatever attacks remain.
    bool loading_fired{};
    // Breath Weapon: one of the action's attacks became an exhalation.
    bool exhaled{};

    bool available(bool magic = false) const noexcept
    {
        return normal || (!magic && surge);
    }

    bool spend(bool magic = false) noexcept
    {
        extra_attacks = 0;
        loading_fired = false;
        exhaled = false;
        if (!magic && surge)
        {
            surge = false;
            return true;
        }
        if (normal)
        {
            normal = false;
            return true;
        }
        return false;
    }

    // Makes one of the Attack action's later attacks, if one is left.
    bool attack_again() noexcept
    {
        if (!extra_attacks)
        {
            return false;
        }
        --extra_attacks;
        return true;
    }
};
} // namespace opengold::srd5::detail
#endif
