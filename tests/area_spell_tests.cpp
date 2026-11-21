// How a computer-run party member judges an area spell's aim
// (rules::judge_area_spell; docs/ORIGINAL-RULES.md, Party area spells).
#include "opengold/rules.h"
#include "test_support.h"

#include <iostream>
#include <vector>

namespace
{
using namespace opengold::rules;
using opengold::test::check;

// A loss of exactly `points` with this chance, none otherwise.
std::vector<double> loses(int points, double chance)
{
    std::vector<double> loss(static_cast<std::size_t>(points) + 1, 0.0);
    loss.front() = 1 - chance;
    loss.back() += chance;
    return loss;
}

CreatureOutlook member(int hit_points, std::vector<double> loss)
{
    CreatureOutlook creature;
    creature.ally = true;
    creature.hit_points = creature.max_hit_points = hit_points;
    creature.loss = std::move(loss);
    return creature;
}

CreatureOutlook enemy(int hit_points, std::vector<double> loss)
{
    CreatureOutlook creature;
    creature.hit_points = creature.max_hit_points = hit_points;
    creature.loss = std::move(loss);
    return creature;
}

// The user's first case: a member who would likely die blocks the spell,
// however sure the enemies' end.
void a_member_who_would_die_blocks_it()
{
    const AreaOutlook outlook{{member(10, loses(10, 0.1)), enemy(10, loses(10, 1.0))}, 1.0};
    check(!judge_area_spell(outlook, {}).cast, "A 10% chance of a member going down blocks it");
    const AreaOutlook unlikely{{member(10, loses(10, 0.04)), enemy(10, loses(10, 1.0))}, 1.0};
    check(judge_area_spell(unlikely, {}).cast, "A 4% chance is under the death limit");
}

// The user's second case: a member losing half its Hit Points, with nothing
// done to the enemies, blocks it.
void a_member_badly_hurt_for_nothing_blocks_it()
{
    const AreaOutlook outlook{{member(20, loses(10, 1.0)), enemy(10, loses(0, 1.0))}, 0.0};
    const auto judgement = judge_area_spell(outlook, {});
    check(!judgement.cast && judgement.hurts_party_badly,
          "Half a member's Hit Points lost for nothing blocks it");
}

// The user's third case: a member losing half while every enemy very likely
// dies allows it, ranked by that chance.
void a_member_badly_hurt_to_end_the_fight_allows_it()
{
    const AreaOutlook outlook{{member(20, loses(10, 1.0)), enemy(10, loses(10, 0.9))}, 0.9};
    const auto judgement = judge_area_spell(outlook, {});
    check(judgement.cast && judgement.hurts_party_badly && judgement.score == 0.9,
          "Half a member's Hit Points for a 90% end of the fight is allowed");
    const AreaOutlook less_likely{{member(20, loses(10, 1.0)), enemy(10, loses(10, 0.7))}, 0.7};
    check(!judge_area_spell(less_likely, {}).cast, "A 70% end of the fight is not enough");
}

// Lighter harm weighs the party's share lost, twice an enemy's, against the
// enemies'; with no member caught any gain will do.
void lighter_harm_is_weighed()
{
    // A member loses 10% of its Hit Points; an enemy goes down half the time.
    const AreaOutlook worth_it{{member(20, loses(2, 1.0)), enemy(10, loses(10, 0.5))}, 0.5};
    const auto judgement = judge_area_spell(worth_it, {});
    check(judgement.cast && !judgement.hurts_party_badly && judgement.cost == 2 * 0.1 &&
              judgement.gain == 0.5 && judgement.score == 0.5 - 0.2,
          "An enemy's likely fall outweighs a member's light wound");
    // A member loses 40%, an enemy a quarter of its Hit Points.
    const AreaOutlook not_worth_it{{member(20, loses(8, 1.0)), enemy(20, loses(5, 1.0))}, 0.0};
    check(!judge_area_spell(not_worth_it, {}).cast, "A light wound to an enemy is not worth more");
    AreaSpellLimits careless;
    careless.party_weight = 1.0;
    const AreaOutlook even{{member(20, loses(5, 1.0)), enemy(20, loses(5, 1.0))}, 0.0};
    check(!judge_area_spell(even, {}).cast && judge_area_spell(even, careless).cast,
          "The party weight sets how much more the party's loss counts");
    const AreaOutlook no_member{{enemy(20, loses(1, 0.1))}, 0.0};
    check(judge_area_spell(no_member, {}).cast, "With no member caught any gain will do");
    const AreaOutlook no_gain{{enemy(20, loses(0, 1.0))}, 0.0};
    check(!judge_area_spell(no_gain, {}).cast, "Nothing done to the enemies is no cast");
}

// A member disabled counts as badly hurt, and as down with an able enemy
// beside it; a member down already caught is slain.
void disabling_and_the_downed()
{
    auto asleep = member(20, {1.0});
    asleep.disabled = 0.6;
    const AreaOutlook disabled{{asleep, enemy(10, loses(10, 0.5))}, 0.5};
    check(judge_area_spell(disabled, {}).hurts_party_badly,
          "A member likely disabled is badly hurt");
    asleep.disabled = 0.1;
    asleep.enemy_beside = true;
    const AreaOutlook beside{{asleep, enemy(10, loses(10, 1.0))}, 1.0};
    check(!judge_area_spell(beside, {}).cast, "Disabled beside an able enemy counts as down");
    asleep.enemy_beside = false;
    const AreaOutlook apart{{asleep, enemy(10, loses(10, 1.0))}, 1.0};
    check(judge_area_spell(apart, {}).cast, "Lightly at risk of sleep away from enemies is fine");
    auto downed = member(20, {1.0});
    downed.hit_points = -3;
    const AreaOutlook on_the_downed{{downed, enemy(10, loses(10, 1.0))}, 1.0};
    check(!judge_area_spell(on_the_downed, {}).cast, "A member down already is slain");
}

// Dispel Magic: freeing a held member is a gain, stripping a member's Haste
// a cost, a quarter of badly hurt each.
void dispelling_is_weighed()
{
    auto held = member(20, {1.0});
    held.freed = 0.6;
    const AreaOutlook freeing{{held}, 0.0};
    const auto judgement = judge_area_spell(freeing, {});
    check(judgement.cast && judgement.gain == 2.0 * 0.5 * 0.6,
          "Freeing a held member is worth it with no enemy near");
    auto hasted = member(20, {1.0});
    hasted.helps_lost = 0.5;
    const AreaOutlook stripping{{hasted}, 0.0};
    check(!judge_area_spell(stripping, {}).cast, "Stripping a member's Haste alone is no cast");
    auto blessed_enemy = enemy(10, {1.0});
    blessed_enemy.helps_lost = 0.5;
    const AreaOutlook both{{hasted, blessed_enemy}, 0.0};
    check(!judge_area_spell(both, {}).cast,
          "A member's Haste outweighs an enemy's as likely Bless, the party counting double");
    blessed_enemy.helps_lost = 1.0;
    const AreaOutlook surer{{hasted, blessed_enemy}, 0.0};
    check(judge_area_spell(surer, {}).cast, "An enemy's surer loss of Bless is worth it");
}

// A passing aim that hurts no member badly ranks above one that does.
void aims_rank()
{
    AreaSpellJudgement safe{true, false, 0.3, 0.1, 0.2};
    AreaSpellJudgement ending{true, true, 0, 0, 0.95};
    AreaSpellJudgement better{true, false, 0.6, 0.1, 0.5};
    check(better_area_aim(safe, ending) && !better_area_aim(ending, safe),
          "An aim hurting no member badly comes first");
    check(better_area_aim(better, safe) && !better_area_aim(safe, better),
          "Among those, the greater gain less cost");
}

} // namespace

int main()
{
    try
    {
        a_member_who_would_die_blocks_it();
        a_member_badly_hurt_for_nothing_blocks_it();
        a_member_badly_hurt_to_end_the_fight_allows_it();
        lighter_harm_is_weighed();
        disabling_and_the_downed();
        dispelling_is_weighed();
        aims_rank();
        std::cout << "Area spell judgement passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
