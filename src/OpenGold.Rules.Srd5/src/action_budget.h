#ifndef OPENGOLD_SRD5_ACTION_BUDGET_H
#define OPENGOLD_SRD5_ACTION_BUDGET_H

namespace opengold::srd5::detail
{
// Keep a restricted Action Surge allowance separate from the ordinary action.
// Spend the restricted allowance first when possible, preserving Magic access.
struct ActionBudget
{
    bool normal{true}, surge{};

    bool available(bool magic = false) const
    {
        return normal || (!magic && surge);
    }

    bool spend(bool magic = false)
    {
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
};
} // namespace opengold::srd5::detail
#endif
