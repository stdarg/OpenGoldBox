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

void sight_checks()
{
    const std::vector<Command> offered{command("move", 0, {1, 2}), command("end")};
    // An ally and an enemy beyond the wall see each other; this creature sees
    // no one, so it waits.
    const auto watched = field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1}),
                                unit(3, Side::party, {8, 4}), unit(4, Side::opposition, {9, 4})},
                               divided());
    check(choose_demo_command(watched, offered).verb == "end",
          "A creature that sees no enemy holds its ground");
    // Even when no one on the field sees anyone; a stand-in for the player in
    // an automated run goes looking.
    const auto blind =
        field({unit(1, Side::party, {1, 1}), unit(2, Side::opposition, {8, 1})}, divided());
    check(choose_demo_command(blind, offered).verb == "end",
          "When no one sees anyone, it still holds its ground");
    check(choose_demo_command(blind, offered, Searching::party).verb == "move",
          "A stand-in for the player walks around the wall to find the enemy");
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
        sight_checks();
        stay_back_checks();
        ranged_checks();
        spell_checks();
        area_checks();
        healing_checks();
        reaction_checks();
        free_move_checks();
        light_and_nick_checks();
        thrown_checks();
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
