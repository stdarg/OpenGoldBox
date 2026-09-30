#ifndef OPENGOLD_TEST_COMBAT_FIXTURE_H
#define OPENGOLD_TEST_COMBAT_FIXTURE_H
#include "opengold/rules.h"
#include <algorithm>
#include <stdexcept>

namespace opengold::test
{
// Unrelated mechanics explicitly decline newly introduced opening choices.
inline void keep_initiative(rules::CombatSession &session)
{
    while (!session.snapshot().initiative_choices.empty())
    {
        const auto commands = session.legal_commands();
        const auto choice = std::find_if(commands.begin(), commands.end(),
                                         [](const auto & c)
        {
            return c.verb == "initiative_keep";
        });
        if (choice == commands.end() || !session.submit(*choice))
            throw std::runtime_error("Missing Initiative decline");
    }
}

// Explicitly choose the former automatic policy in unrelated feature tests.
// Dedicated Savage Attacker tests independently cover both results and skipping.
inline unsigned choose_savage_damage(rules::CombatSession &session)
{
    unsigned choices{};
    while (const auto hit = session.snapshot().savage_attack_choice)
    {
        const auto verb = !hit->second_damage                        ? "savage_use"
                          : hit->first_damage >= *hit->second_damage ? "savage_first"
                          : "savage_second";
        bool accepted = false;
        for (const auto &command : session.legal_commands())
            if (command.verb == verb)
            {
                accepted = session.submit(command);
                break;
            }
        if (!accepted || ++choices > 2)
            throw std::runtime_error("Savage Attacker decision did not resolve");
    }
    return choices;
}
} // namespace opengold::test
#endif
