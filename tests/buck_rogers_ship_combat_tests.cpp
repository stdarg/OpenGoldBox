// Buck Rogers ship combat, plan step B2 (docs/BUCK-ROGERS.md, "Ship combat";
// docs/games/countdown.md, "Ship combat"): the fight SHIP COMBAT starts and
// the repairs after it, on made-up ships with dice the test lays out, then on
// the games' own SHIPS.DAX where their folders are found. Each check names the
// original routine it follows.
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_ship_combat.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/dice.h"
#include "opengold/formats.h"
#include "installed_games.h"
#include "test_support.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::buck_rogers::Ship;
using opengold::original::Profession;
using opengold::original::Skill;
using opengold::original::TeamMember;
using Status = opengold::original::BuckRogersStatus;
using opengold::original::ShipAction;
using opengold::original::ShipCombat;
using opengold::original::ShipCombatStart;
using opengold::original::ShipDice;
using opengold::original::ShipEvent;
using opengold::original::ShipEventKind;
using opengold::original::ShipMemoryWrite;
using opengold::original::ShipOrder;
using opengold::original::ShipSide;
using opengold::original::ShipSystem;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;

constexpr auto countdown = GoldBoxGame::countdown_to_doomsday;
constexpr auto matrix = GoldBoxGame::matrix_cubed;

constexpr std::size_t hull = 0, sensors = 1, life_support = 3, fuel = 4, engines = 5;

// Dice the test lays out one by one. Past the last, every die shows `after`;
// with no `after`, running out fails the test.
struct LaidDice
{
    std::shared_ptr<std::deque<int>> rolls = std::make_shared<std::deque<int>>();
    std::optional<int> after;

    int operator()(int sides) const
    {
        if (rolls->empty())
        {
            check(after.has_value(), "The fight rolled a die the test did not lay out");
            return std::min(*after, sides);
        }
        const int roll = rolls->front();
        rolls->pop_front();
        check(roll >= 1 && roll <= sides, "A laid-out roll of " + std::to_string(roll) +
                                              " does not fit a d" + std::to_string(sides));
        return roll;
    }
};

LaidDice laid(std::initializer_list<int> rolls, std::optional<int> after = std::nullopt)
{
    LaidDice dice;
    dice.rolls->assign(rolls);
    dice.after = after;
    return dice;
}

void all_rolled(const LaidDice &dice, const std::string &what)
{
    check(dice.rolls->empty(),
          what + ": " + std::to_string(dice.rolls->size()) + " laid-out dice were not rolled");
}

Ship::WeaponMount mount(int kind, int count, int rounds = 0, int reloads = 0)
{
    return {static_cast<std::uint8_t>(kind), static_cast<std::uint8_t>(count),
            static_cast<std::uint8_t>(count), static_cast<std::uint8_t>(rounds),
            static_cast<std::uint8_t>(reloads)};
}

// A ship with every system at `strength`, Armor Class 6, no maneuver, no
// crew and no weapons.
Ship made_ship(const std::string &name, int strength, int tonnage, int speed)
{
    Ship ship;
    ship.name = name;
    ship.tonnage = tonnage;
    ship.system_status.fill(strength);
    ship.system_maximum.fill(strength);
    ship.armor_class = 6;
    ship.pilot_skill = 100;
    ship.system_skill = 70;
    ship.gunnery = 50;
    ship.speed = speed;
    ship.type = 6;
    ship.race = 1;
    return ship;
}

// The team's ship with a pilot's K-cannon.
Ship armed_party_ship()
{
    auto ship = made_ship("Party", 600, 150, 3);
    ship.weapons[0] = mount(6, 1, 6, 5);
    return ship;
}

// A Warrior of level 2: combat value 40, Dexterity 16 and every other
// ability 0, so a skill's value is its level; 20 Hit Points and a saving
// throw of 12 against a blast.
TeamMember member(const std::string &name)
{
    TeamMember crew_member;
    auto &character = crew_member.character;
    character.name = name;
    character.career = opengold::original::record_career(Profession::warrior);
    character.abilities[1] = 16;
    character.current_abilities[1] = 16;
    character.base_combat = 40;
    character.level = 2;
    character.hit_points = 20;
    character.hit_point_maximum = 20;
    character.saves[2] = 12;
    opengold::original::set_status(character, Status::okay);
    return crew_member;
}

// Gives the member `skill` at the value `value` (charhlp.pas SkillValue).
void give(TeamMember &crew_member, Skill skill, int value)
{
    auto &character = crew_member.character;
    const int score = opengold::original::skill_ability_score(character, skill);
    opengold::original::level_of(character.skills, skill) =
        static_cast<std::uint8_t>(value - score);
    check(opengold::original::skill_value(character, skill) == value,
          character.name + "'s skill takes its value");
}

TeamMember pilot(const std::string &name)
{
    auto crew_member = member(name);
    crew_member.character.pilot = 1;
    give(crew_member, Skill::pilot_rocket, 60);
    return crew_member;
}

TeamMember as_rocketjock(TeamMember crew_member)
{
    crew_member.character.career = opengold::original::record_career(Profession::rocket_jock);
    return crew_member;
}

TeamMember fallen(TeamMember crew_member, Status status)
{
    opengold::original::set_status(crew_member.character, status);
    crew_member.character.hit_points = 0;
    return crew_member;
}

Status status_of(const TeamMember &crew_member)
{
    return opengold::original::status_of(crew_member.character);
}

ShipCombatStart start_of(Ship party, Ship enemy, std::vector<TeamMember> crew,
                         GoldBoxGame game = countdown)
{
    return {.game = game,
            .party_ship = std::move(party),
            .enemy_ship = std::move(enemy),
            .enemy_ship_number = 1,
            .enemy_pilot_skill = 50,
            .enemy_to_hit = 18,
            .crew = std::move(crew)};
}

ShipCombat fight(Ship party, Ship enemy, std::vector<TeamMember> crew, const LaidDice &dice)
{
    return ShipCombat(start_of(std::move(party), std::move(enemy), std::move(crew)),
                      ShipDice(dice));
}

bool offers(const ShipCombat &combat, ShipAction action)
{
    const auto orders = combat.legal_orders();
    return std::any_of(orders.begin(), orders.end(),
                       [action](const ShipOrder &order)
                       {
                           return order.action == action;
                       });
}

bool offers(const ShipCombat &combat, const ShipOrder &wanted)
{
    const auto orders = combat.legal_orders();
    return std::find(orders.begin(), orders.end(), wanted) != orders.end();
}

void order(ShipCombat &combat, ShipAction action)
{
    combat.submit({.action = action});
}

// Every member still to act this round quits; the next round's first member
// is left to act.
void quit_round(ShipCombat &combat)
{
    std::optional<std::size_t> last;
    while (const auto acting = combat.acting_member())
    {
        if (last && *acting <= *last)
        {
            return;
        }
        last = acting;
        order(combat, ShipAction::quit);
    }
}

std::vector<ShipEvent> events_of(const std::vector<ShipEvent> &events, ShipEventKind kind)
{
    std::vector<ShipEvent> found;
    std::copy_if(events.begin(), events.end(), std::back_inserter(found),
                 [kind](const ShipEvent &event)
                 {
                     return event.kind == kind;
                 });
    return found;
}

std::optional<std::uint8_t> written(const std::vector<ShipMemoryWrite> &writes,
                                    std::uint16_t address)
{
    std::optional<std::uint8_t> value;
    for (const auto &write : writes)
    {
        if (write.address == address)
        {
            value = write.value;
        }
    }
    return value;
}

// The team pilot closes and the unarmed enemy of speed 3 closes too: the
// next round opens at range 2 with the pilot acting.
void close_to_range_two(ShipCombat &combat)
{
    order(combat, ShipAction::close);
    quit_round(combat);
    check(combat.range() == 2 && combat.acting_member() == 0, "The ships closed to range 2");
}

void the_tables_follow_gdconsts()
{
    using namespace opengold::original;
    // gdconsts.pas shipWeapons.
    const auto k_cannon = ship_weapon_facts(countdown, 6);
    check(k_cannon.maximum_range == 4 && k_cannon.damage == 100 && k_cannon.to_hit_bonus == -3 &&
              k_cannon.reload_rounds == 6 && k_cannon.name == "K-cannon",
          "A K-cannon reaches 4, hits for 100 at -3 and reloads 6 rounds");
    const auto laser = ship_weapon_facts(matrix, 1);
    check(laser.maximum_range == 6 && laser.damage == 10 && laser.to_hit_bonus == 1 &&
              laser.reload_rounds == 0,
          "A beam laser reaches 6, hits for 10 at +1 and needs no rounds");
    opengold::test::rejects(
        []
        {
            (void)ship_weapon_facts(countdown, 9);
        },
        "There is no ninth weapon kind");
    opengold::test::rejects(
        []
        {
            (void)ship_weapon_facts(GoldBoxGame::pool_of_radiance, 1);
        },
        "Ship combat is only the Buck Rogers games'");
    // gdconsts.pas systemHitChances.
    int total = 0;
    for (int system = 0; system <= static_cast<int>(ShipSystem::weapons); ++system)
    {
        total += ship_system_hit_chance(countdown, static_cast<ShipSystem>(system));
    }
    check(total == 100 && ship_system_hit_chance(countdown, ShipSystem::hull) == 28 &&
              ship_system_hit_chance(matrix, ShipSystem::engines) == 18,
          "Random hits fall on the hull 28 times in 100 and on the engines 18");
    // gdconsts.pas dexMissileAdjustments.
    check(buck_rogers_missile_adjustment(1) == -6 && buck_rogers_missile_adjustment(15) == 0 &&
              buck_rogers_missile_adjustment(16) == 1 && buck_rogers_missile_adjustment(22) == 4,
          "Dexterity adjusts a gunner from -6 to +4");
}

// shipcbt.pas ShipCombat: the experience by the enemy ship's type.
void experience_follows_the_enemy_ship()
{
    using opengold::original::ship_combat_experience;
    check(ship_combat_experience(countdown, 1) == 300 &&
              ship_combat_experience(countdown, 2) == 3000 &&
              ship_combat_experience(countdown, 3) == 12600 &&
              ship_combat_experience(matrix, 4) == 2500 &&
              ship_combat_experience(matrix, 5) == 4600,
          "Ships 1 to 5 are worth 300, 3000, 12600, 2500 and 4600");
    opengold::test::rejects(
        []
        {
            (void)ship_combat_experience(countdown, 6);
        },
        "SHIP COMBAT knows five enemy ships");
}

// charhlp.pas ShipSpeed and CanShipMove.
void damage_slows_a_ship()
{
    using opengold::original::ship_speed;
    auto ship = made_ship("Slow", 100, 50, 5);
    check(ship_speed(ship, 0) == 5 && ship_speed(ship, 2) == 7, "Boosts add to the speed");
    ship.system_status[hull] = 49;
    check(ship_speed(ship, 0) == 2, "A hull below half halves the speed");
    ship.system_status[hull] = 100;
    ship.system_status[engines] = 49;
    check(ship_speed(ship, 0) == 4, "Engines below half take 1 off");
    ship.system_status[engines] = 24;
    check(ship_speed(ship, 0) == 2, "Engines below a quarter take 3 off");
    ship.speed = 2;
    check(ship_speed(ship, 0) == 1, "A ship that can move makes at least 1");
    ship.system_status[fuel] = 0;
    check(ship_speed(ship, 0) == 0 && !opengold::original::ship_can_move(ship),
          "A ship without fuel cannot move");
}

// shipcbt.pas ShipCombat and PilotContest: the range opens at 8; a d10 each
// and the reactions decide the order, and the team that wins moves second.
void the_fight_opens_at_range_eight()
{
    auto dice = laid({3, 7});
    auto combat = fight(armed_party_ship(), made_ship("Enemy", 100, 50, 3), {pilot("Buck")}, dice);
    check(combat.range() == 8 && !combat.enemy_moves_first() && combat.acting_member() == 0 &&
              combat.pilot() == 0,
          "Losing the pilot roll, the team acts first, its pilot at the helm, at range 8");
    check(!offers(combat, ShipAction::fire) && offers(combat, ShipAction::close) &&
              offers(combat, ShipAction::withdraw) && !offers(combat, ShipAction::ram) &&
              !offers(combat, ShipAction::command),
          "At range 8 the K-cannon cannot reach; the pilot may close or withdraw");
    all_rolled(dice, "The lost pilot roll");

    // 7 - 3, plus the team ship's reaction 0 less the enemy's -1: the team
    // wins, so the enemy moves first and closes at once.
    auto won = laid({7, 3});
    auto enemy = made_ship("Enemy", 100, 50, 3);
    enemy.reaction = -1;
    auto second = fight(armed_party_ship(), enemy, {pilot("Buck")}, won);
    const auto events = second.take_events();
    check(
        second.enemy_moves_first() && events.front().kind == ShipEventKind::pilot_contest &&
            events.front().success && events_of(events, ShipEventKind::close).size() == 1 &&
            second.range() == 8 && second.acting_member() == 0,
        "Winning the pilot roll, the team lets the enemy move first; moves count at the round's end");
    order(second, ShipAction::close);
    check(second.acting_member() == 0 && !offers(second, ShipAction::close),
          "CLOSE leaves the pilot acting, and the ship moves once a round");
    order(second, ShipAction::quit);
    check(second.range() == 2, "Both ships closed 3: the range is 2");
    all_rolled(won, "The won pilot roll");
}

// shipcbt.pas TargetValue and FireWeapon: 60 less the target's Armor Class
// and the gunner's total of combat value, Dexterity, a Rocketjock's level,
// the team's 3, the weapon's bonus and the range.
void a_shot_needs_its_target_number()
{
    // 40, +1 for Dexterity 16, a Rocketjock's level 2, 3 and the K-cannon's
    // -3: 43, so a d20 of 11 hits Armor Class 6.
    auto rocketjock = as_rocketjock(pilot("Buck"));
    auto dice = laid({3, 7, 11, 1, 99});
    auto combat = fight(armed_party_ship(), made_ship("Enemy", 100, 50, 3), {rocketjock}, dice);
    close_to_range_two(combat);
    order(combat, ShipAction::fire);
    const auto shots = events_of(combat.take_events(), ShipEventKind::shot);
    check(shots.size() == 1 && shots[0].success && shots[0].amount == 100 &&
              combat.ship(ShipSide::party).weapons[0].ammunition == 5,
          "A d20 of 11 hits for the K-cannon's 100 and spends a round");
    check(combat.over() && combat.end().outcome == 0 &&
              combat.ship(ShipSide::enemy).system_status[hull] == 0,
          "The hit lands on the hull, a d100 of 1, and destroys the enemy: outcome 0");
    all_rolled(dice, "The Rocketjock's shot");

    // Without the level it needs 13: 12 misses.
    auto miss = laid({3, 7, 12});
    auto second = fight(armed_party_ship(), made_ship("Enemy", 100, 50, 3), {pilot("Buck")}, miss);
    close_to_range_two(second);
    order(second, ShipAction::fire);
    const auto missed = events_of(second.take_events(), ShipEventKind::shot);
    check(missed.size() == 1 && !missed[0].success && !second.over(),
          "Without a Rocketjock's level a d20 of 12 misses");
    all_rolled(miss, "The other pilot's shot");

    // Beyond range 4 a shot loses 5: a beam laser (+1) at range 5 needs 14.
    for (const int roll : {13, 14})
    {
        auto party = made_ship("Party", 600, 150, 3);
        party.weapons[0] = mount(1, 1);
        auto at_five = roll == 14 ? laid({3, 7, 14, 100}) : laid({3, 7, 13});
        auto far = fight(party, made_ship("Enemy", 100, 50, 3), {pilot("Buck")}, at_five);
        check(!offers(far, ShipAction::fire), "At range 8 the laser cannot reach");
        quit_round(far);
        check(far.range() == 5, "The enemy closed to range 5");
        order(far, ShipAction::fire);
        const auto shot = events_of(far.take_events(), ShipEventKind::shot);
        check(shot.size() == 1 && shot[0].success == (roll == 14),
              "At range 5 a beam laser needs a d20 of 14");
        all_rolled(at_five, "The long-range laser");
    }
}

// shipcbt.pas FireWeapon and DispatchWeaponHit: an aimed shot first needs an
// even chance, then strikes the system aimed at; a destroyed one passes the
// hit to the hull.
void targeting_aims_at_a_system()
{
    auto dice = laid({3, 7, 50, 15, 100});
    auto combat = fight(armed_party_ship(), made_ship("Enemy", 100, 50, 3), {pilot("Buck")}, dice);
    close_to_range_two(combat);
    combat.submit({.action = ShipAction::target, .system = ShipSystem::engines});
    const auto &enemy = combat.ship(ShipSide::enemy);
    check(enemy.system_status[engines] == 0 && enemy.system_status[hull] == 100,
          "An even chance of 50, then a d20 of 15, strikes the engines");
    check(!opengold::original::ship_can_move(enemy), "Without engines the enemy cannot move");
    all_rolled(dice, "The aimed shot");

    auto lost = laid({3, 7, 51});
    auto second = fight(armed_party_ship(), made_ship("Enemy", 100, 50, 3), {pilot("Buck")}, lost);
    close_to_range_two(second);
    second.submit({.action = ShipAction::target, .system = ShipSystem::hull});
    const auto shot = events_of(second.take_events(), ShipEventKind::shot);
    check(shot.size() == 1 && !shot[0].success,
          "Losing the even chance, a 51, the aimed shot misses");
    all_rolled(lost, "The lost even chance");

    auto blind = made_ship("Enemy", 200, 50, 3);
    blind.system_status[sensors] = 0;
    auto redirected = laid({3, 7, 1, 15, 100});
    auto third = fight(armed_party_ship(), blind, {pilot("Buck")}, redirected);
    close_to_range_two(third);
    third.submit({.action = ShipAction::target, .system = ShipSystem::sensors});
    check(third.ship(ShipSide::enemy).system_status[hull] == 100,
          "A hit on destroyed sensors falls on the hull");
    all_rolled(redirected, "The hit on a destroyed system");
}

// gdconsts.pas systemHitChances, by DispatchWeaponHit: a d100 walks the
// chances from the hull to the weapons.
void hits_fall_by_the_table()
{
    struct Case
    {
        int roll;
        std::size_t system;
    };

    for (const auto [roll, system] : {Case{28, hull}, Case{29, sensors}, Case{47, life_support},
                                      Case{69, fuel}, Case{70, engines}})
    {
        auto dice = laid({3, 7, 11, roll, 100});
        auto rocketjock = as_rocketjock(pilot("Buck"));
        auto combat = fight(armed_party_ship(), made_ship("Enemy", 200, 50, 3), {rocketjock}, dice);
        close_to_range_two(combat);
        order(combat, ShipAction::fire);
        check(combat.ship(ShipSide::enemy).system_status[system] == 100,
              "A d100 of " + std::to_string(roll) + " strikes system " + std::to_string(system));
        all_rolled(dice, "The table's hit");
    }

    // 88 to 100 strike the weapons: one working weapon, picked by a die of
    // their count; a 3 of 3 is the second of the missile mount's two. The
    // enemy's mounts are its crew's, and it has no crew to fire them.
    auto enemy = made_ship("Enemy", 200, 50, 3);
    enemy.weapons[0] = mount(0x81, 1);
    enemy.weapons[1] = mount(0x84, 2, 4, 2);
    auto rocketjock = as_rocketjock(pilot("Buck"));
    auto dice = laid({3, 7, 11, 88, 3, 100});
    auto combat = fight(armed_party_ship(), enemy, {rocketjock}, dice);
    close_to_range_two(combat);
    order(combat, ShipAction::fire);
    const auto &hit = combat.ship(ShipSide::enemy);
    check(hit.weapons[0].status == 1 && hit.weapons[1].status == 1,
          "A d100 of 88 strikes the weapons, and a d3 of 3 the missile mount");
    all_rolled(dice, "The hit on the weapons");
}

// shipcbt.pas RamAttempt: each hull takes the other ship's tonnage, unless
// the rammed ship dodges.
void ramming_trades_tonnage()
{
    auto dice = laid({3, 7, 100, 100});
    auto combat = fight(made_ship("Party", 600, 150, 4), made_ship("Enemy", 600, 50, 4),
                        {pilot("Buck")}, dice);
    order(combat, ShipAction::close);
    quit_round(combat);
    check(combat.range() == 0 && offers(combat, ShipAction::ram) &&
              !offers(combat, ShipAction::board) && !offers(combat, ShipAction::close),
          "At range 0 the pilot may ram a ship that can still move, not board it");
    order(combat, ShipAction::ram);
    check(combat.ship(ShipSide::party).system_status[hull] == 550 &&
              combat.ship(ShipSide::enemy).system_status[hull] == 450,
          "The 150-ton ship takes 50 off its hull, the 50-ton ship 150");
    all_rolled(dice, "The ram");

    // An enemy with the weaker hull tries to dodge: its pilot's 50 and ten for
    // each point of speed it has over the rammer.
    auto weaker = made_ship("Enemy", 600, 50, 4);
    weaker.system_status[hull] = 500;
    auto dodge = laid({3, 7, 50});
    auto second = fight(made_ship("Party", 600, 150, 4), weaker, {pilot("Buck")}, dodge);
    order(second, ShipAction::close);
    quit_round(second);
    order(second, ShipAction::ram);
    check(!events_of(second.take_events(), ShipEventKind::ram_dodged).empty() &&
              second.ship(ShipSide::party).system_status[hull] == 600,
          "A d100 of 50 against 50 dodges the ram");
    all_rolled(dodge, "The dodged ram");
}

// shipcbt.pas RamAttempt (DodgesRam) and SkillCheck: the original keeps the
// dodge chance in a byte, so a slow pilot's chance below 0 wraps round to a
// certain dodge. Here it fails (Bugs).
void a_hopeless_dodge_stays_hopeless()
{
    auto slow = made_ship("Enemy", 600, 50, 2);
    slow.system_status[hull] = 500;
    auto start = start_of(made_ship("Party", 600, 150, 4), slow, {pilot("Buck")});
    start.enemy_pilot_skill = 5;
    auto dice = laid({3, 7, 1, 100, 100});
    ShipCombat combat(start, ShipDice(dice));
    order(combat, ShipAction::close);
    quit_round(combat);
    order(combat, ShipAction::close);
    quit_round(combat);
    check(combat.range() == 0, "The faster team caught the slower enemy");
    order(combat, ShipAction::ram);
    check(combat.ship(ShipSide::enemy).system_status[hull] == 350,
          "Skill 5, less 20 for being 2 slower: even a d100 of 1 fails to dodge");
    all_rolled(dice, "The hopeless dodge");
}

// shipcbt.pas EnemyTurnBody and RamAttempt: an enemy with the stronger hull
// rams at range 1 or less, and the team's pilot may try to dodge; with no
// one at the helm the ship dodges by its own pilot value.
void the_enemy_rams_and_the_pilot_may_dodge()
{
    auto party = made_ship("Party", 600, 150, 4);
    party.system_status[hull] = 500;
    auto dice = laid({3, 7, 61, 100, 100});
    auto combat = fight(party, made_ship("Enemy", 600, 50, 4), {pilot("Buck")}, dice);
    order(combat, ShipAction::close);
    quit_round(combat);
    quit_round(combat);
    check(combat.awaits_dodge() && !combat.acting_member() && combat.legal_orders().empty(),
          "The enemy rams: the fight asks whether the pilot dodges");
    combat.answer_dodge(true);
    check(combat.ship(ShipSide::party).system_status[hull] == 450 &&
              combat.ship(ShipSide::enemy).system_status[hull] == 450 &&
              combat.acting_member() == 0,
          "Pilot Rocket 60 fails on 61: each hull takes the other's tonnage");
    all_rolled(dice, "The pilot's dodge");

    // No one at the helm: the ship dodges with 0, less 40 for being 4
    // slower, so a d100 of 1 fails.
    auto helmless = laid({3, 7, 1}, 100);
    auto second = fight(party, made_ship("Enemy", 600, 50, 8), {member("Wilma")}, helmless);
    check(offers(second, ShipAction::command), "With no pilot a member may take COMMAND");
    quit_round(second);
    quit_round(second);
    check(second.awaits_dodge(), "The enemy rams the ship no one steers");
    second.answer_dodge(true);
    const auto rammed = second.take_events();
    check(second.ship(ShipSide::party).system_status[hull] == 450 &&
              events_of(rammed, ShipEventKind::ram_dodged).empty() &&
              !events_of(rammed, ShipEventKind::rammed).empty(),
          "An empty helm dodges with 0: the ram lands");

    // The original's way (ShipCombatOptions): the ship's own 100, less 40.
    auto own_value = laid({3, 7, 60});
    auto start = start_of(party, made_ship("Enemy", 600, 50, 8), {member("Wilma")});
    start.options.empty_helm_dodges = true;
    ShipCombat third(start, ShipDice(own_value));
    quit_round(third);
    quit_round(third);
    check(third.awaits_dodge(), "The enemy rams the ship no one steers");
    third.answer_dodge(true);
    check(third.ship(ShipSide::party).system_status[hull] == 500 &&
              !events_of(third.take_events(), ShipEventKind::ram_dodged).empty(),
          "With the original's way the ship's own value dodges on a d100 of 60");
    all_rolled(own_value, "The helmless dodge by the ship's own value");
}

// shipcbt.pas AttemptSpeedBoost: Repair Nuclear Engine at half the skill; a
// success adds a point of speed, at most twice a fight, a failure costs the
// engines 1 to 100.
void boosting_the_engines()
{
    auto engineer = member("Huer");
    give(engineer, Skill::repair_nuclear_engine, 60);
    auto dice = laid({3, 7, 30, 31, 60, 100, 1});
    auto combat = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                        {pilot("Buck"), engineer}, dice);
    order(combat, ShipAction::quit);
    check(offers(combat, ShipAction::boost_engines) && !offers(combat, ShipAction::close),
          "A crew member with the skill may boost; only the pilot moves the ship");
    order(combat, ShipAction::boost_engines);
    check(combat.boosts(ShipSide::party) == 1, "A d100 of 30 against half of 60 boosts");
    order(combat, ShipAction::quit);
    order(combat, ShipAction::boost_engines);
    check(combat.boosts(ShipSide::party) == 1 &&
              combat.ship(ShipSide::party).system_status[engines] == 40,
          "A 31 fails, and the engines take a d100 of 60");
    order(combat, ShipAction::quit);
    order(combat, ShipAction::boost_engines);
    order(combat, ShipAction::quit);
    check(combat.boosts(ShipSide::party) == 2 && !offers(combat, ShipAction::boost_engines),
          "After two boosts the engines boost no more");
    check(opengold::original::ship_speed(combat.ship(ShipSide::party),
                                         combat.boosts(ShipSide::party)) == 4,
          "Speed 3, two boosts, and 1 off for engines below half: 4");
    all_rolled(dice, "The boosts");
}

// shipcbt.pas AttemptJuryRig and RemoveJuryRigs: Jury Rig at the skill
// mends a weapon or a system for the fight; a system once a fight.
void jury_rigging_lasts_the_fight()
{
    auto party = made_ship("Party", 100, 150, 3);
    party.system_status[engines] = 50;
    party.system_status[life_support] = 0;
    party.weapons[0] = mount(0x81, 1);
    party.weapons[0].status = 0;
    auto engineer = member("Huer");
    give(engineer, Skill::jury_rig, 70);
    auto dice = laid({3, 7, 70, 7, 8, 1, 10, 10, 70});
    auto combat = fight(party, made_ship("Enemy", 100, 50, 1), {pilot("Buck"), engineer}, dice);
    order(combat, ShipAction::quit);
    check(offers(combat, {.action = ShipAction::jury_rig, .system = ShipSystem::engines}) &&
              offers(combat,
                     {.action = ShipAction::jury_rig, .system = ShipSystem::weapons, .mount = 0}) &&
              !offers(combat, {.action = ShipAction::jury_rig, .system = ShipSystem::fuel}),
          "JURY RIG offers each damaged weapon and system but fuel");
    combat.submit({.action = ShipAction::jury_rig, .system = ShipSystem::engines});
    check(combat.ship(ShipSide::party).system_status[engines] == 65,
          "A d100 of 70 rigs the engines by 2d10, 15");
    order(combat, ShipAction::quit);
    check(!offers(combat, {.action = ShipAction::jury_rig, .system = ShipSystem::engines}),
          "A rigged system is not rigged again");
    combat.submit({.action = ShipAction::jury_rig, .system = ShipSystem::life_support});
    combat.submit({.action = ShipAction::withdraw});
    order(combat, ShipAction::quit);
    combat.submit({.action = ShipAction::jury_rig, .system = ShipSystem::weapons, .mount = 0});
    check(combat.ship(ShipSide::party).system_status[life_support] == 20 &&
              combat.ship(ShipSide::party).weapons[0].status == 1,
          "Life support rigged from 0 to 20, the laser back in working order");
    combat.submit({.action = ShipAction::withdraw});
    quit_round(combat);
    check(combat.over(), "The team withdrew beyond the scanners");
    const auto &after = combat.end().party_ship;
    check(after.system_status[engines] == 50 && after.weapons[0].status == 0,
          "As the fight ends the rigged points come off");
    check(after.system_status[life_support] == 0,
          "A system rigged from 0 goes back to 0 (the original keeps it mended; Bugs)");
    all_rolled(dice, "The jury-rigging");
}

// shipcbt.pas ShipCombat: an enemy beyond range 8 is lost; the experience
// with it, outcome 1, and the saved enemy cleared.
void withdrawing_past_the_scanners()
{
    auto dice = laid({3, 7});
    auto combat = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 1),
                        {pilot("Buck")}, dice);
    order(combat, ShipAction::withdraw);
    order(combat, ShipAction::quit);
    check(combat.over(), "Withdrawing 3 while the enemy closes 1: range 10, the enemy is lost");
    const auto end = combat.end();
    check(end.outcome == 1 && end.experience == 0 && !end.boarded && end.repair_pending,
          "Lost from the scanners: outcome 1, no experience, the repairs still follow");
    const auto memory = opengold::original::ship_combat_memory(end);
    check(written(memory, 0x4BBA) == 1 && written(memory, 0x4D6C) == 0 &&
              written(memory, 0x4D81) == 0 && written(memory, 0x4D7C) == 0,
          "The script reads outcome 1 at 4BBAh and a cleared enemy at 4D6Ch");
    all_rolled(dice, "The withdrawal");
}

// shipcbt.pas DamageShip and DamageCrew: a hull at 0 destroys the ship, and
// the team's crew with it, those already down too (Bugs).
void the_crew_dies_with_its_ship()
{
    auto dice = laid({3, 7, 100, 100});
    auto combat = fight(made_ship("Party", 40, 150, 4), made_ship("Enemy", 600, 50, 4),
                        {pilot("Buck"), fallen(member("Wilma"), Status::unconscious)}, dice);
    order(combat, ShipAction::close);
    quit_round(combat);
    order(combat, ShipAction::ram);
    check(combat.over() && combat.end().outcome == 1,
          "Ramming a heavier hull destroys the team's ship");
    const auto crew = combat.crew();
    check(std::all_of(crew.begin(), crew.end(),
                      [](const TeamMember &crew_member)
                      {
                          return status_of(crew_member) == Status::dead;
                      }),
          "Every member aboard dies, the unconscious too");
    all_rolled(dice, "The destroyed ship");
}

// shipcbt.pas PlayerTurnBody: BOARD at range 1 or less when the enemy can
// no longer move; outcome 0 and post-combat mode 1 for the script's fight on
// the enemy's decks, with no repairs or treatment here.
void boarding_hands_over_to_the_script()
{
    auto crippled = made_ship("Enemy", 100, 50, 3);
    crippled.system_status[engines] = 0;
    crippled.crew = 20;
    crippled.weapons[0] = mount(0x81, 2);
    crippled.weapons[0].status = 1;
    auto dice = laid({3, 7}, 100);
    auto combat = fight(made_ship("Party", 100, 150, 4), crippled, {pilot("Buck")}, dice);
    order(combat, ShipAction::close);
    quit_round(combat);
    order(combat, ShipAction::close);
    quit_round(combat);
    check(combat.range() == 0 && offers(combat, ShipAction::board), "BOARD is open at range 0");
    order(combat, ShipAction::board);
    const auto end = combat.end();
    check(end.outcome == 0 && end.boarded && !end.repair_pending && !end.treatment_pending &&
              end.experience == 300,
          "Boarding ends the fight: outcome 0, the experience kept for later");
    const auto memory = opengold::original::ship_combat_memory(end);
    check(
        written(memory, 0x4D7C) == 1 && written(memory, 0x4BBA) == 0 &&
            written(memory, 0x4D6C + 2 * engines) == 0 && written(memory, 0x4D6C) == 100 &&
            written(memory, 0x4D78) == 6 && written(memory, 0x4D7B) == 42 &&
            written(memory, 0x4D81) == 1,
        "Post-combat mode 1, and the enemy's systems, type, gunnery and one working weapon for the salvage");
    check(written(memory, 0x4D79).has_value() && written(memory, 0x4D00) == 5 &&
              written(memory, 0x4B80) == 0,
          "The enemy's crew, the team's ship at 4D00h and the treatment flag are written");
}

// shipcbt.pas EnemyTurnBody, ReportEnemyDamage, DamageShip and DamageCrew:
// a burst of enemy fire hurts each okay member a fifth of its damage percent
// of the time, unless the member leaps clear; life support lost hurts every
// okay member once, with no saving throw. A pilot who falls leaves the helm.
void enemy_fire_hurts_the_crew()
{
    auto enemy = made_ship("Enemy", 600, 50, 3);
    enemy.weapons[0] = mount(6, 1, 6, 5);
    auto buck = pilot("Buck");
    buck.character.hit_points = 10;
    auto dice = laid({7, 3, 15, 50, 20, 1, 9, 21, 100, 4, 50});
    auto combat = fight(made_ship("Party", 100, 150, 3), enemy, {buck, member("Wilma")}, dice);
    check(combat.enemy_moves_first(), "The team won the pilot roll");
    order(combat, ShipAction::close);
    order(combat, ShipAction::quit);
    order(combat, ShipAction::quit);
    const auto events = combat.take_events();
    check(combat.ship(ShipSide::party).system_status[life_support] == 0,
          "The K-cannon's 15 hits, and a d100 of 50 strikes life support");
    check(status_of(combat.crew()[0]) == Status::unconscious && !combat.pilot() &&
              combat.crew()[1].character.hit_points == 15,
          "Buck fails 20%, then his saving throw, and takes 10; life support costs Wilma 5");
    check(events_of(events, ShipEventKind::member_hurt).size() == 2 &&
              events_of(events, ShipEventKind::member_leaps_clear).empty(),
          "Two members hurt, none leaping clear");
    check(combat.acting_member() == 1 && offers(combat, ShipAction::command),
          "With the pilot down, Wilma may take COMMAND");
    order(combat, ShipAction::command);
    check(combat.pilot() == 1 && combat.acting_member() == 1 && offers(combat, ShipAction::close),
          "Wilma takes the helm and acts on as the pilot");
    all_rolled(dice, "The enemy's burst");
}

// shipcbt.pas PlayerTurnBody: COMMAND once a round; the pilot replaced
// stands down (the original leaves both marked; Bugs). The Rule Book offers
// COMMAND only while the pilot is dead or incapacitated; the original once a
// round to anyone (ShipCombatOptions).
void command_changes_the_pilot()
{
    auto rule_book_dice = laid({3, 7});
    auto rule_book = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                           {pilot("Buck"), member("Wilma")}, rule_book_dice);
    order(rule_book, ShipAction::quit);
    check(rule_book.acting_member() == 1 && !offers(rule_book, ShipAction::command),
          "With Buck well at the helm, Wilma may not take COMMAND");
    all_rolled(rule_book_dice, "No COMMAND with the pilot well");

    auto dice = laid({3, 7});
    auto start = start_of(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                          {pilot("Buck"), member("Wilma")});
    start.options.command_every_round = true;
    ShipCombat combat(start, ShipDice(dice));
    order(combat, ShipAction::quit);
    order(combat, ShipAction::command);
    check(combat.pilot() == 1 && combat.crew()[1].character.pilot != 0 &&
              combat.crew()[0].character.pilot == 0,
          "With the original's way Wilma takes over; Buck stands down");
    order(combat, ShipAction::quit);
    check(combat.acting_member() == 0 && offers(combat, ShipAction::command),
          "Next round Buck may take COMMAND back");
    all_rolled(dice, "The change of pilot");
}

// viewchar.pas FindPatients and TreatPatients: AID bandages the dying.
void aid_bandages_the_dying()
{
    auto medic = member("Dr Huer");
    give(medic, Skill::first_aid, 30);
    auto dice = laid({3, 7});
    auto combat = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                        {medic, fallen(member("Wilma"), Status::dying)}, dice);
    check(offers(combat, ShipAction::aid), "A member with First Aid may AID the dying");
    order(combat, ShipAction::aid);
    check(status_of(combat.crew()[1]) == Status::unconscious, "Bandaged, Wilma is unconscious");
    check(!offers(combat, ShipAction::aid), "No one is left to aid");
    all_rolled(dice, "The aid");
}

// charhlp.pas ProcessDyingCharacters: the dying fall into a coma after ten
// rounds.
void the_dying_fall_into_a_coma()
{
    auto dice = laid({3, 7});
    auto combat = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                        {pilot("Buck"), fallen(member("Wilma"), Status::dying)}, dice);
    for (int round = 1; round <= 10; ++round)
    {
        check(status_of(combat.crew()[1]) == Status::dying, "Wilma is still dying");
        if (offers(combat, ShipAction::close))
        {
            order(combat, ShipAction::close);
        }
        quit_round(combat);
    }
    check(!combat.over() && status_of(combat.crew()[1]) == Status::comatose,
          "After the tenth round Wilma is comatose");
    all_rolled(dice, "The dying");
}

// shipcbt.pas PlayerTurnBody: SENSORS at twice Sensor Operation; LOAD a
// mount that is out of rounds.
void sensors_and_loading()
{
    auto operator_ = member("Wilma");
    give(operator_, Skill::sensor_operation, 40);
    auto party = made_ship("Party", 100, 150, 3);
    party.weapons[0] = mount(0x84, 1, 0, 2);
    auto dice = laid({3, 7, 80});
    auto combat = fight(party, made_ship("Enemy", 100, 50, 3), {operator_, member("Kane")}, dice);
    check(offers(combat, ShipAction::sensors) &&
              offers(combat, {.action = ShipAction::load, .mount = 0}),
          "Wilma may use the sensors or load the empty missile mount");
    order(combat, ShipAction::sensors);
    const auto scanned = events_of(combat.take_events(), ShipEventKind::sensors);
    check(scanned.size() == 1 && scanned[0].success, "A d100 of 80 against twice 40 succeeds");
    check(!offers(combat, ShipAction::sensors), "Kane has no Sensor Operation");
    combat.submit({.action = ShipAction::load, .mount = 0});
    const auto &missiles = combat.ship(ShipSide::party).weapons[0];
    check(missiles.ammunition == 4 && missiles.reloads == 1,
          "A reload adds the missile mount's 4 rounds");
    all_rolled(dice, "The sensors and the reload");
}

// shipcbt.pas ShipCombat: with no member able to act the rounds go on, the
// enemy's turn each; the fight waits for play_round.
void a_helpless_crew_waits_for_the_round()
{
    auto dice = laid({3, 7});
    auto combat = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3),
                        {fallen(member("Buck"), Status::unconscious)}, dice);
    check(combat.awaits_round() && !combat.acting_member(), "No one can act");
    combat.play_round();
    check(combat.awaits_round() && combat.range() == 5,
          "The enemy closed while the crew lay helpless");
    all_rolled(dice, "The helpless round");
}

// The fight the original never ends: no member can act, and the enemy,
// unarmed with its engines gone, can neither finish the ship nor get away.
// After a helpless round in which it did nothing that could change the
// fight, it breaks off: outcome 1, no experience, as when it is lost
// (docs/games/countdown.md, Bugs).
void a_fight_no_one_can_end_ends()
{
    auto stranded = made_ship("Enemy", 100, 50, 3);
    stranded.system_status[engines] = 0;
    auto dice = laid({3, 7});
    auto combat = fight(made_ship("Party", 100, 150, 3), stranded,
                        {fallen(member("Buck"), Status::unconscious)}, dice);
    check(combat.awaits_round(), "No one can act");
    combat.play_round();
    check(combat.over(), "A round in which nothing could change ends the fight");
    const auto end = combat.end();
    check(end.outcome == 1 && end.experience == 0 && !end.boarded && end.repair_pending,
          "The enemy broke off: outcome 1 and no experience");
    check(events_of(combat.take_events(), ShipEventKind::breaks_off).size() == 1,
          "The fight says the enemy broke off");
    all_rolled(dice, "The endless fight");

    // An unarmed enemy that can move, its hull too weak to ram, closes, holds
    // off to range 3 and then waits there: once a round changes nothing, it
    // breaks off.
    auto closing = laid({3, 7});
    auto second = fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 8),
                        {fallen(member("Buck"), Status::unconscious)}, closing);
    second.play_round();
    check(!second.over() && second.range() == 0, "The enemy closes to range 0");
    second.play_round();
    check(!second.over() && second.range() == 3, "Holding, it withdraws 3");
    second.play_round();
    check(second.over() && second.end().outcome == 1, "Waiting at range 3, it breaks off");
    all_rolled(closing, "The enemy that waits");
}

// AID in a ship fight treats as AID on the ground (viewchar.pas FindPatients
// and TreatPatients, buck_rogers_upkeep.h): the same patients and dice give
// the same result.
void ship_aid_is_ground_aid()
{
    auto medic = member("Dr Huer");
    give(medic, Skill::first_aid, 30);
    give(medic, Skill::treat_stun, 50);
    auto stunned = member("Kane");
    stunned.stunned = true;
    const std::vector team{medic, fallen(member("Wilma"), Status::dying), stunned};
    for (const int stun_roll : {50, 51})
    {
        auto ship_dice = laid({3, 7, stun_roll});
        auto combat =
            fight(made_ship("Party", 100, 150, 3), made_ship("Enemy", 100, 50, 3), team, ship_dice);
        order(combat, ShipAction::aid);
        all_rolled(ship_dice, "The ship's AID");

        auto ground = team;
        const auto patients = opengold::original::find_patients(0, ground);
        check(patients == std::vector<std::size_t>{1, 2}, "The ground's AID sees both patients");
        const auto ground_dice = laid({stun_roll});
        (void)opengold::original::treat_patients(0, ground, patients,
                                                 opengold::original::DieRoll(ground_dice));
        all_rolled(ground_dice, "The ground's AID");
        for (std::size_t member = 0; member < team.size(); ++member)
        {
            const auto &aboard = combat.crew()[member];
            check(status_of(aboard) == status_of(ground[member]) &&
                      aboard.character.enabled == ground[member].character.enabled &&
                      aboard.stunned == ground[member].stunned,
                  "AID aboard and on the ground leave " + aboard.character.name + " alike");
        }
        check(status_of(combat.crew()[1]) == Status::unconscious &&
                  combat.crew()[2].stunned == (stun_roll > 50),
              "Wilma is bandaged; Kane's stun ends on a d100 of 50 against 50, not 51");
    }
}

// shipcbt.pas EnemyFireWorthwhile: at range 1 or less the enemy fires rather
// than rams when its weapons outweigh the team pilot's dodge. The original
// adds the weapons up in a byte read as a signed one, so a pilot's weapons
// worth 150 count as less than nothing and the enemy always rams (Bugs).
void a_well_armed_enemy_fires_rather_than_rams()
{
    for (const int pilot_skill : {50, 30})
    {
        auto cruiser = made_ship("Cruiser", 600, 150, 2);
        cruiser.weapons[0] = mount(1, 5);
        cruiser.weapons[1] = mount(6, 1, 6, 5);
        auto party = made_ship("Party", 600, 150, 4);
        party.system_status[hull] = 500;
        auto buck = pilot("Buck");
        give(buck, Skill::pilot_rocket, pilot_skill);
        auto start = start_of(party, cruiser, {buck});
        start.enemy_to_hit = 16;
        auto dice = laid({3, 7}, 1);
        ShipCombat combat(start, ShipDice(dice));
        order(combat, ShipAction::close);
        quit_round(combat);
        order(combat, ShipAction::close);
        quit_round(combat);
        check(combat.range() == 0, "The ships met");
        (void)combat.take_events();
        quit_round(combat);
        if (pilot_skill == 50)
        {
            check(!combat.awaits_dodge() && combat.acting_member() == 0,
                  "Against a pilot of 50 the cruiser's 150 of weapons fire instead of ramming");
        }
        else
        {
            check(combat.awaits_dodge(), "Against a pilot of 30 the cruiser rams");
        }
    }
}

// postcbt.pas RepairShip: the experience shared among the enabled members;
// each damaged system but fuel mended by the first okay member whose repair
// skill succeeds, by the system's d10s; each broken weapon likewise; and,
// when experience was shared, 9 to 15 sixteenths of the enemy's fuel and 1
// to 16 more, up to the tanks' room, the enemy's tanks emptied.
void repairs_follow_the_fight()
{
    auto party = made_ship("Party", 600, 150, 3);
    party.system_maximum[sensors] = 150;
    party.system_status[sensors] = 100;
    party.system_status[hull] = 500;
    party.system_maximum[fuel] = 450;
    party.system_status[fuel] = 300;
    party.weapons[0] = mount(0x81, 2);
    party.weapons[0].status = 1;
    auto huer = member("Huer");
    give(huer, Skill::repair_rocket_hull, 50);
    give(huer, Skill::repair_weapon, 30);
    auto wilma = member("Wilma");
    give(wilma, Skill::repair_electrical, 40);
    const std::vector crew{huer, wilma, fallen(member("Kane"), Status::unconscious)};
    auto enemy = made_ship("Enemy", 160, 50, 3);

    LaidDice dice;
    dice.rolls->push_back(50);
    dice.rolls->insert(dice.rolls->end(), 20, 5);
    dice.rolls->insert(dice.rolls->end(), {1, 40, 10, 10, 10, 10, 31, 1, 2, 3, 16});
    const auto report =
        opengold::original::repair_ship(countdown, party, crew, 3000, &enemy, false, {}, dice);
    check(report.experience_share == 1500, "3000 shared between the two enabled members");
    check(
        party.system_status[hull] == 600 && party.system_status[sensors] == 140,
        "Repair Rocket Hull's 20d10 mend the hull to whole; Wilma's Repair Electrical 4d10 the sensors");
    check(party.weapons[0].status == 1, "No one's Repair Weapon succeeds: the laser stays broken");
    check(report.fuel_taken == 136 && party.system_status[fuel] == 436 &&
              enemy.system_status[fuel] == 0,
          "12 sixteenths of 160 and a d16 of 16: 136 fuel taken, the enemy's emptied");
    check(report.repairs.size() == 2 && report.repairs[0].member == 0 &&
              report.repairs[1].member == 1,
          "Two repairs, by Huer and Wilma");
    all_rolled(dice, "The repairs");

    // After a boarding the captured ship's fuel is transferred, as the Rule
    // Book says, from the wreck the fight's end saved; with the option the
    // repairs take none, as the original (Countdown; Matrix Cubed reads a ship
    // that is gone).
    for (const auto game : {countdown, matrix})
    {
        auto boarded = made_ship("Party", 600, 150, 3);
        boarded.system_status[fuel] = 100;
        auto captured = made_ship("Enemy", 160, 50, 3);
        auto transfer = laid({1, 4, 16});
        const auto rule_book = opengold::original::repair_ship(game, boarded, crew, 300, &captured,
                                                               true, {}, transfer);
        check(
            rule_book.experience_share == 150 && rule_book.fuel_taken == 136 &&
                boarded.system_status[fuel] == 236 && captured.system_status[fuel] == 0,
            "After a boarding 12 sixteenths of the captured ship's 160 and a d16 of 16 come aboard");
        all_rolled(transfer, "The fuel after a boarding");

        boarded.system_status[fuel] = 100;
        auto wreck = made_ship("Enemy", 160, 50, 3);
        auto none = laid({});
        const auto original = opengold::original::repair_ship(
            game, boarded, crew, 300, &wreck, true, {.no_boarding_fuel = true}, none);
        check(original.experience_share == 150 && original.fuel_taken == 0 &&
                  boarded.system_status[fuel] == 100 && wreck.system_status[fuel] == 160,
              "With the original's way, a boarding brings no fuel");
        all_rolled(none, "No fuel after a boarding");
    }

    // The tanks take no more than their room.
    auto nearly_full = made_ship("Party", 450, 150, 3);
    nearly_full.system_status[fuel] = 440;
    auto full_enemy = made_ship("Enemy", 160, 50, 3);
    auto topped = laid({4, 4, 1});
    const auto topped_up = opengold::original::repair_ship(countdown, nearly_full, crew, 300,
                                                           &full_enemy, false, {}, topped);
    check(topped_up.fuel_taken == 10 && nearly_full.system_status[fuel] == 450,
          "The tanks fill to their 450");
    all_rolled(topped, "The topped-up tanks");
}

// The end's memory after a kill: outcome 0, mode 0, the wreck's state for
// the script's salvage (shipcbt.pas ShipCombat).
void a_kill_saves_the_wreck_for_salvage()
{
    auto rocketjock = as_rocketjock(pilot("Buck"));
    auto enemy = made_ship("Enemy", 100, 50, 3);
    enemy.crew = 0x0102;
    enemy.weapons[1] = mount(0x84, 3, 4, 1);
    auto dice = laid({3, 7, 11, 1, 99});
    auto combat = fight(armed_party_ship(), enemy, {rocketjock}, dice);
    close_to_range_two(combat);
    order(combat, ShipAction::fire);
    const auto end = combat.end();
    check(end.outcome == 0 && !end.boarded && end.repair_pending && end.experience == 300,
          "The wreck is the team's: outcome 0, 300 experience, repairs to follow");
    const auto memory = opengold::original::ship_combat_memory(end);
    check(written(memory, 0x4BBA) == 0 && written(memory, 0x4D7C) == 0 &&
              written(memory, 0x4D6C) == 0 && written(memory, 0x4D6E) == 100 &&
              written(memory, 0x4D79) == 0x02 && written(memory, 0x4D7A) == 0x01 &&
              written(memory, 0x4D81) == 3 && written(memory, 0x4D82) == 0,
          "The hull's 0, the sensors' 100, the crew of 258 and three working weapons are saved");
    const auto ship_bytes = opengold::buck_rogers::encode_ship(end.party_ship);
    check(ship_bytes && written(memory, 0x4D00 + 60 + 4) == (*ship_bytes)[60 + 4] &&
              (*ship_bytes)[60 + 4] == 5,
          "The team's ship at 4D00h keeps its K-cannon's 5 rounds left");
    all_rolled(dice, "The kill");
}

void encode_ship_writes_every_field()
{
    auto ship = armed_party_ship();
    ship.maneuver = -2;
    ship.reaction = -1;
    ship.crew = 300;
    const auto bytes = opengold::buck_rogers::encode_ship(ship);
    check(bytes && bytes->size() == Ship::size, "A ship encodes to 108 bytes");
    const auto again = opengold::buck_rogers::decode_ship(*bytes);
    check(again && again->name == "Party" && again->maneuver == -2 && again->reaction == -1 &&
              again->crew == 300 && again->weapons[0].ammunition == 6 &&
              again->system_status == ship.system_status,
          "The ship decodes as it was encoded");
    ship.name = "A name past fifteen";
    check(!opengold::buck_rogers::encode_ship(ship), "A name longer than 15 does not fit");
}

std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "SHIPS.DAX"))
    {
        std::cout << opengold::game_title(game) << "'s files are not here; they are not read.\n";
        return std::nullopt;
    }
    return folder;
}

Bytes file_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "Open " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

Ship ship_of(const opengold::DaxDecodeResult &archive, unsigned id, Bytes *raw = nullptr)
{
    const auto found = std::find_if(archive.records.begin(), archive.records.end(),
                                    [id](const opengold::DaxRecord &record)
                                    {
                                        return record.id == id;
                                    });
    check(found != archive.records.end(), "SHIPS.DAX holds record " + std::to_string(id));
    if (raw)
    {
        *raw = found->bytes;
    }
    const auto ship = opengold::buck_rogers::decode_ship(found->bytes);
    check(ship.has_value(), "Record " + std::to_string(id) + " is a ship");
    return *ship;
}

// types.pas TShip, read from the game: maneuvers are signed, and every ship
// writes back as it was read.
void the_games_ships_read_and_write_back(const opengold::DaxDecodeResult &archive)
{
    for (const unsigned id : {40U, 41U, 42U, 43U, 44U, 50U})
    {
        Bytes raw;
        const auto ship = ship_of(archive, id, &raw);
        check(opengold::buck_rogers::encode_ship(ship) == raw,
              "Ship " + std::to_string(id) + " writes back byte for byte");
    }
    check(ship_of(archive, 40).maneuver == -2 && ship_of(archive, 42).maneuver == 2 &&
              ship_of(archive, 43).maneuver == -1,
          "The small Hubble is 2 harder to hit, the Mohave 2 easier");
}

// shipcbt.pas TargetValue with the game's ships: the Maelstrom Rider fires
// its K-cannons and missiles at a Hubble, whose maneuver of -2 makes its
// pilot need 15 and 12.
void the_maelstrom_rider_fires_at_a_hubble(const opengold::DaxDecodeResult &archive,
                                           GoldBoxGame game)
{
    auto hubble = ship_of(archive, 40);
    hubble.crew = 0;
    hubble.weapons = {};
    auto dice = laid({3, 7, 14, 15, 50, 100, 11, 12, 50, 100});
    ShipCombat combat(start_of(ship_of(archive, 50), hubble, {pilot("Buck")}, game),
                      ShipDice(dice));
    close_to_range_two(combat);
    order(combat, ShipAction::fire);
    const auto shots = events_of(combat.take_events(), ShipEventKind::shot);
    check(shots.size() == 4 && !shots[0].success && shots[1].success && !shots[2].success &&
              shots[3].success,
          "Two K-cannon shots need 15 and two missiles 12: 14, 15, 11 and 12");
    const auto &target = combat.ship(ShipSide::enemy);
    check(
        target.system_status[life_support] == 0 && target.system_status[hull] == 160,
        "The K-cannon wrecks the Hubble's life support; the missile, striking it again, falls on the hull");
    const auto &rider = combat.ship(ShipSide::party);
    check(rider.weapons[0].ammunition == 4 && rider.weapons[1].ammunition == 2,
          "Each shot spends a round");
    all_rolled(dice, "The Maelstrom Rider's shots");
}

// A whole fight with each of the game's enemy ships, seeded dice and a plain
// player who closes and fires: it ends, the ships stay within their limits,
// and the repairs run after it.
void whole_fights_with_the_games_ships(const opengold::DaxDecodeResult &archive, GoldBoxGame game)
{
    auto engineer = member("Huer");
    give(engineer, Skill::jury_rig, 60);
    give(engineer, Skill::repair_rocket_hull, 50);
    give(engineer, Skill::repair_weapon, 40);
    give(engineer, Skill::repair_nuclear_engine, 50);
    auto medic = member("Wilma");
    give(medic, Skill::first_aid, 40);
    give(medic, Skill::sensor_operation, 30);
    const std::vector crew{pilot("Buck"), engineer, medic, member("Kane")};
    for (int enemy_ship = 1; enemy_ship <= 5; ++enemy_ship)
    {
        for (std::uint64_t seed = 1; seed <= 6; ++seed)
        {
            auto state = std::make_shared<std::uint64_t>(seed * 7919 + enemy_ship);
            const ShipDice dice = [state](int sides)
            {
                return opengold::roll_die(*state, sides);
            };
            auto start =
                start_of(ship_of(archive, 50),
                         ship_of(archive, static_cast<unsigned>(39 + enemy_ship)), crew, game);
            start.enemy_ship_number = enemy_ship;
            ShipCombat combat(start, dice);
            int steps = 0;
            for (; !combat.over() && steps < 5000; ++steps)
            {
                if (combat.awaits_dodge())
                {
                    combat.answer_dodge(true);
                    continue;
                }
                if (combat.awaits_round())
                {
                    combat.play_round();
                    continue;
                }
                const auto orders = combat.legal_orders();
                ShipOrder chosen{.action = ShipAction::quit};
                for (const auto wanted : {ShipAction::close, ShipAction::fire, ShipAction::load,
                                          ShipAction::jury_rig, ShipAction::aid})
                {
                    const auto found = std::find_if(orders.begin(), orders.end(),
                                                    [wanted](const ShipOrder &order)
                                                    {
                                                        return order.action == wanted;
                                                    });
                    if (found != orders.end())
                    {
                        chosen = *found;
                        break;
                    }
                }
                combat.submit(chosen);
            }
            const std::string fight_name =
                "Ship " + std::to_string(enemy_ship) + ", seed " + std::to_string(seed);
            check(combat.over(), fight_name + ": the fight ends");
            auto end = combat.end();
            check(end.outcome == 0 || end.outcome == 1, fight_name + ": the outcome is 0 or 1");
            for (const auto *ship : {&end.party_ship, &end.enemy_ship})
            {
                for (std::size_t system = 0; system < Ship::systems; ++system)
                {
                    check(ship->system_status[system] >= 0 &&
                              ship->system_status[system] <= ship->system_maximum[system],
                          fight_name + ": every system stays within 0 and its whole strength");
                }
            }
            if (end.party_ship.system_status[hull] == 0)
            {
                check(std::all_of(end.crew.begin(), end.crew.end(),
                                  [](const TeamMember &crew_member)
                                  {
                                      return status_of(crew_member) == Status::dead;
                                  }),
                      fight_name + ": the crew died with the ship");
                continue;
            }
            check(!opengold::original::ship_combat_memory(end).empty(),
                  fight_name + ": the end writes the script's memory");
            if (end.repair_pending)
            {
                const auto report =
                    opengold::original::repair_ship(game, end.party_ship, end.crew, end.experience,
                                                    &end.enemy_ship, false, {}, dice);
                check(end.party_ship.system_status[fuel] <= end.party_ship.system_maximum[fuel] &&
                          end.enemy_ship.system_status[fuel] == 0 && report.fuel_taken >= 0,
                      fight_name + ": the repairs take fuel within the tanks");
            }
        }
    }
}

void the_games_ships_fight(GoldBoxGame game)
{
    const auto folder = files_of(game);
    if (!folder)
    {
        return;
    }
    const auto archive = opengold::decode_dax_archive(file_bytes(*folder / "SHIPS.DAX"));
    the_games_ships_read_and_write_back(archive);
    the_maelstrom_rider_fires_at_a_hubble(archive, game);
    whole_fights_with_the_games_ships(archive, game);
}

// Matrix Cubed's SHIPS.DAX is Countdown's (docs/BUCK-ROGERS.md).
void the_two_games_share_their_ships()
{
    const auto first = files_of(countdown);
    const auto second = files_of(matrix);
    if (!first || !second)
    {
        return;
    }
    check(file_bytes(*first / "SHIPS.DAX") == file_bytes(*second / "SHIPS.DAX"),
          "Both games hold the same ships");
}

} // namespace

int main()
{
    try
    {
        the_tables_follow_gdconsts();
        experience_follows_the_enemy_ship();
        damage_slows_a_ship();
        the_fight_opens_at_range_eight();
        a_shot_needs_its_target_number();
        targeting_aims_at_a_system();
        hits_fall_by_the_table();
        ramming_trades_tonnage();
        a_hopeless_dodge_stays_hopeless();
        the_enemy_rams_and_the_pilot_may_dodge();
        boosting_the_engines();
        jury_rigging_lasts_the_fight();
        withdrawing_past_the_scanners();
        the_crew_dies_with_its_ship();
        boarding_hands_over_to_the_script();
        enemy_fire_hurts_the_crew();
        command_changes_the_pilot();
        aid_bandages_the_dying();
        the_dying_fall_into_a_coma();
        sensors_and_loading();
        a_helpless_crew_waits_for_the_round();
        a_fight_no_one_can_end_ends();
        ship_aid_is_ground_aid();
        a_well_armed_enemy_fires_rather_than_rams();
        repairs_follow_the_fight();
        a_kill_saves_the_wreck_for_salvage();
        encode_ship_writes_every_field();
        the_games_ships_fight(countdown);
        the_games_ships_fight(matrix);
        the_two_games_share_their_ships();
        std::cout << "Buck Rogers ship combat passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
