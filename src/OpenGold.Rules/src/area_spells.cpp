#include "opengold/rules.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

// How a computer-run party member weighs an area spell's likely harm to its
// own side against its likely harm to the enemies (docs/ORIGINAL-RULES.md,
// Party area spells; the user's decision, 2026-11-21).
namespace opengold::rules
{
namespace
{

// The chance of losing at least `points` Hit Points.
double chance_of_losing(const std::vector<double> &loss, int points)
{
    double chance = 0;
    for (auto lost = static_cast<std::size_t>(std::max(points, 0)); lost < loss.size(); ++lost)
    {
        chance += loss[lost];
    }
    return std::min(chance, 1.0);
}

// The expected share of `whole` Hit Points lost, a loss beyond them counting
// as all of them.
double expected_share_lost(const std::vector<double> &loss, int whole)
{
    if (whole <= 0)
    {
        return 1;
    }
    double share = 0;
    for (std::size_t lost = 0; lost < loss.size(); ++lost)
    {
        share += loss[lost] * std::min(static_cast<double>(lost) / whole, 1.0);
    }
    return share;
}

// The expected share of `whole` Hit Points lost by one that keeps standing,
// that is loses fewer than all of them; 0 when it cannot stand.
double share_lost_standing(const std::vector<double> &loss, int whole)
{
    double standing = 0, share = 0;
    for (std::size_t lost = 0; lost < loss.size() && static_cast<int>(lost) < whole; ++lost)
    {
        standing += loss[lost];
        share += loss[lost] * static_cast<double>(lost) / whole;
    }
    return standing > 0 ? share / standing : 0;
}

// A chance that counts: above nothing and at least the limit. A limit of 0
// is so met by any chance at all, never by none.
bool reaches(double chance, double limit)
{
    return chance > 0 && chance >= limit;
}

} // namespace

AreaSpellJudgement judge_area_spell(const AreaOutlook &outlook, const AreaSpellLimits &limits)
{
    // A spell effect lost or gained counts as a quarter of being badly hurt;
    // being disabled, or freed of what disables, as all of it.
    const double effect_share = limits.hurt_share / 4;
    AreaSpellJudgement judgement;
    bool badly_hurt = false;
    for (const auto &creature : outlook.creatures)
    {
        const double felled = chance_of_losing(creature.loss, creature.hit_points);
        if (creature.ally)
        {
            // A member down already is slain; one disabled beside an able
            // enemy as good as down.
            const double down =
                creature.hit_points <= 0
                    ? 1.0
                    : std::min(felled + (creature.enemy_beside ? creature.disabled : 0.0), 1.0);
            if (reaches(down, limits.death_limit))
            {
                return {};
            }
            const int hurt_points = std::max(
                static_cast<int>(std::ceil(limits.hurt_share * creature.max_hit_points)), 1);
            const double hurt =
                std::min(chance_of_losing(creature.loss, hurt_points) + creature.disabled, 1.0);
            badly_hurt = badly_hurt || reaches(hurt, limits.hurt_chance);
            judgement.cost += limits.party_weight *
                              (expected_share_lost(creature.loss, creature.max_hit_points) +
                               limits.hurt_share * creature.disabled +
                               effect_share * (creature.helps_lost + creature.hindrances_gained));
            judgement.gain += limits.party_weight * (effect_share * creature.hindrances_lost +
                                                     limits.hurt_share * creature.freed);
            continue;
        }
        const double down = std::min(felled + creature.disabled, 1.0);
        judgement.gain +=
            creature.weight *
            (down + (1 - down) * share_lost_standing(creature.loss, creature.hit_points) +
             effect_share * (creature.helps_lost + creature.hindrances_gained) -
             effect_share * creature.hindrances_lost - limits.hurt_share * creature.freed);
    }
    if (badly_hurt)
    {
        judgement.hurts_party_badly = true;
        judgement.score = outlook.every_enemy_down;
        judgement.cast = outlook.every_enemy_down >= limits.fight_end_chance;
        return judgement;
    }
    // With no member caught the cost is none, and any gain will do.
    judgement.score = judgement.gain - judgement.cost;
    judgement.cast = judgement.gain > 0 && judgement.gain >= judgement.cost;
    return judgement;
}

bool better_area_aim(const AreaSpellJudgement &first, const AreaSpellJudgement &second) noexcept
{
    if (first.hurts_party_badly != second.hurts_party_badly)
    {
        return !first.hurts_party_badly;
    }
    return first.score > second.score;
}

} // namespace opengold::rules
