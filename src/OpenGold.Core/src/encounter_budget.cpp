#include "opengold/encounter_budget.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace opengold
{
namespace
{
// Moderate XP Budget per Character, SRD 5.2.1 p. 202, levels 1-20.
constexpr std::array<unsigned, 20> moderate_budgets{75,   150,  225,  375,  750,  1000, 1300,
        1700, 2000, 2300, 2900, 3700, 4200, 4900,
        5400, 6100, 7200, 8700, 10700, 13200};
} // namespace

unsigned encounter_xp_budget(std::span<const unsigned> character_levels, unsigned challenge)
{
    if (challenge > max_encounter_challenge)
        throw std::invalid_argument("Encounter challenge is 0-200");
    unsigned total = 0;
    for (const auto level : character_levels)
    {
        if (level < 1 || level > moderate_budgets.size())
            throw std::invalid_argument("Character level is 1-20");
        total += moderate_budgets[level - 1];
    }
    return total * challenge / 100;
}

std::vector<unsigned> fit_encounter_to_budget(std::span<const EncounterGroup> groups,
        unsigned budget, unsigned max_creatures)
{
    unsigned long long xp = 0;
    unsigned creatures = 0;
    for (const auto &group : groups)
    {
        xp += 1ULL * group.xp * group.count;
        creatures += group.count;
    }
    // The tighter of the two limits sets the one factor every group shrinks by.
    unsigned long long numerator = 1, denominator = 1;
    if (xp > budget)
    {
        numerator = budget;
        denominator = xp;
    }
    if (creatures > max_creatures && 1ULL * max_creatures * denominator < numerator * creatures)
    {
        numerator = max_creatures;
        denominator = creatures;
    }
    std::vector<unsigned> counts;
    for (const auto &group : groups)
        counts.push_back(numerator == denominator
                         ? group.count
                         : std::max(1U, unsigned(group.count * numerator / denominator)));
    return counts;
}
} // namespace opengold
