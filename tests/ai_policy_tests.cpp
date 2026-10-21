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
        std::cout << "AI policy tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
