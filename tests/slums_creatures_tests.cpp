// The Slums set encounters' creatures (docs/audits/cluebook-check.md): the
// Hobgoblin Warrior's poisoned arrows, the Ogre, the Troll's three Rends and
// Regeneration, and the level-3 magic-user.
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

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// The shipped creatures plus test fighters: a sturdy target, a slasher that
// drops a Troll in one blow and a burner that does it with Fire.
std::unique_ptr<RulesModule> rules(std::string extra = {})
{
    return srd5::parse_content(
               read(root / "data/rules/srd-5.2.1/combat.rules") +
               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
               "creature slasher 30 1000 20 30 30 10 20 20 0 0 0 0 0 0 0 0 0 1 0\n"
               "damage_types slasher slashing slashing\n"
               "creature burner 30 1000 20 30 30 10 20 20 0 0 0 0 0 0 0 0 0 1 0\n"
               "damage_types burner fire fire\n" +
               extra);
}

std::unique_ptr<CombatSession> fight(const RulesModule &module, const std::string &monster,
                                     const std::string &hero, Cell monster_cell = {1, 1},
                                     Cell hero_cell = {2, 1})
{
    return module.create({{10, 4, std::vector<std::uint8_t>(40)},
        {{1, monster, "Monster", 1, monster_cell}, {2, hero, "Hero", 0, hero_cell}}},
    7);
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    const auto snapshot = c.snapshot();
    return *std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                         [&](const auto & u)
    {
        return u.id == id;
    });
}

bool offered(const CombatSession &c, std::string_view verb, EntityId actor, EntityId target)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb && command.actor == actor && command.target == target;
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId actor, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.actor == actor && (!target || command.target == target))
            return c.submit(command);
    return false;
}

// Ends turns until it is `id`'s.
void turn_of(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id; ++turns)
    {
        check(turns < 8, "Reach the wanted turn");
        check(submit(c, "end", c.snapshot().actor), "End the turn");
    }
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

void creatures_join()
{
    const auto module = rules();
    for (const auto *creature : {"slums-hobgoblin", "ogre", "troll", "slums-magic-user"})
        check(fight(*module, creature, "target")->snapshot().combatants.size() == 2,
              std::string(creature) + " joins a fight");
}

// As in the original game the Troll's melee reaches adjacent squares only
// (MELEE-1), and its Attack action makes three Rends.
void troll_attacks()
{
    const auto module = rules();
    auto far = fight(*module, "troll", "target", {1, 1}, {3, 1});
    turn_of(*far, 1);
    check(!offered(*far, "melee", 1, 2), "The Troll cannot strike two squares away");
    auto c = fight(*module, "troll", "target", {1, 1}, {2, 1});
    turn_of(*c, 1);
    check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster -> Hero") == 3,
          "The Troll makes three Rend attacks");
}

// A Troll (1) with a sturdy ally (4) far away, against two heroes who act first
// (initiative +20): `first` beside the Troll and `second` beside it too.
std::unique_ptr<CombatSession> troll_fight(const RulesModule &module, const std::string &first,
        const std::string &second)
{
    return module.create({{10, 4, std::vector<std::uint8_t>(40)},
        {   {1, "troll", "Monster", 1, {1, 1}}, {4, "target", "Ally", 1, {9, 3}},
            {2, first, "Hero", 0, {2, 1}}, {3, second, "Friend", 0, {1, 2}}
        }},
    7);
}

// The hero acting now and the other one.
std::pair<EntityId, EntityId> heroes(const CombatSession &c)
{
    const auto now = c.snapshot().actor;
    check(now == 2 || now == 3, "A hero acts first");
    return {now, now == 2 ? 3 : 2};
}

// Felled by a weapon while its side fights on, the Troll can still be attacked
// and regenerates on its turn.
void troll_regenerates()
{
    const auto module = rules();
    auto c = troll_fight(*module, "slasher", "slasher");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1), "A hero attacks the Troll");
    check(unit(*c, 1).hit_points == 0 && !unit(*c, 1).dead &&
          c->snapshot().outcome == Outcome::ongoing,
          "A downed Troll is not dead while its side fights on");
    check(submit(*c, "end", first) && c->snapshot().actor == second &&
          offered(*c, "melee", second, 1),
          "A downed Troll can still be attacked");
    check(submit(*c, "end", second), "The other hero ends its turn");
    check(unit(*c, 1).hit_points == 15 && count_logged(*c, "Monster regenerates 15") == 1,
          "The Troll regains 15 HP at the start of its turn");
}

// Downed by Fire, the Troll cannot regenerate and dies at the start of its turn.
void troll_burns()
{
    const auto module = rules();
    auto c = troll_fight(*module, "burner", "burner");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1) && unit(*c, 1).hit_points == 0, "Fire fells the Troll");
    const auto checkpoint = c->save();
    check(checkpoint.starts_with("OGCOMBAT 47 "), "Checkpoints use the current format");
    auto restored = module->restore(checkpoint);
    check(restored->save() == checkpoint, "A blocked Regeneration survives a checkpoint");
    check(submit(*restored, "end", first) && submit(*restored, "end", second),
          "The heroes end their turns");
    check(unit(*restored, 1).dead && count_logged(*restored, "cannot regenerate and dies") == 1,
          "The Troll dies when it cannot regenerate");
}

// As in the original game, a hero standing on a downed Troll keeps it down.
void troll_held_down()
{
    const auto module = rules();
    auto c = troll_fight(*module, "slasher", "slasher");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1) && submit(*c, "end", first), "A hero fells the Troll");
    bool moved = false;
    for (const auto &command : c->legal_commands())
        if (!moved && command.actor == second && command.verb == "move" &&
                command.destination == Cell{1, 1})
            moved = c->submit(command);
    check(moved && unit(*c, second).cell == Cell{1, 1}, "A hero stands on the downed Troll");
    const auto checkpoint = c->save();
    check(module->restore(checkpoint)->save() == checkpoint,
          "Standing on a downed Troll survives a checkpoint");
    check(submit(*c, "end", second), "The hero ends its turn");
    check(unit(*c, 1).dead, "A Troll with a hero on it cannot get up and dies");
}

// The fight ends once every enemy is down, and a downed Troll stays down.
void troll_stays_down()
{
    const auto module = rules();
    auto c = fight(*module, "troll", "slasher");
    turn_of(*c, 2);
    check(submit(*c, "melee", 2, 1), "The slasher fells the Troll");
    check(c->snapshot().outcome == Outcome::victory && unit(*c, 1).dead &&
          count_logged(*c, "Monster stays down.") == 1,
          "With every enemy down the fight is won and the Troll stays down");
}

// A Hobgoblin Warrior's arrow adds 3d4 Poison damage.
void hobgoblin_arrows()
{
    const auto module = rules();
    bool poisoned = false;
    for (unsigned attempt = 0; attempt < 10 && !poisoned; ++attempt)
    {
        auto c = fight(*module, "slums-hobgoblin", "target", {1, 1}, {8, 1});
        turn_of(*c, 1);
        check(submit(*c, "ranged", 1, 2), "The hobgoblin shoots");
        poisoned = count_logged(*c, "Poison damage") == 1;
    }
    check(poisoned, "A hobgoblin's arrow hit adds Poison damage");
    bool rejected = false;
    try
    {
        (void)rules("ranged_extra_damage troll 3 4 poison\n");
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "Extra ranged damage needs a ranged attack");
}
} // namespace

int main()
{
    try
    {
        creatures_join();
        troll_attacks();
        troll_regenerates();
        troll_burns();
        troll_held_down();
        troll_stays_down();
        hobgoblin_arrows();
        std::cout << "Slums creature tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
