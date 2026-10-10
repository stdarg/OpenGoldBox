#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

// Fleeing a fight, as the original game had it: a party member runs off the
// edge of the field. Faster than every conscious enemy, it gets away; as fast
// as the fastest, it has an even chance and otherwise stays to the end; slower
// than any, it cannot leave. One who got away is out of the fight; when no
// party member is left on the field, the party has fled.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Harmless creatures that differ only in speed.
std::unique_ptr<RulesModule> module()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature runner 10 30 0 30 0 1 2 0 0 1 2 0 80 320 0 0 0 1 0\n"
                               "creature slow 10 30 0 25 0 1 2 0 0 1 2 0 80 320 0 0 0 1 0\n"
                               "creature even 10 30 0 30 0 1 2 0 0 1 2 0 80 320 0 0 0 1 0\n"
                               "creature fast 10 30 0 40 0 1 2 0 0 1 2 0 80 320 0 0 0 1 0\n");
}

// The runner stands on the field's west edge; the enemy is far to the east
// unless `beside`.
std::unique_ptr<CombatSession> fight(const RulesModule &rules, std::string_view enemy,
                                     std::uint64_t seed, bool second_member = false,
                                     bool beside = false)
{
    Encounter e{{8, 4, std::vector<Terrain>(32)},
        {{1, "runner", "Runner", 0, {0, 1}}, {9, std::string(enemy), "Enemy", 1, {beside ? 1 : 7, 1}}}};
    if (second_member)
        e.participants.push_back({2, "runner", "Stayer", 0, {3, 3}});
    return rules.create(std::move(e), seed);
}

std::optional<Command> offered(const CombatSession &c, std::string_view verb, EntityId actor)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.actor == actor)
            return command;
    return std::nullopt;
}

// Ends turns until `id` acts.
void turn_of(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id; ++turns)
    {
        check(turns < 12, "Reach the wanted turn");
        const auto end = offered(c, "end", c.snapshot().actor);
        check(end && c.submit(*end), "End the turn");
    }
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

const CombatantView &unit(const Snapshot &s, EntityId id)
{
    return *std::find_if(s.combatants.begin(), s.combatants.end(), [&](const auto & u)
    {
        return u.id == id;
    });
}

void faster_party_gets_away()
{
    const auto rules = module();
    auto c = fight(*rules, "slow", 1);
    turn_of(*c, 1);
    const auto flee = offered(*c, "flee", 1);
    check(flee && flee->destination == Cell{-1, 1}, "On the edge, a faster member can run off it");
    check(c->submit(*flee) && logged(*c, "Runner flees the battle."), "The runner gets away");
    const auto s = c->snapshot();
    check(s.outcome == Outcome::fled, "With nobody left on the field, the party has fled");
    check(unit(s, 1).fled && unit(s, 1).hit_points == 30 && !unit(s, 1).persistent.dead,
          "One who fled keeps its vitals");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A fled fight survives a checkpoint");
}

void slower_party_stays()
{
    const auto rules = module();
    auto c = fight(*rules, "fast", 1);
    turn_of(*c, 1);
    check(!offered(*c, "flee", 1), "A member slower than an enemy cannot run off the field");
}

void even_speed_is_a_coin_toss()
{
    const auto rules = module();
    bool stayed = false, escaped = false;
    for (std::uint64_t seed = 1; seed < 40 && !(stayed && escaped); ++seed)
    {
        auto c = fight(*rules, "even", seed);
        turn_of(*c, 1);
        check(c->submit(*offered(*c, "flee", 1)), "An as-fast member may try to run off");
        if (logged(*c, "Runner cannot get away and must stay."))
        {
            stayed = true;
            check(c->snapshot().outcome == Outcome::ongoing, "The fight goes on");
            const auto end = offered(*c, "end", 1);
            check(end && c->submit(*end), "The runner ends its turn");
            turn_of(*c, 1);
            check(!offered(*c, "flee", 1), "A member who failed must stay to the end");
        }
        else if (c->snapshot().outcome == Outcome::fled)
            escaped = true;
    }
    check(stayed && escaped, "As fast as the fastest enemy, a member gets away about half the time");
}

void fight_goes_on_without_one_who_fled()
{
    const auto rules = module();
    auto c = fight(*rules, "slow", 1, true);
    turn_of(*c, 1);
    check(c->submit(*offered(*c, "flee", 1)), "The runner gets away");
    const auto s = c->snapshot();
    check(s.outcome == Outcome::ongoing && s.actor != 1 && unit(s, 1).fled,
          "The fight goes on without the runner");
    for (unsigned turns = 0; turns < 6; ++turns)
    {
        check(c->snapshot().actor != 1, "One who fled takes no more turns");
        const auto end = offered(*c, "end", c->snapshot().actor);
        check(end && c->submit(*end), "Turns pass");
    }
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A fight with one fled survives a checkpoint");
}

void leaving_reach_provokes()
{
    const auto rules = module();
    auto c = fight(*rules, "slow", 2, false, true);
    turn_of(*c, 1);
    check(c->submit(*offered(*c, "flee", 1)) && c->snapshot().reaction_pending,
          "Running off beside an enemy provokes an opportunity attack");
}
// The Flee button's policy: a member fast enough runs for the nearest edge,
// Dashing when it must, and steps off; a slower one fights on as the demo AI
// would.
void flee_policy()
{
    const auto rules = module();
    {
        Encounter e{{16, 4, std::vector<Terrain>(64)},
            {{1, "runner", "Runner", 0, {8, 1}}, {9, "slow", "Enemy", 1, {15, 3}}}};
        auto c = rules->create(std::move(e), 1);
        for (unsigned n = 0; n < 200 && c->snapshot().outcome == Outcome::ongoing; ++n)
            check(c->submit(choose_flee_command(*c)), "The flee policy's command is legal");
        check(c->snapshot().outcome == Outcome::fled && logged(*c, "Runner flees the battle."),
              "A member fast enough runs to the edge and off the field");
    }
    auto c = fight(*rules, "fast", 1);
    turn_of(*c, 1);
    const auto policy = choose_flee_command(*c), demo = choose_demo_command(*c);
    check(policy.verb == demo.verb && policy.target == demo.target &&
          policy.destination == demo.destination,
          "A member too slow to flee fights on as the demo AI would");
}
// Monster morale, the original's rule: once its side has lost more of its Hit
// Points than the encounter's morale allows, a creature flees in panic when no
// opponent is faster, surrenders when one is and it is not witless, and fights
// on otherwise. Its own morale, if any, holds it while its wounds stay within it.
struct MoraleFight
{
    std::unique_ptr<CombatSession> combat;
    bool wounded{};
};

MoraleFight morale_fight(const RulesModule &rules, std::string_view enemy, unsigned morale,
                         unsigned own = 0, unsigned intelligence = 10)
{
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        Encounter e{{8, 4, std::vector<Terrain>(32)},
            {{1, "runner", "Runner", 0, {0, 1}}, {9, std::string(enemy), "Enemy", 1, {1, 1}}}};
        e.participants[1].morale = own;
        e.participants[1].intelligence = intelligence;
        e.morale = morale;
        auto c = rules.create(std::move(e), seed);
        turn_of(*c, 1);
        const auto melee = offered(*c, "melee", 1);
        check(melee && c->submit(*melee), "The runner strikes");
        if (unit(c->snapshot(), 9).hit_points < 30)
        {
            const auto end = offered(*c, "end", 1);
            check(end && c->submit(*end), "The runner ends its turn");
            return {std::move(c), true};
        }
    }
    throw std::runtime_error("The runner never hits");
}

// A broken creature as fast as its foe that fails to get away is cornered:
// it fights on instead of running for the edge every turn.
void cornered_monster_fights()
{
    const auto rules = module();
    for (std::uint64_t seed = 1; seed < 80; ++seed)
    {
        Encounter e{{8, 4, std::vector<Terrain>(32)},
            {{1, "runner", "Runner", 0, {0, 1}}, {9, "even", "Enemy", 1, {1, 1}}}};
        e.morale = 1;
        auto c = rules->create(std::move(e), seed);
        turn_of(*c, 1);
        const auto melee = offered(*c, "melee", 1);
        check(melee && c->submit(*melee), "The runner strikes");
        if (unit(c->snapshot(), 9).hit_points == 30)
            continue;
        for (unsigned n = 0; n < 80 && c->snapshot().outcome == Outcome::ongoing; ++n)
            check(c->submit(choose_demo_command(*c)), "The AI's command is legal");
        const auto log = c->snapshot().log();
        const auto stayed = std::find(log.begin(), log.end(), "Enemy cannot get away and must stay.");
        if (stayed == log.end())
            continue;
        check(std::any_of(stayed, log.end(), [](const auto & line)
        {
            return line.starts_with("Enemy -> Runner");
        }), "Cornered, the broken creature attacks again");
        check(!unit(c->snapshot(), 9).panicked, "Cornered, it is no longer fleeing in panic");
        return;
    }
    throw std::runtime_error("No broken creature failed to get away");
}

void monsters_break()
{
    const auto rules = module();
    {
        auto [c, wounded] = morale_fight(*rules, "fast", 1);
        check(logged(*c, "Enemy flees in panic.") && unit(c->snapshot(), 9).panicked,
              "A wounded side below its morale flees when no opponent is faster");
        const auto conditions = unit(c->snapshot(), 9).conditions;
        check(std::any_of(conditions.begin(), conditions.end(), [](const auto & condition)
        {
            return condition.source == "Fleeing in panic";
        }), "A panicked creature shows it among its conditions");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A panicked creature survives a checkpoint");
        for (unsigned n = 0; n < 60 && c->snapshot().outcome == Outcome::ongoing; ++n)
            check(c->submit(choose_demo_command(*c)), "The AI's command is legal");
        check(c->snapshot().outcome == Outcome::victory && unit(c->snapshot(), 9).fled,
              "The panicked creature runs off the field and the fight is won");
    }
    {
        auto [c, wounded] = morale_fight(*rules, "slow", 1);
        const auto s = c->snapshot();
        check(logged(*c, "Enemy surrenders.") && s.outcome == Outcome::victory &&
              unit(s, 9).surrendered && unit(s, 9).hit_points == 0,
              "Outpaced, a broken creature surrenders and counts as defeated");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A surrender survives a checkpoint");
    }
    {
        auto [c, wounded] = morale_fight(*rules, "slow", 1, 0, 3);
        check(!logged(*c, "surrenders") && !logged(*c, "panic") &&
              c->snapshot().outcome == Outcome::ongoing,
              "A witless creature that cannot run fights on");
    }
    {
        auto [c, wounded] = morale_fight(*rules, "fast", 100);
        check(!logged(*c, "panic"), "An encounter morale of 100 never breaks");
    }
    {
        auto [c, wounded] = morale_fight(*rules, "fast", 1, 100);
        check(!logged(*c, "panic"), "A creature's own morale holds it while its wounds stay within it");
    }
}
} // namespace

int main()
{
    try
    {
        faster_party_gets_away();
        slower_party_stays();
        even_speed_is_a_coin_toss();
        fight_goes_on_without_one_who_fled();
        leaving_reach_provokes();
        flee_policy();
        monsters_break();
        cornered_monster_fights();
        std::cout << "Flee tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
