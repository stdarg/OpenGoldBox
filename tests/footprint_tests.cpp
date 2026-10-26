// Creatures whose combat art fills more than one square fill those squares in
// a fight, as in the original (FOOT-1). The fixture is the campaign's troll,
// whose tall art (MON2CHA record 31) fills two squares, one above the other.
#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include "combat_fixture.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
using test::check;
using test::logged;
using test::rejects;

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
constexpr EntityId troll_id = 98;
constexpr Footprint tall{1, 2};

std::unique_ptr<RulesModule> load_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    check(bool(in), "Read rules");
    // A sturdy fighter that hits back.
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature hero 18 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A level-1 Druid with Thunderwave and Wild Shape's forms to come.
Character druid(unsigned level)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "druid";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Druid";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:druid", {"nature", "medicine"}},
                  {"class:druid:primal_order", {"warden"}}};
    d.cantrips = {"produce_flame", "shillelagh"};
    d.spells = SpellChoices{
        {},
        std::vector<std::string>{"cure_wounds", "entangle", "faerie_fire", "thunderwave"},
        {},
        {}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "druid-xp");
    for (unsigned n = 1; n < level; ++n)
    {
        party.advance(id, party.default_advancement(id));
    }
    return party.member(id).character;
}

Participant troll(Cell cell)
{
    Participant p{troll_id, "troll", "Troll", Side::opposition, cell};
    p.footprint = tall;
    return p;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
    {
        if (a.id == id)
        {
            return a;
        }
    }
    throw std::runtime_error("Missing combatant");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
    return false;
}

bool offered(const CombatSession &c, std::string_view verb, Cell destination)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && command.destination == destination;
                       });
}

bool submit_cell(CombatSession &c, std::string_view verb, Cell destination)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && command.destination == destination)
        {
            return c.submit(command);
        }
    }
    return false;
}

void turn_of(CombatSession &c, EntityId id)
{
    test::keep_initiative(c);
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 8; ++turns)
    {
        check(submit(c, "end"), "Pass the turn");
    }
    check(c.snapshot().actor == id, "Reach the combatant's turn");
}

Battlefield open_field()
{
    return {12, 8, std::vector<Terrain>(96, Terrain::open)};
}

// The troll fills both of its squares: no one starts on its lower square, and
// it cannot start with a square off the field or on an obstacle.
void placement_checks(const RulesModule &module)
{
    auto c =
        module.create({open_field(), {{1, "hero", "Hero", Side::party, {1, 1}}, troll({5, 2})}}, 1);
    check(unit(*c, troll_id).footprint == tall && unit(*c, troll_id).cell == Cell{5, 2},
          "The troll reports its footprint and top-left square");
    check(unit(*c, 1).footprint == Footprint{}, "An ordinary creature fills one square");
    rejects(
        [&]
        {
            (void)module.create(
                {open_field(), {{1, "hero", "Hero", Side::party, {5, 3}}, troll({5, 2})}}, 1);
        },
        "No one starts on the troll's lower square");
    rejects(
        [&]
        {
            (void)module.create(
                {open_field(), {{1, "hero", "Hero", Side::party, {1, 1}}, troll({5, 7})}}, 1);
        },
        "The troll's lower square cannot be off the field");
    auto walled = open_field();
    walled.terrain[3 * 12 + 5] = Terrain::obstacle;
    rejects(
        [&]
        {
            (void)module.create({walled, {{1, "hero", "Hero", Side::party, {1, 1}}, troll({5, 2})}},
                                1);
        },
        "The troll's lower square cannot be an obstacle");
}

// A hero below the troll's lower square is in reach though the troll's own
// square is 10 feet away, and cannot step onto either of its squares. In a
// corridor one square wide, no route passes it.
void reach_and_occupancy_checks(const RulesModule &module)
{
    auto c =
        module.create({open_field(), {{1, "hero", "Hero", Side::party, {5, 4}}, troll({5, 2})}}, 3);
    turn_of(*c, 1);
    check(!offered(*c, "move", {5, 3}) && !offered(*c, "move", {5, 2}),
          "Neither of the troll's squares is a destination");
    check(offered(*c, "move", {5, 1}), "A route goes around it on the open field");
    check(submit(*c, "melee", troll_id), "The hero below the lower square attacks the troll");

    auto corridor = open_field();
    for (int y = 0; y < corridor.height; ++y)
    {
        for (int x = 0; x < corridor.width; ++x)
        {
            corridor.terrain[static_cast<std::size_t>(y * corridor.width + x)] =
                x == 5 ? Terrain::open : Terrain::obstacle;
        }
    }
    auto blocked =
        module.create({corridor, {{1, "hero", "Hero", Side::party, {5, 5}}, troll({5, 2})}}, 3);
    turn_of(*blocked, 1);
    check(offered(*blocked, "move", {5, 4}) && !offered(*blocked, "move", {5, 1}),
          "No route runs through the troll's squares");
}

// The troll moves its two squares together: an obstacle beside its lower
// square shuts the step its upper square alone could take.
void movement_checks(const RulesModule &module)
{
    auto board = open_field();
    board.terrain[3 * 12 + 6] = Terrain::obstacle;
    auto c = module.create({board, {{1, "hero", "Hero", Side::party, {1, 7}}, troll({5, 2})}}, 5);
    turn_of(*c, troll_id);
    check(!offered(*c, "move", {6, 2}), "The lower square cannot enter the obstacle");
    check(offered(*c, "move", {6, 1}) && offered(*c, "move", {5, 5}),
          "Squares with room for both are destinations");
    check(submit_cell(*c, "move", {5, 5}), "The troll moves");
    check(unit(*c, troll_id).cell == Cell{5, 5} && unit(*c, troll_id).footprint == tall,
          "Both squares moved together");
}

// Leaving the reach of a creature beside only the troll's lower square still
// provokes its Opportunity Attack.
void opportunity_checks(const RulesModule &module)
{
    auto c =
        module.create({open_field(), {{1, "hero", "Hero", Side::party, {6, 3}}, troll({5, 2})}}, 7);
    turn_of(*c, 1);
    check(submit_cell(*c, "move", {9, 3}), "The hero walks away");
    const auto commands = c->legal_commands();
    const bool asked = std::any_of(commands.begin(), commands.end(),
                                   [](const auto &command)
                                   {
                                       return command.verb == "opportunity";
                                   });
    check(asked || logged(*c, "Troll -> Hero"), "Leaving its reach provokes the troll");
}

// Thunderwave's cube over only the troll's lower square catches the troll,
// and a push moves both of its squares.
void area_checks(const RulesModule &module)
{
    const auto profile = module.character_profile(druid(1).sheet(), {}).data;
    bool caught = false;
    for (std::uint64_t seed = 1; seed <= 20 && !caught; ++seed)
    {
        auto c = module.create(
            {open_field(),
             {{1, "campaign-character", "Druid", Side::party, {5, 6}, profile}, troll({5, 2})}},
            seed);
        turn_of(*c, 1);
        check(submit(*c, "thunderwave"), "Thunderwave aims");
        std::vector<Cell> aims;
        for (const auto &command : c->legal_commands())
        {
            if (command.verb == "area_move")
            {
                aims.push_back(command.destination);
            }
        }
        bool aimed = false;
        for (const auto aim : aims)
        {
            check(submit_cell(*c, "area_move", aim), "Move the aim");
            const auto cells = c->snapshot().area_targeting->cells;
            const auto has = [&](Cell cell)
            {
                return std::find(cells.begin(), cells.end(), cell) != cells.end();
            };
            if (has({5, 3}) && !has({5, 2}))
            {
                aimed = true;
                break;
            }
        }
        check(aimed, "An aim covers only the troll's lower square");
        check(submit(*c, "area_cast"), "Cast Thunderwave");
        check(logged(*c, "Troll takes"), "The troll is in the area");
        if (unit(*c, troll_id).cell != Cell{5, 2})
        {
            caught = true;
            check(unit(*c, troll_id).cell == Cell{5, 0}, "Pushed 10 feet, both squares together");
        }
    }
    check(caught, "A failed save pushed the troll");
}

// An area a creature breathes starts from its square nearest the aim: an
// ankheg's wide art fills two squares side by side, and its 30-foot Acid
// Spray reaches a hero 30 feet from its right square.
void breath_checks(const RulesModule &module)
{
    Participant ankheg{troll_id, "ankheg", "Ankheg", Side::opposition, {2, 2}};
    ankheg.footprint = {2, 1};
    auto c = module.create({open_field(), {{1, "hero", "Hero", Side::party, {9, 2}}, ankheg}}, 13);
    turn_of(*c, troll_id);
    check(submit(*c, "acid_spray"), "The ankheg aims its Acid Spray");
    check(submit_cell(*c, "area_move", {9, 2}), "The hero is in range of its right square");
    const auto cells = c->snapshot().area_targeting->cells;
    check(std::find(cells.begin(), cells.end(), Cell{9, 2}) != cells.end() &&
              std::find(cells.begin(), cells.end(), Cell{3, 2}) == cells.end(),
          "The spray starts beside its right square and reaches the hero");
}

// A creature at the field's bottom edge whose lower square would step off it
// runs off the field.
void flight_checks(const RulesModule &module)
{
    Participant hero{1, "hero", "Hero", Side::party, {3, 6}};
    hero.footprint = tall;
    auto c = module.create({open_field(), {hero, troll({9, 1})}}, 15);
    turn_of(*c, 1);
    check(offered(*c, "flee", {3, 7}), "A step down puts its lower square off the field");
    check(submit_cell(*c, "flee", {3, 7}) && unit(*c, 1).fled, "It runs off the field");
}

// A combat checkpoint keeps the footprint.
void checkpoint_checks(const RulesModule &module)
{
    auto c =
        module.create({open_field(), {{1, "hero", "Hero", Side::party, {1, 1}}, troll({5, 2})}}, 9);
    const auto restored = module.restore(c->save());
    check(unit(*restored, troll_id).footprint == tall, "A checkpoint keeps the troll's squares");
    check(restored->save() == c->save(), "The checkpoint round-trips");
}

// Wild Shape's wide forms fill two squares side by side; with no room to the
// right, the Druid takes the nearest space with room.
void wild_shape_checks(const RulesModule &module)
{
    const auto profile = module.character_profile(druid(2).sheet(), {}).data;
    auto c = module.create({open_field(),
                            {{1, "campaign-character", "Druid", Side::party, {5, 5}, profile},
                             {2, "hero", "Ally", Side::party, {6, 5}},
                             troll({1, 1})}},
                           11);
    turn_of(*c, 1);
    check(submit(*c, "wild_shape_wolf"), "The Druid becomes a Wolf");
    const auto wolf = unit(*c, 1);
    check(wolf.footprint == Footprint{2, 1}, "The Wolf's wide art fills two squares");
    check(wolf.cell == Cell{4, 5}, "With the ally to the right, the Wolf takes the space beside");
    const auto restored = module.restore(c->save());
    check(unit(*restored, 1).footprint == Footprint{2, 1} && unit(*restored, 1).cell == Cell{4, 5},
          "The form's squares survive a checkpoint");
}
} // namespace

int main()
{
    try
    {
        const auto module = load_rules();
        placement_checks(*module);
        reach_and_occupancy_checks(*module);
        movement_checks(*module);
        opportunity_checks(*module);
        area_checks(*module);
        breath_checks(*module);
        flight_checks(*module);
        checkpoint_checks(*module);
        wild_shape_checks(*module);
        std::cout << "Footprint checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
