// How the computer plays monsters and Quick members (docs/AI.md, #266), on
// fights drawn by hand: each rule the document states, one check at a time.
#include "opengold/combat_demo.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

CombatantView unit(EntityId id, Side side, Cell cell, int hit_points = 10, int maximum = 10)
{
    CombatantView view;
    view.id = id;
    view.name = "Unit " + std::to_string(id);
    view.side = side;
    view.cell = cell;
    view.hit_points = hit_points;
    view.max_hit_points = maximum;
    view.conscious = hit_points > 0;
    view.action = view.bonus_action = view.reaction = true;
    return view;
}

// A 12 by 8 field, open but for `walls`, where the first unit acts.
Snapshot field(std::vector<CombatantView> units, const std::vector<Cell> &walls = {})
{
    Snapshot state;
    state.revision = 1;
    state.actor = units.front().id;
    state.battlefield = {12, 8, std::vector<Terrain>(96)};
    for (const auto wall : walls)
    {
        state.battlefield.terrain[static_cast<std::size_t>(wall.y * 12 + wall.x)] =
            Terrain::obstacle;
    }
    state.combatants = std::move(units);
    return state;
}

Command command(std::string verb, EntityId target = 0, Cell destination = {})
{
    Command c;
    c.actor = 1;
    c.target = target;
    c.verb = std::move(verb);
    c.destination = destination;
    return c;
}

// A wall down column 4 with a gap in the bottom row.
std::vector<Cell> divided()
{
    std::vector<Cell> walls;
    for (int y = 0; y < 7; ++y)
    {
        walls.push_back({4, y});
    }
    return walls;
}

void target_checks()
{
    auto state = field({unit(1, Side::party, {5, 4}), unit(2, Side::opposition, {6, 4}, 3),
                        unit(3, Side::opposition, {4, 4})});
    const std::vector<Command> offered{command("melee", 2), command("melee", 3), command("end")};
    state.combatants[0].last_target = 3;
    check(choose_demo_command(state, offered).target == 3,
          "It keeps attacking its target, not the weakest enemy");
    state.combatants[0].last_target = 0;
    bool second = false, third = false;
    for (std::uint64_t revision = 1; revision <= 40; ++revision)
    {
        state.revision = revision;
        const auto target = choose_demo_command(state, offered).target;
        second = second || target == 2;
        third = third || target == 3;
    }
    check(second && third, "With no target, it picks among the enemies at random");
}

// Above Normal it goes after the weakest enemy, fewest Hit Points then lowest
// AC: on Hard once its target is gone, on Nightmare and Infernal at once. An
// attack on its own side, now offered (CLOSE-1), it never takes.
void difficulty_target_checks()
{
    auto state = field({unit(1, Side::party, {5, 4}), unit(2, Side::opposition, {6, 4}, 3),
                        unit(3, Side::opposition, {4, 4}), unit(4, Side::party, {5, 5}, 1)});
    std::vector<Command> offered{command("melee", 2), command("melee", 3), command("melee", 4),
                                 command("end")};
    for (auto &c : offered)
    {
        c.intent = c.target ? Intent::hostile : Intent::neutral;
    }
    state.combatants[0].last_target = 3;
    check(choose_demo_command(state, offered, Searching::by_sight, Difficulty::hard).target == 3,
          "On Hard it keeps its target while it stands");
    check(choose_demo_command(state, offered, Searching::by_sight, Difficulty::nightmare).target ==
              2,
          "On Nightmare it turns on the weakest enemy");
    state.combatants[0].last_target = 0;
    for (std::uint64_t revision = 1; revision <= 20; ++revision)
    {
        state.revision = revision;
        for (const auto level : {Difficulty::hard, Difficulty::infernal})
        {
            const auto target =
                choose_demo_command(state, offered, Searching::by_sight, level).target;
            check(target == 2, "Above Normal it always takes the weakest enemy");
        }
        check(choose_demo_command(state, offered).target != 4, "It never attacks its own side");
    }
    // Hit Points tied, the lower AC.
    state.combatants[1].hit_points = 10;
    state.combatants[1].armor_class = 15;
    state.combatants[2].armor_class = 12;
    check(choose_demo_command(state, offered, Searching::by_sight, Difficulty::hard).target == 3,
          "With Hit Points tied it takes the lower AC");

    // A downed enemy, still alive: finished on Nightmare and Infernal, left
    // alone below them.
    auto downed = field({unit(1, Side::opposition, {5, 4}), unit(2, Side::party, {6, 4}, 0),
                         unit(3, Side::party, {4, 4})});
    std::vector<Command> strikes{command("melee", 2), command("melee", 3), command("end")};
    for (auto &c : strikes)
    {
        c.intent = c.target ? Intent::hostile : Intent::neutral;
    }
    for (const auto level : {Difficulty::nightmare, Difficulty::infernal})
    {
        check(choose_demo_command(downed, strikes, Searching::by_sight, level).target == 2,
              "On the hardest levels it finishes a downed character");
    }
    for (const auto level : {Difficulty::easy, Difficulty::normal, Difficulty::hard})
    {
        check(choose_demo_command(downed, strikes, Searching::by_sight, level).target == 3,
              "Below them it leaves a downed character be");
    }
}

void sight_checks()
{
    const std::vector<Command> offered{command("move", 0, {1, 2}), command("end")};
    // A field too wide to hear across, with a wall at x = 5 and a way round at y = 7.
    const auto wide = [](std::vector<CombatantView> units)
    {
        auto state = field(std::move(units));
        state.battlefield = {40, 8, std::vector<Terrain>(320)};
        for (int y = 0; y < 7; ++y)
        {
            state.battlefield.terrain[static_cast<std::size_t>(y * 40 + 5)] = Terrain::obstacle;
        }
        return state;
    };
    // An ally and an enemy beyond the wall see each other; this creature neither
    // sees nor hears anyone, so it waits.
    const auto watched = wide({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {30, 1}),
                               unit(3, Side::party, {30, 4}), unit(4, Side::opposition, {31, 4})});
    check(choose_demo_command(watched, offered).verb == "end",
          "A creature that neither sees nor hears an enemy holds its ground");
    // Even when no one on the field perceives anyone; a stand-in for the player
    // in an automated run goes looking.
    const auto far_off = wide({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {30, 1})});
    check(choose_demo_command(far_off, offered).verb == "end",
          "When no one perceives anyone, it still holds its ground");
    check(choose_demo_command(far_off, offered, Searching::party).verb == "move",
          "A stand-in for the player goes to find the enemy");
    // Sound carries 100 feet, 20 squares, through the wall.
    const auto heard = wide({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {21, 1})});
    check(choose_demo_command(heard, offered).verb == "move",
          "A creature goes for an enemy it hears at 100 feet");
    const auto out_of_earshot =
        wide({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {22, 1})});
    check(choose_demo_command(out_of_earshot, offered).verb == "end",
          "An enemy just past 100 feet goes unheard");
    // Either side of a wall's corner, the stalemate the play-through found.
    const auto corner =
        field({unit(1, Side::party, {4, 3}), unit(2, Side::opposition, {3, 4})}, {{3, 3}, {4, 4}});
    check(choose_demo_command(corner, offered).verb == "move",
          "Creatures either side of a wall's corner do not wait on each other");
}

void stay_back_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1})});
    const std::vector<Command> offered{command("move", 0, {2, 1}), command("end")};
    check(choose_demo_command(state, offered).verb == "move", "A fighter closes in");
    state.combatants[0].unarmored_arcane_caster = true;
    check(choose_demo_command(state, offered).verb == "end",
          "An unarmored arcane caster stays back");
}

void ranged_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1})});
    const std::vector<Command> offered{command("shoot", 2), command("move", 0, {2, 1}),
                                       command("end")};
    state.combatants[0].melee_rating = 8;
    state.combatants[0].ranged_rating = 6;
    check(choose_demo_command(state, offered).verb == "shoot",
          "A bow rated more than half the sword is drawn before closing in");
    state.combatants[0].ranged_rating = 4;
    check(choose_demo_command(state, offered).verb == "move",
          "A weaker bow waits while the enemy can be reached");
}

// Spells by the original's priorities: Hold Person (6) before Magic Missile
// (4), and none on a roll that allows only priority 7.
void spell_checks()
{
    auto caster = unit(1, Side::party, {1, 1});
    caster.spells = {"hold_person", "magic_missile"};
    auto state = field({caster, unit(2, Side::opposition, {5, 1})});
    auto hold = command("hold_person", 2);
    hold.label = "Hold Person";
    hold.spell.concentration = true;
    auto missile = command("magic_missile", 2);
    missile.label = "Magic Missile";
    const std::vector<Command> offered{missile, hold, command("end")};
    bool held = false, missiled = false, none = false;
    for (std::uint64_t revision = 1; revision <= 60; ++revision)
    {
        state.revision = revision;
        const auto verb = choose_demo_command(state, offered).verb;
        held = held || verb == "hold_person";
        missiled = missiled || verb == "magic_missile";
        none = none || verb == "end";
    }
    check(held && !missiled && none,
          "The higher priority comes first, and a low roll casts nothing");
    // Already Concentrating, it casts no spell that would end it.
    state.combatants[0].conditions = {{"Concentrating", {}}};
    held = missiled = false;
    for (std::uint64_t revision = 1; revision <= 60; ++revision)
    {
        state.revision = revision;
        const auto verb = choose_demo_command(state, offered).verb;
        held = held || verb == "hold_person";
        missiled = missiled || verb == "magic_missile";
    }
    check(!held && missiled, "A Concentration spell waits while one is held");
    // A creature already held is not held again.
    state.combatants[0].conditions.clear();
    state.combatants[1].status_effects = {{{"Paralyzed (Hold Person)", {}}, {}}};
    held = false;
    for (std::uint64_t revision = 1; revision <= 60; ++revision)
    {
        state.revision = revision;
        held = held || choose_demo_command(state, offered).verb == "hold_person";
    }
    check(!held, "No spell goes on a creature already under it");
}

void area_checks()
{
    auto caster = unit(1, Side::party, {1, 1});
    caster.spells = {"fireball"};
    auto fireball = command("fireball");
    fireball.label = "Fireball";
    fireball.aims_area = true;
    fireball.spell = {false, 150, 20, false};
    const std::vector<Command> offered{fireball, command("end")};
    const auto cast_ever = [&](const Snapshot &base)
    {
        auto state = base;
        for (std::uint64_t revision = 1; revision <= 30; ++revision)
        {
            state.revision = revision;
            if (choose_demo_command(state, offered).verb == "fireball")
            {
                return true;
            }
        }
        return false;
    };
    check(cast_ever(field({caster, unit(2, Side::opposition, {9, 4})})),
          "A Fireball is cast where it catches no ally");
    check(!cast_ever(
              field({caster, unit(2, Side::opposition, {9, 4}), unit(3, Side::party, {9, 6})})),
          "No Fireball where an ally could be caught");
    // Aiming: set off over the enemy, else moved to a safe one, else given up.
    auto state = field({caster, unit(2, Side::opposition, {9, 4}), unit(3, Side::party, {2, 6}),
                        unit(4, Side::opposition, {3, 6})});
    state.area_targeting =
        AreaTargeting{1, "fireball", {9, 4}, {{9, 4}, {9, 5}}, 1, fireball.spell};
    const std::vector<Command> aiming{command("area_move", 0, {9, 4}),
                                      command("area_move", 0, {3, 6}), command("area_cast"),
                                      command("spell_cancel")};
    check(choose_demo_command(state, aiming).verb == "area_cast",
          "The aimed area holds an enemy and no ally: cast");
    state.area_targeting->center = {3, 6};
    state.area_targeting->cells = {{3, 6}, {2, 6}};
    const auto moved = choose_demo_command(state, aiming);
    check(moved.verb == "area_move" && moved.destination == Cell{9, 4},
          "An ally in the area moves the aim to a safe enemy");

    // The rules aim an area at a creature's own square, so a large enemy whose
    // other squares alone are in range or in sight offers no aim: chosen anyway,
    // the cast would be given up and chosen again without end.
    auto large = unit(2, Side::opposition, {6, 1});
    large.footprint = {2, 2};
    auto short_reach = fireball;
    short_reach.spell = {false, 10, 0, false};
    const auto casts_at_infernal = [](const Snapshot &at, const Command &spell)
    {
        return choose_demo_command(at, {spell, command("end")}, Searching::by_sight,
                                   Difficulty::infernal)
                   .verb == spell.verb;
    };
    auto far_side = unit(1, Side::party, {9, 1});
    far_side.spells = {"fireball"};
    check(casts_at_infernal(field({far_side, unit(2, Side::opposition, {7, 1})}), short_reach),
          "A short area spell goes off at a target in range");
    check(!casts_at_infernal(field({far_side, large}), short_reach),
          "No cast where only a large enemy's other squares are in range");
    auto beyond_wall = unit(1, Side::party, {1, 0});
    beyond_wall.spells = {"fireball"};
    auto half_hidden = unit(2, Side::opposition, {5, 0});
    half_hidden.footprint = {2, 2};
    const auto corner = field({beyond_wall, half_hidden}, {{4, 0}});
    check(!rules::has_line_of_sight(corner.battlefield, {1, 0}, {5, 0}) &&
              rules::has_line_of_sight(corner.battlefield, {1, 0}, {5, 1}),
          "The wall hides the large enemy's own square and not all of it");
    check(!casts_at_infernal(corner, fireball),
          "No cast where only a large enemy's other squares are in sight");
}

void healing_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::party, {2, 1}, 4, 10),
                        unit(3, Side::party, {1, 2}, 2, 20), unit(4, Side::opposition, {9, 6})});
    const std::vector<Command> offered{command("cure_wounds", 2), command("cure_wounds", 3),
                                       command("end")};
    check(choose_demo_command(state, offered).target == 3, "The most wounded ally is healed");
    state.combatants[2].hit_points = 20;
    state.combatants[1].hit_points = 5;
    check(choose_demo_command(state, offered).target == 2,
          "An ally at exactly half its Hit Points is healed");
}

void reaction_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1})});
    state.reaction_pending = true;
    const std::vector<Command> offered{command("shield"), command("decline")};
    state.reaction_attack = Snapshot::AttackRoll{18, 12};
    check(choose_demo_command(state, offered).verb == "decline",
          "No Shield when +5 cannot turn the hit");
    state.reaction_attack = Snapshot::AttackRoll{15, 12};
    check(choose_demo_command(state, offered).verb == "shield", "Shield when +5 turns the hit");
    state.reaction_attack.reset();
    check(choose_demo_command(state, {command("decline"), command("uncanny_dodge")}).verb ==
              "uncanny_dodge",
          "Uncanny Dodge is chosen, not merely listed first");
}

void free_move_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1})});
    state.free_movement = FreeMovement{1, 15};
    const std::vector<Command> offered{command("end"), command("move", 0, {2, 1})};
    check(choose_demo_command(state, offered).verb == "move",
          "A free move is taken toward the enemy");
}

// The features #269 taught the computer, one rule each.
// Above Normal the walk goes for the weakest enemy it can strike this turn, not
// a weak one across the field; with none in reach, for the nearest.
void walk_checks()
{
    auto strong_near = unit(2, Side::opposition, {4, 1}, 30, 30);
    const auto weak_far = unit(3, Side::opposition, {11, 7}, 2, 30);
    const auto apart = field({unit(1, Side::party, {1, 1}), strong_near, weak_far});
    const std::vector<Command> steps{command("move", 0, {3, 1}), command("move", 0, {3, 3}),
                                     command("end")};
    for (const auto level : {Difficulty::hard, Difficulty::nightmare, Difficulty::infernal})
    {
        check(choose_demo_command(apart, steps, Searching::by_sight, level).destination ==
                  Cell{3, 1},
              "Above Normal it walks up to the enemy it can reach, not the weak one far off");
    }
    const auto weak_near = field(
        {unit(1, Side::party, {1, 1}), strong_near, unit(3, Side::opposition, {4, 3}, 2, 30)});
    const std::vector<Command> both{command("move", 0, {3, 0}), command("move", 0, {3, 3}),
                                    command("end")};
    check(choose_demo_command(weak_near, both, Searching::by_sight, Difficulty::nightmare)
                  .destination == Cell{3, 3},
          "Of the enemies in reach it walks up to the weakest");
    const auto none_in_reach =
        field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {6, 1}, 30, 30),
               unit(3, Side::opposition, {11, 7}, 2, 30)});
    const std::vector<Command> short_step{command("move", 0, {2, 1}), command("move", 0, {1, 2}),
                                          command("end")};
    check(choose_demo_command(none_in_reach, short_step, Searching::by_sight, Difficulty::nightmare)
                  .destination == Cell{2, 1},
          "With no enemy in reach it heads for the nearest");
}

void light_and_nick_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1}),
                        unit(3, Side::opposition, {1, 2})});
    state.combatants[0].last_target = 3;
    const std::vector<Command> offered{command("light_melee", 2), command("light_melee", 3),
                                       command("end")};
    const auto light = choose_demo_command(state, offered);
    check(light.verb == "light_melee" && light.target == 3,
          "A Light weapon's extra attack follows at the same target");
}

// Each level chooses weapons better than the last (docs/AI.md section 3).
void weapon_choice_checks()
{
    const auto play =
        [](const Snapshot &state, const std::vector<Command> &offered, Difficulty difficulty)
    {
        return choose_demo_command(state, offered, Searching::by_sight, difficulty);
    };
    auto beside = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1})});
    const auto weapons = [](int melee, int ranged, const std::string &verb = "ranged")
    {
        std::vector<Command> commands{command("melee", 2), command(verb, 2), command("end")};
        commands[0].expected_damage = melee;
        commands[1].expected_damage = ranged;
        return commands;
    };
    check(play(beside, weapons(100, 300), Difficulty::normal).verb == "melee",
          "Normal stays in melee beside an enemy, whatever the weapons promise");
    // Easy is arbitrary: over the fight's states it uses both weapons.
    std::vector<std::string> easy_verbs;
    for (std::uint64_t revision = 1; revision < 40; ++revision)
    {
        beside.revision = revision;
        easy_verbs.push_back(play(beside, weapons(500, 100), Difficulty::easy).verb);
    }
    check(std::count(easy_verbs.begin(), easy_verbs.end(), "melee") &&
              std::count(easy_verbs.begin(), easy_verbs.end(), "ranged"),
          "Easy picks its weapon at random, even a poor shot beside an enemy");
    beside.revision = 0;
    check(play(beside, weapons(500, 300), Difficulty::hard).verb == "melee",
          "Hard uses the stronger melee weapon");
    check(play(beside, weapons(300, 300), Difficulty::hard).verb == "melee",
          "A tie stays in melee");
    check(play(beside, weapons(100, 300), Difficulty::hard).verb == "ranged",
          "A ranged weapon that still promises more, penalty counted, is used");
    check(play(beside, weapons(100, 300, "shoot"), Difficulty::hard).verb == "shoot",
          "A carried bow that promises more is drawn");
    check(play(beside, weapons(100, 300, "throw"), Difficulty::hard).verb == "throw",
          "A thrown weapon that promises more is thrown");

    const auto apart = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1})});
    auto weak_throw = command("ranged", 2);
    weak_throw.expected_damage = 100;
    weak_throw.close_damage = 500;
    const std::vector<Command> closing{weak_throw, command("move", 0, {2, 1}), command("end")};
    check(play(apart, closing, Difficulty::normal).verb == "ranged",
          "Normal throws what it holds, however weak");
    check(play(apart, closing, Difficulty::hard).verb == "move",
          "Hard closes in when its shot is worth half its melee weapon or less");
    auto good_shot = weak_throw;
    good_shot.expected_damage = 300;
    check(play(apart, {good_shot, command("move", 0, {2, 1}), command("end")}, Difficulty::hard)
                  .verb == "ranged",
          "Hard shoots when the shot is worth more than half");
    check(play(apart, {weak_throw, command("end")}, Difficulty::hard).verb == "ranged",
          "Unable to get closer, it throws anyway");

    auto near = field({unit(1, Side::party, {4, 1}), unit(2, Side::opposition, {6, 1})});
    const std::vector<Command> retreat{good_shot, command("move", 0, {2, 1}), command("end")};
    check(play(near, retreat, Difficulty::hard).verb == "ranged", "Hard shoots from where it is");
    check(play(near, retreat, Difficulty::nightmare).verb == "move",
          "Nightmare backs away from a near enemy before shooting");
    const std::vector<Command> two_ways{good_shot, command("move", 0, {0, 1}),
                                        command("move", 0, {2, 1}), command("end")};
    check(play(near, two_ways, Difficulty::nightmare).destination == Cell{2, 1},
          "Nightmare backs off 4 squares, no further: a thrown weapon falls short beyond that");
    const auto walled =
        field({unit(1, Side::party, {4, 3}), unit(2, Side::opposition, {6, 3})}, {{3, 3}});
    check(!rules::has_line_of_sight(walled.battlefield, {2, 3}, {6, 3}) &&
              rules::has_line_of_sight(walled.battlefield, {3, 2}, {6, 3}),
          "The wall hides the target from one square and not the other");
    check(play(walled,
               {good_shot, command("move", 0, {2, 3}), command("move", 0, {3, 2}), command("end")},
               Difficulty::nightmare)
                  .destination == Cell{3, 2},
          "Nightmare does not back off out of sight of what it shoots");
    near.combatants[1].cell = {9, 1};
    check(play(near, retreat, Difficulty::nightmare).verb == "ranged",
          "Nightmare shoots once it is far enough away");
    const auto pair = field({unit(1, Side::party, {1, 3}), unit(2, Side::opposition, {6, 3}),
                             unit(3, Side::opposition, {6, 5})});
    const std::vector<Command> steps{command("move", 0, {5, 4}), command("move", 0, {5, 2}),
                                     command("end")};
    check(play(pair, steps, Difficulty::nightmare).destination == Cell{5, 2},
          "Nightmare comes up beside as few enemies as it can");

    const auto two = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1}, 40, 40),
                            unit(3, Side::opposition, {2, 2}, 6, 40)});
    auto strong = command("melee", 2);
    strong.expected_damage = 900;
    auto fatal = command("melee", 3);
    fatal.expected_damage = 500;
    check(play(two, {strong, fatal, command("end")}, Difficulty::infernal).target == 3,
          "Infernal picks the target it can most nearly fell");
    auto brute = unit(2, Side::opposition, {2, 1}, 20, 40);
    brute.melee_rating = 30;
    auto lesser = unit(3, Side::opposition, {2, 2}, 20, 40);
    lesser.melee_rating = 6;
    auto at_brute = command("melee", 2);
    at_brute.expected_damage = 500;
    auto at_lesser = command("melee", 3);
    at_lesser.expected_damage = 500;
    const auto threats = field({unit(1, Side::party, {1, 1}), brute, lesser});
    check(play(threats, {at_lesser, at_brute, command("end")}, Difficulty::infernal).target == 2,
          "Infernal strikes the enemy that does the most harm");
    auto holding = threats;
    holding.combatants[2].conditions.push_back({"Concentrating"});
    holding.combatants[2].melee_rating = 20;
    check(play(holding, {at_lesser, at_brute, command("end")}, Difficulty::infernal).target == 3,
          "Infernal counts a spell a hit could break");
    const auto threat_apart = field({unit(1, Side::party, {1, 1}), lesser, brute});
    auto walked = threat_apart;
    walked.combatants[1].cell = {4, 3};
    walked.combatants[2].cell = {4, 0};
    check(play(walked, {command("move", 0, {3, 3}), command("move", 0, {3, 0}), command("end")},
               Difficulty::infernal)
                  .destination == Cell{3, 0},
          "Infernal walks up to the enemy in reach that does the most harm");
    auto rogue_shot = command("ranged", 2);
    rogue_shot.expected_damage = 300;
    auto rogue_melee = command("melee", 2);
    rogue_melee.expected_damage = 100;
    check(play(beside, {rogue_shot, rogue_melee, command("cunning_disengage", 1), command("end")},
               Difficulty::infernal)
                  .verb == "cunning_disengage",
          "Infernal slips free with a Bonus Action before it shoots");
}

void thrown_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {6, 1})});
    state.combatants[0].melee_rating = 8;
    state.combatants[0].thrown_rating = 6;
    const std::vector<Command> offered{command("throw", 2), command("move", 0, {2, 1}),
                                       command("end")};
    check(choose_demo_command(state, offered).verb == "throw",
          "A javelin rated more than half the sword is thrown before closing in");
    state.combatants[1].cell = {2, 1};
    check(choose_demo_command(state, {command("throw", 2), command("melee", 2), command("end")})
                  .verb == "melee",
          "With the enemy beside it, it fights in melee");
}

void action_surge_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {4, 1})});
    state.combatants[0].action = false;
    const std::vector<Command> offered{command("action_surge", 1), command("end")};
    check(choose_demo_command(state, offered).verb == "action_surge",
          "A Fighter surges once its Action is spent");
    const auto hidden = field({state.combatants[0], unit(2, Side::opposition, {8, 1})}, divided());
    check(choose_demo_command(hidden, offered).verb == "end", "With no enemy in view, it does not");
}

void smite_checks()
{
    auto paladin = unit(1, Side::party, {1, 1});
    paladin.spells = {"divine_smite"};
    paladin.bonus_action = true;
    auto state = field({paladin, unit(2, Side::opposition, {2, 1})});
    const std::vector<Command> offered{command("divine_smite_2", 2), command("divine_smite", 2),
                                       command("end")};
    check(choose_demo_command(state, offered).verb == "end",
          "No slot is spent on a smite after an ordinary hit");
    state.combatants[0].critical_hit = true;
    check(choose_demo_command(state, offered).verb == "divine_smite",
          "A Critical Hit is smitten from the lowest slot");
}

void steady_aim_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1})});
    check(choose_demo_command(state, {command("melee", 2), command("steady_aim"), command("end")})
                  .verb == "steady_aim",
          "A Rogue that can attack without moving takes Steady Aim first");
    state.combatants[1].cell = {8, 1};
    check(choose_demo_command(state,
                              {command("move", 0, {2, 1}), command("steady_aim"), command("end")})
                  .verb == "move",
          "A Rogue that must move first does not");
}

void dash_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {9, 1})});
    for (const auto *verb : {"cunning_dash", "adrenaline_rush"})
    {
        check(
            choose_demo_command(state, {command("move", 0, {4, 1}), command(verb), command("end")})
                    .verb == verb,
            "Out of reach even with all its movement, it dashes with its Bonus Action");
        check(
            choose_demo_command(state, {command("move", 0, {8, 1}), command(verb), command("end")})
                    .verb == "move",
            "Within reach, it walks");
    }
}

void disengage_checks()
{
    auto state = field({unit(1, Side::opposition, {1, 1}), unit(2, Side::party, {2, 1})});
    state.combatants[0].panicked = true;
    const std::vector<Command> offered{command("move", 0, {0, 1}), command("disengage"),
                                       command("dash"), command("end")};
    check(choose_demo_command(state, offered).verb == "disengage",
          "A panicked monster with an enemy beside it Disengages before running");
    state.combatants[0].disengaged = true;
    check(choose_demo_command(state, offered).verb == "move", "Disengaged, it runs");
}

void dodge_checks()
{
    const auto state = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {2, 1})});
    check(choose_demo_command(state, {command("dodge"), command("end")}).verb == "dodge",
          "With nothing else to do and an enemy beside it, it Dodges");
}

void breath_checks()
{
    auto breath = command("breath_weapon_cone");
    breath.aims_area = true;
    breath.spell = {false, 15, 15, true};
    const std::vector<Command> offered{breath, command("melee", 2), command("end")};
    check(choose_demo_command(
              field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {3, 1})}), offered)
                  .verb == "breath_weapon_cone",
          "A Breath Weapon goes off before the attacks");
    check(
        choose_demo_command(field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {3, 1}),
                                   unit(3, Side::party, {2, 1})}),
                            offered)
                .verb == "melee",
        "Not where it would catch an ally");
}

void dragons_breath_checks()
{
    auto caster = unit(1, Side::party, {1, 1});
    caster.spells = {"dragons_breath"};
    auto state = field({caster, unit(2, Side::opposition, {6, 1})});
    auto spell = command("dragons_breath", 1);
    spell.label = "Dragon's Breath";
    spell.spell.concentration = true;
    bool cast = false;
    for (std::uint64_t revision = 1; revision <= 40 && !cast; ++revision)
    {
        state.revision = revision;
        const auto chosen = choose_demo_command(state, {spell, command("end")});
        cast = chosen.verb == "dragons_breath" && chosen.target == 1;
    }
    check(cast, "Dragon's Breath is cast as a helpful spell of priority 3");
}

void fire_and_healing_checks()
{
    auto state = field({unit(1, Side::party, {1, 1}, 10, 10), unit(2, Side::opposition, {2, 1})});
    const std::vector<Command> burning{command("extinguish"), command("melee", 2), command("end")};
    check(choose_demo_command(state, burning).verb == "melee", "Burning but healthy, it fights on");
    state.combatants[0].hit_points = 5;
    check(choose_demo_command(state, burning).verb == "extinguish",
          "Burning at half its Hit Points, it puts the fire out");
    check(choose_demo_command(state, {command("wholeness_of_body", 1), command("melee", 2)}).verb ==
              "wholeness_of_body",
          "Wholeness of Body heals at half its Hit Points");
}
} // namespace

int main()
{
    try
    {
        target_checks();
        difficulty_target_checks();
        sight_checks();
        stay_back_checks();
        ranged_checks();
        spell_checks();
        area_checks();
        healing_checks();
        reaction_checks();
        free_move_checks();
        walk_checks();
        light_and_nick_checks();
        thrown_checks();
        weapon_choice_checks();
        action_surge_checks();
        smite_checks();
        steady_aim_checks();
        dash_checks();
        disengage_checks();
        dodge_checks();
        breath_checks();
        dragons_breath_checks();
        fire_and_healing_checks();
        std::cout << "AI policy tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
