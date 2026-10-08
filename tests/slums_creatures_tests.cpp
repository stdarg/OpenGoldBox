// The Slums set encounters' creatures (docs/audits/cluebook-check.md): the
// Hobgoblin Warrior's poisoned arrows, the Ogre, the Troll's reach, three Rends
// and Regeneration, and the level-3 magic-user.
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
    const auto log = c.snapshot().log;
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

// The Troll reaches 10 feet and its Attack action makes three Rends.
void troll_attacks()
{
    const auto module = rules();
    auto c = fight(*module, "troll", "target", {1, 1}, {3, 1});
    turn_of(*c, 1);
    check(offered(*c, "melee", 1, 2), "The Troll attacks from 10 feet away");
    check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster -> Hero") == 3,
          "The Troll makes three Rend attacks");
}

// Felled by a weapon, the Troll stays in the fight, can still be attacked and
// regenerates on its turn. The two slashers act before it (initiative +20).
void troll_regenerates()
{
    const auto module = rules();
    auto c = module->create({{10, 4, std::vector<std::uint8_t>(40)},
        {   {1, "troll", "Monster", 1, {1, 1}}, {2, "slasher", "Hero", 0, {2, 1}},
            {3, "slasher", "Ally", 0, {1, 2}}
        }},
    7);
    const auto first = c->snapshot().actor, second = first == 2 ? EntityId{3} : EntityId{2};
    check(first != 1, "A slasher acts first");
    check(submit(*c, "melee", first, 1), "The slasher attacks the Troll");
    check(unit(*c, 1).hit_points == 0 && !unit(*c, 1).dead &&
          c->snapshot().outcome == Outcome::ongoing,
          "A Troll at 0 HP is not dead and the fight goes on");
    check(submit(*c, "end", first) && c->snapshot().actor == second &&
          offered(*c, "melee", second, 1),
          "A downed Troll can still be attacked");
    check(submit(*c, "end", second), "The ally ends its turn");
    check(unit(*c, 1).hit_points == 15 && count_logged(*c, "Monster regenerates 15") == 1,
          "The Troll regains 15 HP at the start of its turn");
}

// Downed by Fire, the Troll cannot regenerate and dies at the start of its turn.
void troll_burns()
{
    const auto module = rules();
    auto c = fight(*module, "troll", "burner");
    turn_of(*c, 2);
    check(submit(*c, "melee", 2, 1) && unit(*c, 1).hit_points == 0, "Fire fells the Troll");
    const auto checkpoint = c->save();
    check(checkpoint.starts_with("OGCOMBAT 41 "), "Checkpoints use the current format");
    auto restored = module->restore(checkpoint);
    check(restored->save() == checkpoint, "A blocked Regeneration survives a checkpoint");
    check(submit(*restored, "end", 2), "The burner ends its turn");
    check(unit(*restored, 1).dead && restored->snapshot().outcome == Outcome::victory &&
          count_logged(*restored, "cannot regenerate and dies") == 1,
          "The Troll dies when it cannot regenerate");
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
