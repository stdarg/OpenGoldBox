#ifndef OPENGOLD_ENCOUNTER_BUDGET_H
#define OPENGOLD_ENCOUNTER_BUDGET_H
#include <span>
#include <vector>

namespace opengold
{
// The encounter challenge setting is a percentage of the SRD's Moderate XP budget.
inline constexpr unsigned max_encounter_challenge = 200;
inline constexpr unsigned default_encounter_challenge = 33;

// SRD 5.2.1 p. 202: the sum of each character's Moderate XP budget, scaled by the
// challenge percentage.
[[nodiscard]] unsigned encounter_xp_budget(std::span<const unsigned> character_levels,
        unsigned challenge);

struct EncounterGroup
{
    unsigned xp{}, count{};
};

// The counts after shrinking every group by one factor so the encounter's total
// XP fits the budget and its size is at most max_creatures, keeping at least one
// of each group. Never grows a group.
[[nodiscard]] std::vector<unsigned> fit_encounter_to_budget(std::span<const EncounterGroup> groups,
        unsigned budget, unsigned max_creatures);
} // namespace opengold
#endif
