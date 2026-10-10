// A combat command applies completely or not at all (Effective C++ Item 29):
// whichever step of a command fails, the fight is exactly as it was before.
#include "failing_allocation.h"
#include "opengold/srd5.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<CombatSession> fight(const RulesModule &rules)
{
    Encounter encounter{{6, 3, std::vector<Terrain>(18)},
        {   {1, "bandit", "Bandit", Side::party, {1, 1}},
            {2, "bandit", "Rival", Side::opposition, {2, 1}}
        }};
    return rules.create(std::move(encounter), 7);
}

// Runs `command` with an allocation failing at every step it makes, one step
// per attempt, until it completes. Returns the number of failed attempts.
unsigned fail_at_every_step(const RulesModule &rules, std::size_t command_index)
{
    for (long failing = 0; failing < 1'000'000; ++failing)
    {
        auto session = fight(rules);
        const auto command = session->legal_commands().at(command_index);
        const auto before = session->save();
        bool failed = false;
        {
            // Every allocation from the failing one on fails, so restoring
            // the fight must not need memory either.
            const test::FailingAllocation failure(failing, test::FailureSpan::rest);
            try
            {
                (void)session->submit(command);
            }
            catch (const std::bad_alloc &)
            {
                failed = true;
            }
        }
        if (!failed)
            return static_cast<unsigned>(failing);
        check(session->save() == before, "A failed command leaves the fight unchanged");
    }
    throw std::runtime_error("A command never completed");
}

void every_opening_command_is_all_or_nothing()
{
    const auto rules = srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                                  "data/rules/srd-5.2.1/combat.rules");
    const auto commands = fight(*rules)->legal_commands().size();
    check(commands > 1, "The opening turn offers several commands");
    for (std::size_t index = 0; index < commands; ++index)
        check(fail_at_every_step(*rules, index) > 0, "Each command allocates while it runs");
}
} // namespace

int main()
{
    try
    {
        every_opening_command_is_all_or_nothing();
        std::cout << "Combat rollback tests passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
