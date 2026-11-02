#include "opengold/encounter_budget.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <stdexcept>

namespace opengold
{
namespace
{
// Moderate XP Budget per Character, SRD 5.2.1 p. 202, levels 1-20.
constexpr std::array<unsigned, 20> moderate_budgets{75,   150,  225,  375,  750,   1000, 1300,
                                                    1700, 2000, 2300, 2900, 3700,  4200, 4900,
                                                    5400, 6100, 7200, 8700, 10700, 13200};
} // namespace

unsigned encounter_xp_budget(std::span<const unsigned> character_levels, unsigned challenge)
{
    if (challenge > max_encounter_challenge)
    {
        throw std::invalid_argument("Encounter challenge is 0-200");
    }
    unsigned total = 0;
    for (const auto level : character_levels)
    {
        if (level < 1 || level > moderate_budgets.size())
        {
            throw std::invalid_argument("Character level is 1-20");
        }
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
    {
        counts.push_back(
            numerator == denominator
                ? group.count
                : std::max(1U, static_cast<unsigned>(group.count * numerator / denominator)));
    }
    return counts;
}

unsigned average_party_level(std::span<const unsigned> member_levels)
{
    return member_levels.empty() ? 1
                                 : std::accumulate(member_levels.begin(), member_levels.end(), 0U) /
                                       static_cast<unsigned>(member_levels.size());
}

std::vector<unsigned> fit_encounter_to_difficulty(std::span<const EncounterGroup> groups,
                                                  std::span<const unsigned> member_levels,
                                                  unsigned challenge, Difficulty difficulty)
{
    const auto strength = monster_strength(difficulty, average_party_level(member_levels));
    const auto scaled_challenge =
        std::min(challenge * strength.challenge_percent / 100, max_encounter_challenge);
    const auto members = static_cast<unsigned>(member_levels.size());
    const auto allowed = members * strength.creatures_per_ten_members / 10;
    return fit_encounter_to_budget(groups, encounter_xp_budget(member_levels, scaled_challenge),
                                   allowed - std::min(allowed, strength.creatures_fewer));
}
} // namespace opengold
