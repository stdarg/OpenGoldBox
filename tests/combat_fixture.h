#ifndef OPENGOLD_TEST_COMBAT_FIXTURE_H
#define OPENGOLD_TEST_COMBAT_FIXTURE_H
#include "opengold/rules.h"
#include <algorithm>
#include <stdexcept>
#include <string_view>

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

// Automatic combat results (Sneak Attack, Savage Attacker) are visible only in the log.
inline bool logged(const rules::CombatSession &session, std::string_view text)
{
    const auto log = session.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}
} // namespace opengold::test
#endif
