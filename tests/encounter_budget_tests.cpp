#include "opengold/encounter_budget.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace opengold;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> bool rejects(F f)
{
    try
    {
        f();
    }
    catch (const std::invalid_argument &)
    {
        return true;
    }
    return false;
}

// SRD 5.2.1 p. 202 Moderate budgets, per character, summed over the party.
void budgets()
{
    const std::array<unsigned, 6> six_level_two{2, 2, 2, 2, 2, 2};
    check(encounter_xp_budget(six_level_two, 100) == 900,
          "Challenge 100 is the SRD's Moderate budget");
    check(encounter_xp_budget(six_level_two, 50) == 450 &&
          encounter_xp_budget(six_level_two, 200) == 1800 &&
          encounter_xp_budget(six_level_two, 0) == 0,
          "The challenge scales the Moderate budget as a percentage");
    const std::array<unsigned, 2> mixed{1, 2};
    check(encounter_xp_budget(mixed, 100) == 75 + 150,
          "A mixed-level party sums each character's own budget");
    const std::array<unsigned, 1> twentieth{20};
    check(encounter_xp_budget(twentieth, 100) == 13200, "The table runs to level 20");
    check(rejects([&]
    {
        (void)encounter_xp_budget(six_level_two, 201);
    }), "Challenge is at most 200");
    const std::array<unsigned, 1> invalid{21};
    check(rejects([&]
    {
        (void)encounter_xp_budget(invalid, 100);
    }), "Levels outside the table are rejected");
}

void fitting()
{
    const std::array<EncounterGroup, 2> goblins{{{50, 4}, {50, 12}}};
    check((fit_encounter_to_budget(goblins, 800, 16) == std::vector<unsigned> {4, 12}),
          "An encounter within both limits is unchanged");
    check((fit_encounter_to_budget(goblins, 2000, 40) == std::vector<unsigned> {4, 12}),
          "Generous limits never add monsters");
    check((fit_encounter_to_budget(goblins, 400, 16) == std::vector<unsigned> {2, 6}),
          "Every group shrinks by the same factor to fit the XP budget");
    check((fit_encounter_to_budget(goblins, 800, 8) == std::vector<unsigned> {2, 6}),
          "Every group shrinks by the same factor to fit the creature limit");
    check((fit_encounter_to_budget(goblins, 600, 4) == std::vector<unsigned> {1, 3}),
          "The tighter limit decides the factor");
    const std::array<EncounterGroup, 2> bugbear_and_kobolds{{{200, 1}, {25, 10}}};
    check((fit_encounter_to_budget(bugbear_and_kobolds, 100, 11) == std::vector<unsigned> {1, 2}),
          "Each group keeps at least one monster");
}
} // namespace

int main()
{
    try
    {
        budgets();
        fitting();
        std::cout << "Encounter budget tests passed.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
