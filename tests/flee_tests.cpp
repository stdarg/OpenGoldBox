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
    Encounter e{{8, 4, std::vector<std::uint8_t>(32)},
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
    const auto log = c.snapshot().log;
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
        std::cout << "Flee tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
