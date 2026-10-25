// The Slums set encounters' creatures (docs/audits/cluebook-check.md): the
// Hobgoblin Warrior's poisoned arrows, the Ogre, the Troll's three Rends and
// Regeneration, and the level-3 magic-user; Podol Plaza's buccaneer; and the
// Cadorna Textile House's scorpions and Wight, and Sunlight Sensitivity; and
// Mendor's Library's Specter and Basilisk.
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
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

bool aim(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == "area_move" && command.destination == cell)
        {
            return c.submit(command);
        }
    }
    return false;
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
    check(checkpoint.starts_with("OGCOMBAT 69 "), "Checkpoints use the current format");
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
    check(checkpoint.starts_with("OGCOMBAT 69 "), "Checkpoints use the current format");
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

// Podol Plaza's buccaneer, the SRD Pirate, trades the first of its two Dagger
// attacks for Enthralling Panache: a Wisdom save (DC 12) or Charmed until the
// start of its next turn, so its target cannot attack it. The Dagger that
// follows does not end the charm.
void buccaneer_enthralls()
{
    const auto module = rules();
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "buccaneer", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2), "The buccaneer attacks");
        check(count_logged(*c, "Monster uses Enthralling Panache on Hero.") == 1 &&
                  count_logged(*c, "Monster -> Hero") == 1,
              "Enthralling Panache replaces one of the buccaneer's two attacks");
        if (count_logged(*c, "Hero is Charmed by Monster.") == 0 ||
            unit(*c, 2).hit_points == unit(*c, 2).max_hit_points)
        {
            continue;
        }
        check(submit(*c, "end", 1), "The buccaneer ends its turn");
        turn_of(*c, 2);
        check(!offered(*c, "melee", 2, 1), "A charmed hero cannot attack the buccaneer");
        check(submit(*c, "end", 2), "The hero ends its turn");
        turn_of(*c, 1);
        check(offered(*c, "melee", 1, 2) && submit(*c, "melee", 1, 2) &&
                  count_logged(*c, "Monster uses Enthralling Panache on Hero.") == 2,
              "The charm ends at the start of the buccaneer's next turn");
        return;
    }
    check(false, "Find a charm that holds through a Dagger hit");
}
// A human-shaped creature fights with the weapons it holds (#274): their dice
// and range in place of its stat block's, which keeps its own bonuses.
void held_weapons()
{
    const auto module = rules();
    const auto orc = [&](std::string melee, std::string ranged)
    {
        Encounter encounter{{10, 4, std::vector<Terrain>(40)},
                            {{1, "slums-orc", "Orc", Side::opposition, {1, 1}},
                             {2, "target", "Hero", Side::party, {2, 1}}}};
        encounter.participants[0].melee_weapon = std::move(melee);
        encounter.participants[0].ranged_weapon = std::move(ranged);
        return module->create(std::move(encounter), 7);
    };
    const auto plain = unit(*orc("", ""), 1);
    check(plain.melee_rating == 12 && plain.ranged_rating == 6,
          "Without weapons the orc has its Greataxe and Javelins");
    auto armed = orc("longsword+1", "longbow");
    check(unit(*armed, 1).melee_rating == 8 + 8 && unit(*armed, 1).ranged_rating == 8,
          "The orc fights with its +1 Longsword and shoots its Longbow");
    const auto wrong = unit(*orc("longbow", "longsword"), 1);
    check(wrong.melee_rating == 12 && wrong.ranged_rating == 6,
          "A weapon of the wrong kind leaves the stat block's");
    const auto checkpoint = armed->save();
    const auto restored = module->restore(checkpoint);
    check(restored->save() == checkpoint && unit(*restored, 1).melee_rating == 16,
          "A checkpoint keeps the weapons a creature holds");
}
// A Giant Scorpion makes two Claw attacks, then its Sting, whose hit adds
// Poison damage. The Large Scorpion is its stat block with the original's 10
// Hit Points.
void scorpion_stings()
{
    const auto module = rules();
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "giant-scorpion", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster -> Hero") == 3,
              "The scorpion makes three attacks");
        const auto log = c->snapshot().log();
        if (log.back().find("Monster -> Hero") != std::string::npos ||
            count_logged(*c, "Poison damage") == 0)
        {
            continue;
        }
        check(count_logged(*c, "Poison damage") == 1 &&
                  log.back().find("Hero takes") != std::string::npos,
              "Only the Sting, the last attack, adds Poison damage");
        auto large = fight(*module, "large-scorpion", "target");
        check(unit(*large, 1).max_hit_points == 10, "The Large Scorpion has 10 Hit Points");
        return;
    }
    check(false, "Find a Sting that hits");
}

// A Wight trades one of its two melee attacks for Life Drain: on a failed
// Constitution save (DC 13), Necrotic damage that lowers the Hit Point maximum
// by as much. At range it shoots its Necrotic Bow twice.
void wight_drains_life()
{
    const auto module = rules();
    bool drained = false;
    for (std::uint64_t seed = 1; seed < 40 && !drained; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "wight", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2) &&
                  count_logged(*c, "Monster uses Life Drain on Hero.") == 1 &&
                  count_logged(*c, "Monster -> Hero") == 1,
              "Life Drain replaces one of the Wight's two attacks");
        const auto lowered = count_logged(*c, "Hero's Hit Point maximum drops by");
        drained = lowered == 1 && unit(*c, 2).max_hit_points < 1000 &&
                  count_logged(*c, "Necrotic damage") >= 1;
    }
    check(drained, "A failed save lowers the Hit Point maximum");
    auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                             {{1, "wight", "Monster", Side::opposition, {1, 1}},
                              {2, "target", "Hero", Side::party, {8, 1}}}},
                            3);
    turn_of(*c, 1);
    check(submit(*c, "ranged", 1, 2) && count_logged(*c, "Monster -> Hero") == 2 &&
              count_logged(*c, "Life Drain") == 0,
          "At range the Wight shoots twice, without Life Drain");
}

// Whether `actor` is offered `verb` at any target.
bool offered_any(const CombatSession &c, std::string_view verb, EntityId actor)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && command.actor == actor;
                       });
}

// Grishnak, the SRD Priest, casts Spirit Guardians once a day with her Action,
// spending no slot, and Divine Aid's spells (Bless among them) as a Bonus
// Action three times a day.
void grishnak_casts()
{
    const auto module = rules();
    auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                             {{1, "grishnak", "Monster", Side::opposition, {1, 1}},
                              {2, "target", "Hero", Side::party, {2, 1}}}},
                            9);
    turn_of(*c, 1);
    check(offered_any(*c, "spirit_guardians_innate", 1) && offered_any(*c, "bless_innate_bonus", 1),
          "Grishnak is offered Spirit Guardians and Divine Aid's Bless");
    check(submit(*c, "spirit_guardians_innate", 1) &&
              count_logged(*c, "Spirit guardians flit around Monster.") == 1,
          "Grishnak casts Spirit Guardians with her Action");
    check(offered_any(*c, "bless_innate_bonus", 1) && !offered_any(*c, "melee", 1),
          "Her Bonus Action is left for Divine Aid; her Action is spent");
    check(submit(*c, "end", 1), "Grishnak ends her turn");
    turn_of(*c, 1);
    check(!offered_any(*c, "spirit_guardians_innate", 1),
          "Spirit Guardians is cast once a day");
    for (unsigned day_use = 0; day_use < 3; ++day_use)
    {
        check(offered_any(*c, "healing_word_innate_bonus", 1) ||
                  offered_any(*c, "bless_innate_bonus", 1),
              "Divine Aid has a use left");
        const auto verb = offered_any(*c, "bless_innate_bonus", 1) ? "bless_innate_bonus"
                                                                    : "healing_word_innate_bonus";
        // Bless takes up to three creatures, then is cast.
        check(submit(*c, verb, 1) && (!offered_any(*c, "spell_cast", 1) || submit(*c, "spell_cast", 1)),
              "Grishnak uses Divine Aid");
        check(submit(*c, "end", 1), "Grishnak ends her turn");
        turn_of(*c, 1);
    }
    check(!offered_any(*c, "bless_innate_bonus", 1) &&
              !offered_any(*c, "healing_word_innate_bonus", 1),
          "Divine Aid is used three times a day");
}

// Sunlight Sensitivity: a kobold on a square open to the sky by day attacks
// with Disadvantage; in shade or at night it does not.
void sunlight_sensitivity()
{
    const auto module = rules();
    const auto attack = [&](bool daylight, bool sky)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40), std::vector<bool>(40, sky)},
                                 {{1, "slums-kobold", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}},
                                 1,
                                 100,
                                 0,
                                 daylight},
                                5);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2), "The kobold attacks");
        return count_logged(*c, "(disadvantage)") == 1;
    };
    check(attack(true, true), "A kobold in sunlight attacks with Disadvantage");
    check(!attack(true, false) && !attack(false, true),
          "In shade or at night a kobold attacks normally");
}
// A Specter's Life Drain: each melee hit lowers the Hit Point maximum by the
// damage taken. It resists Slashing damage.
void specter_drains_life()
{
    const auto module = rules();
    bool drained = false;
    for (std::uint64_t seed = 1; seed < 40 && !drained; ++seed)
    {
        auto c = fight(*module, "specter", "target");
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster -> Hero") == 1,
              "The Specter makes one Life Drain attack");
        const int lost = 1000 - unit(*c, 2).hit_points;
        drained = lost > 0 && unit(*c, 2).max_hit_points == 1000 - lost &&
                  count_logged(*c, "Hero's Hit Point maximum drops by " + std::to_string(lost)) == 1;
    }
    check(drained, "A Life Drain hit lowers the Hit Point maximum by the damage taken");
    const auto cutting = rules("creature cutter 30 1000 20 30 30 1 2 9 0 0 0 0 0 0 0 0 0 1 0\n"
                               "damage_types cutter slashing slashing\n");
    auto c = fight(*cutting, "specter", "cutter");
    turn_of(*c, 2);
    check(submit(*c, "melee", 2, 1) && unit(*c, 1).hit_points == 33,
          "The Specter takes half of a 10- or 11-point Slashing hit");
}

// A Specter flies and is incorporeal: it passes through a wall and an enemy to
// stand beyond them, where a Skeleton cannot.
void specter_passes_through()
{
    const auto module = rules();
    std::vector<Terrain> wall(40);
    for (int y = 0; y < 4; ++y)
    {
        wall[static_cast<std::size_t>(y * 10 + 3)] = Terrain::obstacle;
    }
    const auto pass = [&](const std::string &monster, Cell destination)
    {
        auto c = module->create({{10, 4, wall},
                                 {{1, monster, "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {8, 3}}}},
                                3);
        turn_of(*c, 1);
        return move_to(*c, 1, destination);
    };
    check(pass("specter", {5, 1}), "The Specter moves through the wall");
    check(!pass("skeleton", {5, 1}), "A Skeleton cannot pass the wall");
    // A corridor one square wide, the hero standing in it.
    std::vector<Terrain> corridor(20, Terrain::obstacle);
    std::fill_n(corridor.begin(), 10, Terrain::open);
    auto c = module->create({{10, 2, corridor},
                             {{1, "specter", "Monster", Side::opposition, {1, 0}},
                              {2, "target", "Hero", Side::party, {2, 0}}}},
                            3);
    turn_of(*c, 1);
    check(move_to(*c, 1, {3, 0}), "The Specter moves through an enemy");
}

// A Basilisk's Petrifying Gaze, a Bonus Action that recharges on a 4-6: a
// creature in its Cone that fails a Constitution save is Restrained and, if it
// fails again at the end of its next turn, turns to stone. A Specter is immune
// to the Restrained condition.
void basilisk_gazes()
{
    const auto module = rules();
    bool stone = false, shaken = false;
    for (std::uint64_t seed = 1; seed < 80 && !(stone && shaken); ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "basilisk", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {3, 1}},
                                  {3, "specter", "Ghost", Side::party, {3, 2}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "petrifying_gaze", 1) && aim(*c, Cell{3, 1}) &&
                  submit(*c, "area_cast", 1),
              "The Basilisk gazes with its Bonus Action");
        check(!offered_any(*c, "petrifying_gaze", 1) && offered_any(*c, "dash", 1),
              "The gaze spends the Bonus Action and leaves the Action");
        check(count_logged(*c, "Ghost is Restrained") ==
                      count_logged(*c, "Ghost is immune to the Restrained condition.") &&
                  unit(*c, 3).status.find("Restrained") == std::string::npos,
              "The Specter never stays Restrained");
        const auto restrained = [&]
        {
            const auto statuses = unit(*c, 2).status;
            return statuses.find("Restrained (Petrifying Gaze)") != std::string::npos;
        };
        if (!restrained())
        {
            continue;
        }
        turn_of(*c, 2);
        check(submit(*c, "end", 2), "The Restrained hero ends its turn");
        if (unit(*c, 2).status.find("Petrified") != std::string::npos)
        {
            stone = count_logged(*c, "Hero turns to stone.") == 1;
        }
        else
        {
            shaken = count_logged(*c, "Hero shakes off the stone.") == 1 && !restrained();
        }
    }
    check(stone && shaken, "A second failed save petrifies; a success ends the gaze");
    // Recharge 4-6: some turns the gaze comes back, some it does not.
    bool recharged = false, waited = false;
    for (std::uint64_t seed = 1; seed < 40 && !(recharged && waited); ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "basilisk", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {6, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "petrifying_gaze", 1) && aim(*c, Cell{6, 1}) &&
                  submit(*c, "area_cast", 1) && submit(*c, "end", 1),
              "The Basilisk gazes and ends its turn");
        turn_of(*c, 1);
        const bool back = offered_any(*c, "petrifying_gaze", 1);
        check(back == (count_logged(*c, "Monster's Petrifying Gaze recharges") == 1),
              "The gaze is offered again exactly when it recharges");
        (back ? recharged : waited) = true;
    }
    check(recharged && waited, "The gaze recharges on some turns and not others");
}
// A Stirge's Proboscis attaches it to the creature it hits; attached, it does
// not attack, and drains 2d4 Necrotic as each of its turns starts, until the
// creature pulls it off with an action.
void stirge_attaches()
{
    const auto module = rules();
    bool attached = false;
    for (std::uint64_t seed = 1; seed < 40 && !attached; ++seed)
    {
        auto c = fight(*module, "stirge", "target");
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2), "The Stirge attacks");
        attached = count_logged(*c, "Monster attaches to Hero.") == 1;
        if (!attached)
        {
            continue;
        }
        check(!offered(*c, "melee", 1, 2), "An attached Stirge cannot attack");
        check(submit(*c, "end", 1), "The Stirge ends its turn");
        turn_of(*c, 1);
        check(count_logged(*c, "Necrotic damage from Monster.") == 1,
              "The attached Stirge drains Necrotic damage as its turn starts");
        check(submit(*c, "end", 1), "The Stirge ends its turn again");
        turn_of(*c, 2);
        check(submit(*c, "detach", 2, 1) && count_logged(*c, "Hero pulls Monster off.") == 1,
              "The hero pulls the Stirge off with an action");
        check(submit(*c, "end", 2), "The hero ends its turn");
        turn_of(*c, 1);
        check(offered(*c, "melee", 1, 2), "Pulled off, the Stirge attacks again");
    }
    check(attached, "A Stirge's hit attaches it");
}

// A Drider's Magic of the Spider Queen is a Bonus Action that recharges on a
// 5 or 6; its own Web never catches it.
void drider_casts()
{
    const auto module = rules();
    bool recharged = false, waited = false;
    for (std::uint64_t seed = 1; seed < 60 && !(recharged && waited); ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "drider", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {6, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(offered_any(*c, "faerie_fire_innate_bonus", 1), "The Drider can cast Faerie Fire");
        check(submit(*c, "faerie_fire_innate_bonus", 1) && aim(*c, Cell{6, 1}) &&
                  submit(*c, "area_cast", 1) && submit(*c, "end", 1),
              "The Drider casts Faerie Fire with its Bonus Action");
        turn_of(*c, 1);
        const bool back = offered_any(*c, "faerie_fire_innate_bonus", 1);
        check(back == (count_logged(*c, "Magic of the Spider Queen recharges") == 1),
              "The spells come back exactly when they recharge");
        (back ? recharged : waited) = true;
    }
    check(recharged && waited, "Magic of the Spider Queen recharges on some turns only");
}

// Kovel Mansion's master thief is the SRD Spy: Cunning Action lets it Dash or
// Disengage as a Bonus Action, and its Shortsword adds 2d6 Poison damage.
void master_thief_strikes()
{
    const auto module = rules();
    bool poisoned = false;
    for (std::uint64_t seed = 1; seed < 40 && !poisoned; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "kovel-master-thief", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(offered_any(*c, "cunning_dash", 1) && offered_any(*c, "cunning_disengage", 1),
              "The master thief has Cunning Action");
        check(submit(*c, "melee", 1, 2), "The master thief attacks");
        poisoned = count_logged(*c, "Poison damage") >= 1;
    }
    check(poisoned, "The master thief's Shortsword adds Poison damage");
}

// Valjevo Castle's knights parry as the SRD Knight does: hit by a melee attack
// roll, a knight adds 2 to its AC with its Reaction when that makes the attack
// miss, once a round.
void knight_parries()
{
    const auto module = rules("creature fencer 10 1000 0 30 5 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    bool parried = false;
    for (std::uint64_t seed = 1; seed < 80 && !parried; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "castle-knight", "Monster", Side::opposition, {1, 1}},
                                  {2, "fencer", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 2);
        check(submit(*c, "melee", 2, 1), "The fencer attacks the knight");
        parried = count_logged(*c, "Monster parries Hero's attack.") == 1;
        if (parried)
        {
            const auto log = c->snapshot().log();
            check(std::any_of(log.begin(), log.end(),
                              [](const auto &line)
                              {
                                  return line.find("Hero -> Monster") != std::string::npos &&
                                         line.find("vs AC 20 misses") != std::string::npos;
                              }),
                  "The parried attack misses against AC 20");
        }
    }
    check(parried, "A knight parries a melee hit that its Parry turns into a miss");
}

// Valhingen Graveyard's mummies glare as the SRD Mummy does: with its two
// Rotting Fists, a Wisdom save (DC 11) or Frightened until the end of the
// mummy's next turn; a success makes the creature immune to its glare.
void mummy_glares()
{
    const auto module = rules();
    bool frightened = false, resisted = false;
    for (std::uint64_t seed = 1; seed < 80 && !(frightened && resisted); ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "mummy", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2) &&
                  count_logged(*c, "Monster uses Dreadful Glare on Hero.") == 1 &&
                  count_logged(*c, "Monster -> Hero") == 2,
              "The mummy glares besides its two Rotting Fists");
        if (count_logged(*c, "Hero is Frightened.") == 1)
        {
            frightened = unit(*c, 2).status.find("Frightened") != std::string::npos;
            continue;
        }
        check(submit(*c, "end", 1), "The mummy ends its turn");
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2) &&
                  count_logged(*c, "Monster uses Dreadful Glare on Hero.") == 1,
              "A creature that resists the glare is immune to it");
        resisted = true;
    }
    check(frightened && resisted, "The glare frightens on a failure and not after a success");
}

// A mummy's Rotting Fist curses the creature it hits: it regains no Hit
// Points, and as each day ends its Hit Point maximum drops by 3d6.
void mummy_curses()
{
    const auto module = rules();
    std::optional<rules::VitalState> cursed;
    for (std::uint64_t seed = 1; seed < 40 && !cursed; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "mummy", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(submit(*c, "melee", 1, 2), "The mummy attacks");
        if (count_logged(*c, "Hero is cursed.") == 1)
        {
            check(unit(*c, 2).status.find("Cursed (Rotting Fist)") != std::string::npos,
                  "The cursed hero shows the curse");
            cursed = unit(*c, 2).persistent;
        }
    }
    check(cursed.has_value(), "A Rotting Fist hit curses");
    std::vector<rules::Participant> hero{{2, "target", "Hero", Side::party, {2, 1}}};
    hero[0].state = *cursed;
    RandomState random{7};
    module->elapse(hero, std::chrono::hours{25}, random);
    auto later = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "target", "Other", Side::opposition, {8, 1}}, hero[0]}},
                                3);
    const int lowered = 1000 - unit(*later, 2).max_hit_points;
    check(lowered >= 3 && lowered <= 18, "A day later the Hit Point maximum drops by 3d6");
    module->elapse(hero, std::chrono::hours{48}, random);
    auto much_later = module->create(
        {{10, 4, std::vector<Terrain>(40)},
         {{1, "target", "Other", Side::opposition, {8, 1}}, hero[0]}},
        3);
    check(1000 - unit(*much_later, 2).max_hit_points >= lowered + 6,
          "Two days more lower it by 6d6 more, and the deeper curse still loads");
}

// Valhingen Graveyard's vampire charms and bites as the SRD Vampire does: its
// Charm is Charm Person as a Bonus Action (DC 17), and with its two Grave
// Strikes it bites a creature it has Charmed: a Constitution save or Piercing
// and Necrotic damage, the Necrotic lowering the Hit Point maximum and healing
// the vampire.
void vampire_bites()
{
    const auto module = rules("creature hardy 10 300 0 30 0 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    bool bitten = false;
    for (std::uint64_t seed = 1; seed < 120 && !bitten; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                                 {{1, "vampire", "Monster", Side::opposition, {1, 1}},
                                  {2, "hardy", "Hero", Side::party, {2, 1}}}},
                                seed);
        turn_of(*c, 1);
        check(offered_any(*c, "charm_person_innate_bonus", 1), "The vampire can Charm");
        check(submit(*c, "charm_person_innate_bonus", 1, 2), "The vampire casts Charm Person");
        if (unit(*c, 2).status.find("Charmed") == std::string::npos)
        {
            continue;
        }
        check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster uses Bite on Hero.") == 1,
              "The vampire bites the creature it has Charmed with its Grave Strikes");
        bitten = count_logged(*c, "Hero's Hit Point maximum drops by") == 1 &&
                 unit(*c, 2).max_hit_points < 300;
    }
    check(bitten, "A failed save against the Bite lowers the Hit Point maximum");
}

// Tyranthraxus breathes as the SRD Adult Bronze Dragon does: Lightning Breath
// with its action, 10d10 Lightning to each creature in its Line, half on a
// DC 19 Dexterity save, recharging on a 5 or 6.
void tyranthraxus_breathes()
{
    const auto module = rules();
    bool recharged = false, waited = false;
    for (std::uint64_t seed = 1; seed < 60 && !(recharged && waited); ++seed)
    {
        auto c = module->create({{12, 5, std::vector<Terrain>(60)},
                                 {{1, "tyranthraxus", "Monster", Side::opposition, {0, 1}},
                                  {2, "target", "Hero", Side::party, {8, 2}}}},
                                seed);
        turn_of(*c, 1);
        const auto before = unit(*c, 2).hit_points;
        check(submit(*c, "lightning_breath", 1) && aim(*c, Cell{8, 2}) &&
                  submit(*c, "area_cast", 1),
              "Tyranthraxus breathes with his action");
        const auto taken = before - unit(*c, 2).hit_points;
        check(count_logged(*c, "Monster uses Lightning Breath.") == 1 &&
                  count_logged(*c, "Hero takes ") == 1 && taken >= 5 && taken <= 100,
              "The breath deals 10d10 Lightning, halved on a save");
        check(!offered_any(*c, "lightning_breath", 1) && submit(*c, "end", 1),
              "The breath is spent and Tyranthraxus ends his turn");
        turn_of(*c, 1);
        const bool back = offered_any(*c, "lightning_breath", 1);
        check(back == (count_logged(*c, "Monster's Lightning Breath recharges") == 1),
              "The breath is offered again exactly when it recharges");
        (back ? recharged : waited) = true;
    }
    check(recharged && waited, "The breath recharges on some turns and not others");
    bool refused = false;
    try
    {
        (void)rules("breath target lightning_bolt 19 5\n");
    }
    catch (const std::runtime_error &)
    {
        refused = true;
    }
    check(refused, "A breath row names a breath weapon, not a spell");
}

// Heroes who fail every save: -10 to each, so even a 20 misses DC 11.
const std::string frail_heroes = "creature frail 1 1000 0 30 0 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                                 "saves frail -10 -10 -10 -10 -10 -10\n";

// Ends the turns of everyone after `id` until `id`'s turn comes again.
void round_after(CombatSession &c, EntityId id)
{
    check(c.snapshot().actor == id && submit(c, "end", id), "End the monster's turn");
    for (unsigned turns = 0; c.snapshot().actor != id; ++turns)
    {
        check(turns < 8, "Come back to the monster");
        check(submit(c, "end", c.snapshot().actor), "End a hero's turn");
    }
}

// Legendary Resistance (SRD 5.2.1) turns a failed save into a success while a
// use is left: here once a day, against the first of two mummies' glares.
void legendary_resistance()
{
    const auto module = rules(frail_heroes + "legendary_resistance frail 1\n");
    auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                             {{1, "mummy", "First", Side::opposition, {1, 1}},
                              {3, "mummy", "Second", Side::opposition, {3, 1}},
                              {2, "frail", "Hero", Side::party, {2, 1}}}},
                            7);
    turn_of(*c, 1);
    check(submit(*c, "melee", 1, 2) &&
              count_logged(*c, "Hero uses Legendary Resistance and succeeds instead (0 left).") ==
                  1 &&
              unit(*c, 2).status.find("Frightened") == std::string::npos,
          "Legendary Resistance turns the first failed save into a success");
    check(submit(*c, "end", 1), "The first mummy ends its turn");
    turn_of(*c, 3);
    check(submit(*c, "melee", 3, 2) && count_logged(*c, "Hero is Frightened.") == 1,
          "With no use left the next failed save fails");
}

// Valhingen Graveyard's vampire takes the SRD Vampire's Legendary Actions:
// right after each other creature's turn, three a round, a Deathless Strike at
// a creature beside it (without moving), or Beguile, Command's Grovel, at one
// farther off, once a round.
void vampire_legendary_actions()
{
    const auto module = rules();
    auto beside = module->create({{10, 4, std::vector<Terrain>(40)},
                                  {{1, "vampire", "Monster", Side::opposition, {1, 1}},
                                   {2, "target", "Hero A", Side::party, {2, 1}},
                                   {3, "target", "Hero B", Side::party, {2, 2}},
                                   {4, "target", "Hero C", Side::party, {2, 0}},
                                   {5, "target", "Hero D", Side::party, {0, 1}}}},
                                 7);
    turn_of(*beside, 1);
    const auto struck = count_logged(*beside, "uses Deathless Strike (Legendary Action).");
    round_after(*beside, 1);
    check(count_logged(*beside, "uses Deathless Strike (Legendary Action).") - struck == 3 &&
              count_logged(*beside, "Monster uses Beguile") == 0,
          "The vampire strikes after three of the four heroes' turns");
    auto apart = module->create({{12, 5, std::vector<Terrain>(60)},
                                 {{1, "vampire", "Monster", Side::opposition, {1, 1}},
                                  {2, "target", "Hero A", Side::party, {9, 1}},
                                  {3, "target", "Hero B", Side::party, {9, 3}}}},
                                7);
    turn_of(*apart, 1);
    const auto beguiled = count_logged(*apart, "Monster uses Beguile (Legendary Action).");
    round_after(*apart, 1);
    check(count_logged(*apart, "Monster uses Beguile (Legendary Action).") - beguiled == 1 &&
              count_logged(*apart, "Deathless Strike") == 0,
          "The vampire Beguiles a creature out of reach once a round");
}

// Tyranthraxus takes the SRD Adult Bronze Dragon's Legendary Actions:
// Thunderclap on two enemies or more (Constitution save, DC 17, or 3d6
// Thunder), Pounce at one beside him, Guiding Light at one farther off.
void tyranthraxus_legendary_actions()
{
    const auto module = rules(frail_heroes);
    const auto first_action = [&](Cell a, Cell b)
    {
        auto c = module->create({{14, 6, std::vector<Terrain>(84)},
                                 {{1, "tyranthraxus", "Monster", Side::opposition, {0, 2}},
                                  {2, "frail", "Hero A", Side::party, a},
                                  {3, "frail", "Hero B", Side::party, b}}},
                                7);
        turn_of(*c, 1);
        const auto before = c->snapshot().log().size();
        round_after(*c, 1);
        const auto log = c->snapshot().log();
        return std::vector<std::string>(log.begin() + static_cast<std::ptrdiff_t>(before),
                                        log.end());
    };
    const auto logged = [](const std::vector<std::string> &log, std::string_view text)
    {
        return std::count_if(log.begin(), log.end(),
                             [&](const auto &line)
                             {
                                 return line.find(text) != std::string::npos;
                             });
    };
    const auto grouped = first_action({7, 2}, {7, 3});
    check(logged(grouped, "Monster uses Thunderclap (Legendary Action).") == 2 &&
              logged(grouped, "Hero A takes ") >= 1 && logged(grouped, "Hero B takes ") >= 1,
          "Tyranthraxus thunders at two heroes together");
    const auto close = first_action({1, 2}, {13, 5});
    check(logged(close, "Monster uses Pounce (Legendary Action).") == 2,
          "Tyranthraxus pounces on a hero beside him");
    const auto far = first_action({9, 0}, {13, 5});
    check(logged(far, "Monster uses Guiding Light (Legendary Action).") == 2 &&
              logged(far, "Monster -> Hero A") == 2,
          "Tyranthraxus casts Guiding Light at a hero out of reach");
}

// Tyranthraxus breathes Repulsion Breath for the last of his three Rends when
// its 30-foot Cone catches two enemies: a Strength save (DC 19) or pushed 60
// feet away and knocked Prone.
void tyranthraxus_repels()
{
    const auto module = rules(frail_heroes);
    auto c = module->create({{20, 4, std::vector<Terrain>(80)},
                             {{1, "tyranthraxus", "Monster", Side::opposition, {1, 1}},
                              {2, "frail", "Hero A", Side::party, {2, 1}},
                              {3, "frail", "Hero B", Side::party, {3, 1}}}},
                            7);
    turn_of(*c, 1);
    check(submit(*c, "melee", 1, 2) && count_logged(*c, "Monster -> Hero A") == 2 &&
              count_logged(*c, "Monster uses Repulsion Breath.") == 1,
          "Two Rends, then Repulsion Breath");
    check(unit(*c, 2).cell.x > 3 && unit(*c, 3).cell.x > 3 &&
              unit(*c, 2).status.find("Prone") != std::string::npos &&
              unit(*c, 3).status.find("Prone") != std::string::npos,
          "Both heroes are pushed away and knocked Prone");
    auto alone = module->create({{20, 4, std::vector<Terrain>(80)},
                                 {{1, "tyranthraxus", "Monster", Side::opposition, {1, 1}},
                                  {2, "frail", "Hero A", Side::party, {2, 1}}}},
                                7);
    turn_of(*alone, 1);
    check(submit(*alone, "melee", 1, 2) && count_logged(*alone, "Monster -> Hero A") == 3 &&
              count_logged(*alone, "Repulsion Breath") == 0,
          "With one enemy in reach he rends three times");
}

// An `off` row switches a monster ability off: for every creature, or for one.
void abilities_switch_off()
{
    const auto off_for_one = rules("off legendary vampire\n");
    auto c = fight(*off_for_one, "vampire", "target");
    turn_of(*c, 1);
    round_after(*c, 1);
    check(count_logged(*c, "Legendary Action") == 0, "The vampire's Legendary Actions are off");
    auto dragon = fight(*off_for_one, "tyranthraxus", "target");
    turn_of(*dragon, 1);
    round_after(*dragon, 1);
    check(count_logged(*dragon, "Legendary Action") == 1,
          "Tyranthraxus keeps his Legendary Actions");
    const auto off_for_all = rules("off legendary\n");
    auto none = fight(*off_for_all, "tyranthraxus", "target");
    turn_of(*none, 1);
    round_after(*none, 1);
    check(count_logged(*none, "Legendary Action") == 0, "No one takes Legendary Actions");
    bool refused = false;
    try
    {
        (void)rules("off creature\n");
    }
    catch (const std::runtime_error &)
    {
        refused = true;
    }
    check(refused, "Only a monster ability can be switched off");
}

// Valjevo Castle's medusa makes the SRD Medusa's Multiattack: two Claws and
// then Snake Hair (1d4 + 3 Piercing and 4d6 Poison), or three Poison Rays.
void medusa_strikes()
{
    const auto module = rules();
    auto close = fight(*module, "medusa", "target");
    turn_of(*close, 1);
    check(submit(*close, "melee", 1, 2) && count_logged(*close, "Monster -> Hero") == 3 &&
              count_logged(*close, "Poison damage") >= 1,
          "Two Claws, then the poisonous Snake Hair");
    auto far = module->create({{12, 4, std::vector<Terrain>(48)},
                               {{1, "medusa", "Monster", Side::opposition, {1, 1}},
                                {2, "target", "Hero", Side::party, {8, 1}}}},
                              7);
    turn_of(*far, 1);
    check(submit(*far, "ranged", 1, 2) && count_logged(*far, "Monster -> Hero") == 3,
          "Three Poison Rays");
}

// The giants' hit riders (SRD 5.2.1): a Hill Giant's Trash Lob poisons until
// the end of the target's next turn; an Ettin's Morningstar, its second
// attack, and a Fire Giant's Hammer Throw give Disadvantage on the target's
// next attack roll, and the Hammer Throw pushes 15 feet.
void giants_hit_riders()
{
    const auto module = rules();
    auto lob = module->create({{12, 4, std::vector<Terrain>(48)},
                               {{1, "hill-giant", "Monster", Side::opposition, {1, 1}},
                                {2, "target", "Hero", Side::party, {7, 1}}}},
                              7);
    turn_of(*lob, 1);
    check(submit(*lob, "ranged", 1, 2) && count_logged(*lob, "Hero is Poisoned.") >= 1 &&
              unit(*lob, 2).status.find("Poisoned") != std::string::npos,
          "Trash Lob poisons");
    auto ettin = fight(*module, "ettin", "target");
    turn_of(*ettin, 1);
    check(submit(*ettin, "melee", 1, 2) && count_logged(*ettin, "Hero is knocked Prone.") == 1 &&
              count_logged(*ettin, "Hero has Disadvantage on its next attack roll.") == 1 &&
              unit(*ettin, 2).status.find("Hampered") != std::string::npos,
          "The Battleaxe knocks Prone and the Morningstar hampers");
    check(submit(*ettin, "end", 1), "The ettin ends its turn");
    turn_of(*ettin, 2);
    check(submit(*ettin, "melee", 2, 1) &&
              unit(*ettin, 2).status.find("Hampered") == std::string::npos,
          "The hero's next attack roll spends the hampering");
    auto hammer = module->create({{14, 4, std::vector<Terrain>(56)},
                                  {{1, "fire-giant", "Monster", Side::opposition, {1, 1}},
                                   {2, "target", "Hero", Side::party, {7, 1}}}},
                                 7);
    turn_of(*hammer, 1);
    check(submit(*hammer, "ranged", 1, 2) && count_logged(*hammer, "Hero is pushed 15 feet.") >= 1 &&
              unit(*hammer, 2).cell.x >= 10 &&
              count_logged(*hammer, "Hero has Disadvantage on its next attack roll.") >= 1,
          "The Hammer Throw pushes 15 feet and hampers");
}

// The Buccaneers' captain is the SRD Pirate Captain: Captain's Charm with its
// Bonus Action (Wisdom save, DC 14, or Charmed until the start of its next
// turn), Advantage after a Rapier hit on its next attack this turn, and
// Riposte, which parries a melee hit with +3 AC and strikes back on the miss.
void pirate_captain_fights()
{
    const auto module = rules(frail_heroes);
    auto c = fight(*module, "pirate-captain", "frail");
    turn_of(*c, 1);
    check(submit(*c, "melee", 1, 2) &&
              count_logged(*c, "Monster uses Captain's Charm on Hero.") == 1 &&
              count_logged(*c, "Hero is Charmed by Monster.") == 1,
          "The captain charms the creature it attacks");
    check(count_logged(*c, "vs AC 1 (advantage)") >= 1,
          "A Rapier hit gives Advantage on the next attack");
    check(submit(*c, "end", 1), "The captain ends its turn");
    turn_of(*c, 2);
    check(!offered(*c, "melee", 2, 1), "The charmed hero cannot attack the captain");
    const auto fencing = rules("creature fencer 10 1000 0 30 5 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    bool riposted = false;
    for (std::uint64_t seed = 1; seed < 120 && !riposted; ++seed)
    {
        auto duel = fencing->create({{10, 4, std::vector<Terrain>(40)},
                                     {{1, "pirate-captain", "Monster", Side::opposition, {1, 1}},
                                      {2, "fencer", "Hero", Side::party, {2, 1}}}},
                                    seed);
        turn_of(*duel, 2);
        check(submit(*duel, "melee", 2, 1), "The fencer attacks the captain");
        if (count_logged(*duel, "Monster parries Hero's attack.") == 1)
        {
            check(count_logged(*duel, "Monster ripostes.") == 1 &&
                      count_logged(*duel, "Monster -> Hero") == 1,
                  "The parried miss draws a Riposte");
            riposted = true;
        }
    }
    check(riposted, "The captain parries and ripostes");
}

// A Valhingen Graveyard wraith's Create Specter: a Humanoid corpse within 10
// feet rises as an SRD Specter under its control; the computer does it before
// attacking.
void wraith_creates_specter()
{
    const auto module = rules();
    auto c = module->create({{10, 4, std::vector<Terrain>(40)},
                             {{1, "wraith", "Monster", Side::opposition, {1, 1}},
                              {2, "target", "Hero", Side::party, {5, 1}},
                              {3, "bandit", "Corpse", Side::party, {2, 2}, {}, VitalState{0, true, {}}}}},
                            7);
    turn_of(*c, 1);
    const auto chosen = choose_demo_command(*c);
    check(chosen.verb == "create_specter" && chosen.target == 3,
          "The computer has the wraith raise the corpse");
    check(c->submit(chosen) && count_logged(*c, "Monster uses Create Specter.") == 1 &&
              count_logged(*c, "Corpse rises as a Specter.") == 1,
          "The corpse rises as a Specter");
    const auto risen = unit(*c, 3);
    check(risen.side == Side::opposition && risen.hit_points == 22,
          "The specter fights for the wraith with the SRD Specter's 22 Hit Points");
}

// The wilderness's tiger has the SRD Tiger's Nimble Escape: Disengage or
// Hide as a Bonus Action, without Cunning Action's Dash.
void tiger_escapes()
{
    const auto module = rules();
    auto c = fight(*module, "tiger", "target");
    turn_of(*c, 1);
    check(offered_any(*c, "cunning_disengage", 1) && !offered_any(*c, "cunning_dash", 1),
          "The tiger may Disengage as a Bonus Action but not Dash");
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
        buccaneer_enthralls();
        scorpion_stings();
        wight_drains_life();
        sunlight_sensitivity();
        grishnak_casts();
        held_weapons();
        specter_drains_life();
        specter_passes_through();
        basilisk_gazes();
        stirge_attaches();
        drider_casts();
        master_thief_strikes();
        tiger_escapes();
        tyranthraxus_breathes();
        knight_parries();
        mummy_glares();
        mummy_curses();
        vampire_bites();
        legendary_resistance();
        vampire_legendary_actions();
        tyranthraxus_legendary_actions();
        tyranthraxus_repels();
        abilities_switch_off();
        medusa_strikes();
        giants_hit_riders();
        pirate_captain_fights();
        wraith_creates_specter();
        std::cout << "Slums creature tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
