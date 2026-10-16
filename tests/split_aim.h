#ifndef OPENGOLD_TESTS_SPLIT_AIM_H
#define OPENGOLD_TESTS_SPLIT_AIM_H
#include "opengold/rules.h"

// Magic Missile's darts and Scorching Ray's rays are aimed one click at a time
// (rules 0.6.161). After the first, this aims the rest at the same creature, as
// further clicks on it would, so the spell is cast. False if an aim is refused.
inline bool finish_aiming(opengold::rules::CombatSession &c)
{
    for (unsigned n = 0; n < 8; ++n)
    {
        const auto state = c.snapshot();
        const auto &targeting = state.spell_targeting;
        // A Shield prompt pauses the cast until the target answers.
        if (state.reaction_pending || !targeting || targeting->chosen.empty() ||
            !(targeting->verb.starts_with("magic_missile") ||
              targeting->verb.starts_with("scorching_ray")))
        {
            return true;
        }
        const auto target = targeting->chosen.back();
        bool aimed = false;
        for (const auto &command : c.legal_commands())
        {
            if (command.verb == targeting->verb && command.target == target)
            {
                if (!c.submit(command))
                {
                    return false;
                }
                aimed = true;
                break;
            }
        }
        if (!aimed)
        {
            return false;
        }
    }
    return false;
}

// Submits a command and, for a split spell, aims the rest at the same creature.
inline bool submit_aimed(opengold::rules::CombatSession &c, const opengold::rules::Command &command)
{
    return c.submit(command) && finish_aiming(c);
}
#endif
