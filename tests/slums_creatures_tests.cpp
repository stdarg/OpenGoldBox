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
    {
        throw std::runtime_error(message);
    }
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
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
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
    return module.create({{10, 4, std::vector<Terrain>(40)},
                          {{1, monster, "Monster", Side::opposition, monster_cell},
                           {2, hero, "Hero", Side::party, hero_cell}}},
                         7);
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    const auto snapshot = c.snapshot();
    return *std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                         [&](const auto &u)
                         {
                             return u.id == id;
                         });
}

bool offered(const CombatSession &c, std::string_view verb, EntityId actor, EntityId target)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && command.actor == actor &&
                                  command.target == target;
                       });
}

bool submit(CombatSession &c, std::string_view verb, EntityId actor, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && command.actor == actor && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
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
    return std::count_if(log.begin(), log.end(),
                         [&](const auto &line)
                         {
                             return line.find(text) != std::string::npos;
                         });
}

void creatures_join()
{
    const auto module = rules();
    for (const auto *creature : {"slums-hobgoblin", "ogre", "troll", "slums-magic-user"})
    {
        check(fight(*module, creature, "target")->snapshot().combatants.size() == 2,
              std::string(creature) + " joins a fight");
    }
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
    return module.create({{10, 4, std::vector<Terrain>(40)},
                          {{1, "troll", "Monster", Side::opposition, {1, 1}},
                           {4, "target", "Ally", Side::opposition, {9, 3}},
                           {2, first, "Hero", Side::party, {2, 1}},
                           {3, second, "Friend", Side::party, {1, 2}}}},
                         7);
}

// The hero acting now and the other one.
std::pair<EntityId, EntityId> heroes(const CombatSession &c)
{
    const auto now = c.snapshot().actor;
    check(now == 2 || now == 3, "A hero acts first");
    return {now, now == 2 ? 3 : 2};
}

// Ends turns until the downed Troll gets up; returns the round it does.
unsigned round_troll_gets_up(CombatSession &c)
{
    for (unsigned turns = 0; unit(c, 1).hit_points == 0; ++turns)
    {
        check(turns < 100 && !unit(c, 1).dead, "The Troll gets up within 3d6 rounds");
        check(submit(c, "end", c.snapshot().actor), "End the turn");
    }
    return c.snapshot().round;
}

// As in the original game, a Troll felled by a weapon while its side fights on
// stays down for 3d6 rounds, and can still be attacked; then it gets back up
// with all its Hit Points.
void troll_gets_up_later()
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
    const auto fell = c->snapshot().round;
    const auto rose = round_troll_gets_up(*c);
    check(rose >= fell + 3 && rose <= fell + 18, "The Troll stays down for 3d6 rounds");
    check(unit(*c, 1).hit_points == unit(*c, 1).max_hit_points &&
              count_logged(*c, "Monster gets back up.") == 1 &&
              count_logged(*c, "Monster regenerates") == 0,
          "The Troll gets back up with all its Hit Points");
}

// The rounds a downed Troll has left before it gets up survive a checkpoint.
void troll_delay_saved()
{
    const auto module = rules();
    auto c = troll_fight(*module, "slasher", "slasher");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1) && unit(*c, 1).hit_points == 0, "A hero fells the Troll");
    const auto checkpoint = c->save();
    check(checkpoint.starts_with("OGCOMBAT 63 "), "Checkpoints use the current format");
    auto restored = module->restore(checkpoint);
    check(restored->save() == checkpoint, "A downed Troll's delay survives a checkpoint");
    check(round_troll_gets_up(*restored) == round_troll_gets_up(*c),
          "A restored Troll gets up when the saved one does");
}

// Downed by Fire, the Troll cannot regenerate and dies at the start of its turn.
void troll_burns()
{
    const auto module = rules();
    auto c = troll_fight(*module, "burner", "burner");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1) && unit(*c, 1).hit_points == 0, "Fire fells the Troll");
    const auto checkpoint = c->save();
    check(checkpoint.starts_with("OGCOMBAT 63 "), "Checkpoints use the current format");
    auto restored = module->restore(checkpoint);
    check(restored->save() == checkpoint, "A blocked Regeneration survives a checkpoint");
    check(submit(*restored, "end", first) && submit(*restored, "end", second),
          "The heroes end their turns");
    check(unit(*restored, 1).dead && count_logged(*restored, "cannot regenerate and dies") == 1,
          "The Troll dies when it cannot regenerate");
}

// Moves `id` onto `destination`; true when it got there.
bool move_to(CombatSession &c, EntityId id, Cell destination)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.actor == id && command.verb == "move" && command.destination == destination)
        {
            return c.submit(command) && unit(c, id).cell == destination;
        }
    }
    return false;
}

// As in the original game, a hero standing on a downed Troll keeps it from
// getting up for as long as the hero stays; once the hero steps off, the Troll
// gets up on its turn.
void troll_held_down()
{
    const auto module = rules();
    auto c = troll_fight(*module, "slasher", "slasher");
    const auto [first, second] = heroes(*c);
    check(submit(*c, "melee", first, 1) && submit(*c, "end", first), "A hero fells the Troll");
    check(move_to(*c, second, {1, 1}), "A hero stands on the downed Troll");
    const auto checkpoint = c->save();
    check(module->restore(checkpoint)->save() == checkpoint,
          "Standing on a downed Troll survives a checkpoint");
    // Longer than the most a 3d6 delay can last.
    const auto stood_from = c->snapshot().round;
    while (c->snapshot().round < stood_from + 20)
    {
        check(submit(*c, "end", c->snapshot().actor), "End the turn");
    }
    check(unit(*c, 1).hit_points == 0 && !unit(*c, 1).dead,
          "A Troll with a hero on it stays down without dying");
    turn_of(*c, second);
    check(move_to(*c, second, {2, 2}), "The hero steps off the Troll");
    check(submit(*c, "end", second), "The hero ends its turn");
    turn_of(*c, 1);
    check(unit(*c, 1).hit_points == unit(*c, 1).max_hit_points,
          "Its square clear, the Troll gets up on its turn");
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

// As in the original game, a downed creature does not hold its square. In a
// one-square corridor a monster (1) drops the hero beside it (2); the hero
// behind (3) can be reached only by standing where the first one fell. (Dying
// heroes walling in the last one left a Slums troll and that hero unable to
// reach each other, and the fight never ended.)
void downed_square_stood_on()
{
    const auto module = rules("creature poker 1 1000 20 30 20 1 2 1 0 0 0 0 0 0 0 0 0 1 0\n"
                              "creature frail 1 2 0 30 0 1 2 0 0 0 0 0 0 0 0 0 0 1 0\n");
    std::vector<Terrain> corridor(30, Terrain::obstacle);
    for (int x = 0; x < 10; ++x)
    {
        corridor[static_cast<std::size_t>(10 + x)] = Terrain::open;
    }
    auto c = module->create({{10, 3, corridor},
                             {{1, "poker", "Monster", Side::opposition, {1, 1}},
                              {2, "frail", "Hero", Side::party, {2, 1}},
                              {3, "target", "Friend", Side::party, {3, 1}}}},
                            7);
    turn_of(*c, 1);
    check(submit(*c, "melee", 1, 2) && unit(*c, 2).hit_points == 0 && !unit(*c, 2).dead,
          "The monster drops the hero beside it");
    check(move_to(*c, 1, {2, 1}), "The monster stands where the hero fell");
    check(submit(*c, "end", 1), "The monster ends its turn");
    turn_of(*c, 1);
    check(offered(*c, "melee", 1, 3), "From there it reaches the hero behind");
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
        troll_gets_up_later();
        troll_delay_saved();
        troll_burns();
        troll_held_down();
        troll_stays_down();
        downed_square_stood_on();
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
