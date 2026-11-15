#include "opengold/campaign_encounter.h"
#include "opengold/original_rules.h"
#include <algorithm>

namespace opengold
{
namespace
{
// Morale that already never breaks stays so; the rest may not reach it, or, for a
// creature's own morale, fall to 0, which would leave it to the encounter's.
unsigned shifted_morale(unsigned morale, int shift, int lowest)
{
    return morale >= 100
               ? morale
               : static_cast<unsigned>(std::clamp(static_cast<int>(morale) + shift, lowest, 99));
}
} // namespace

void apply_difficulty(CampaignEncounter &encounter, Difficulty difficulty, unsigned party_level)
{
    const auto strength = monster_strength(difficulty, party_level);
    encounter.morale = shifted_morale(encounter.morale, strength.morale_shift, 0);
    encounter.spares_the_party = spares_the_party(difficulty);
    for (auto &enemy : encounter.enemies)
    {
        enemy.hit_points_percent = strength.hit_points_percent;
        enemy.attack_bonus = strength.attack_bonus;
        if (enemy.morale > 0)
        {
            enemy.morale = shifted_morale(enemy.morale, strength.morale_shift, 1);
        }
    }
}

void apply_original_difficulty(CampaignEncounter &encounter, Difficulty difficulty,
                               GoldBoxGame game)
{
    const auto level = static_cast<original::DifficultyLevel>(difficulty);
    const auto strength = original_monster_strength(difficulty);
    // Secret's and the later games' Hit Points by their own levels.
    const auto hit_points_percent = original::scales_monster_hit_points(game)
                                        ? original::game_hit_points_percent(level)
                                        : strength.hit_points_percent;
    for (auto &enemy : encounter.enemies)
    {
        enemy.original_strength = {hit_points_percent, strength.to_hit, strength.armor_class_bonus};
    }
}
} // namespace opengold
