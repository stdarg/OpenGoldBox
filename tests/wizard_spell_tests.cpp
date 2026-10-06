#include "opengold/campaign_party.h"
#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
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

// A sturdy AC 1 target that hits back and a one-Hit-Point weakling.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature weakling 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature armored 40 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}


// A Wizard of the given level whose book holds the named level-one spells (and
// level-two ones from level three), all prepared.
Character wizard(unsigned level, std::vector<std::string> spells, std::vector<std::string> second = {},
                 std::vector<std::string> cantrips = {"fire_bolt", "ray_of_frost", "chill_touch"})
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Wizard";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::move(cantrips);
    d.spells = SpellChoices{{{"spellbook:1", spells}}, spells, {}, {}};
    d.training = {{"class:wizard", {"medicine", "nature"}}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "wizard-xp");
    for (unsigned n = 1; n < level; ++n)
    {
        auto choice = party.default_advancement(id);
        if (n + 1 == 3 && !second.empty())
        {
            (*choice.spell_learning)["spellbook:3"] = second;
            choice.spells = party.member(id).character.sheet().prepared_spells;
            choice.spells.insert(choice.spells.end(), second.begin(), second.end());
        }
        party.advance(id, choice);
    }
    return party.member(id).character;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool aim(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "area_move" && command.destination == cell)
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Level-one and level-two slots, read from "SRD11 winds slots slots2 ...".
std::pair<int, int> slots(const CombatSession &c)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    int winds{}, first{}, second{};
    in >> magic >> winds >> first >> second;
    return {first, second};
}

bool has(const std::vector<Cell> &cells, Cell cell)
{
    return std::find(cells.begin(), cells.end(), cell) != cells.end();
}

// The Wizard (1) at (1,1), an ally (2) below it and enemies (98, 99) east of it.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      Cell first, Cell second, std::uint64_t seed = 5,
                                      Cell ally = {1, 2}, std::string enemy = "target")
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, ally},
            {98, enemy, "First", 1, first},
            {99, "target", "Second", 1, second}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Wizard's turn");
    check(c->snapshot().actor == 1, "The Wizard acts");
    return c;
}

void burning_hands_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "burning_hands"}), {2, 1}, {3, 1});
    check(submit(*c, "burning_hands") && aim(*c, Cell{3, 1}), "Aim Burning Hands east");
    const auto cone = c->snapshot().area_targeting->cells;
    check(cone.size() == 7 && has(cone, {2, 1}) && has(cone, {3, 1}) && has(cone, {4, 1}) &&
          has(cone, {3, 0}) && has(cone, {3, 2}) && !has(cone, {1, 2}) && !has(cone, {2, 3}),
          "A 15-foot cone of seven squares spreads from the Wizard");
    check(submit(*c, "area_cast") && logged(*c, "First takes") && logged(*c, "Second takes") &&
          !logged(*c, "Ally takes") && logged(*c, "First Dexterity save"),
          "Each creature in the cone saves against Fire damage");
}

void thunderwave_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, wizard(1, {"magic_missile", "thunderwave"}), {2, 1}, {9, 4}, seed);
        check(submit(*c, "thunderwave") && aim(*c, Cell{2, 1}), "Aim Thunderwave east");
        const auto cube = c->snapshot().area_targeting->cells;
        check(cube.size() == 9 && has(cube, {2, 0}) && has(cube, {4, 2}) && !has(cube, {1, 2}),
              "A 15-foot cube stands beside the Wizard");
        check(submit(*c, "area_cast") && logged(*c, "First takes") && !logged(*c, "Ally takes"),
              "The creature in the cube takes Thunder damage");
        if (!logged(*c, "First is pushed 10 feet."))
            continue;
        check(unit(*c, 98).cell == Cell{4, 1}, "A failed save pushes it 10 feet away");
        return;
    }
    throw std::runtime_error("No seed fails the Constitution save");
}

void shatter_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(3, {"magic_missile"}, {"shatter", "blindness"}), {6, 2}, {11, 5});
    check(submit(*c, "shatter") && aim(*c, Cell{6, 2}), "Aim Shatter");
    check(c->snapshot().area_targeting->cells.size() == 25, "A 10-foot-radius sphere");
    const auto before = slots(*c);
    check(submit(*c, "area_cast") && logged(*c, "First takes") && !logged(*c, "Second takes"),
          "Only the creature in the sphere takes Thunder damage");
    check(slots(*c).second == before.second - 1 && slots(*c).first == before.first,
          "Shatter spends a level-two slot");
}
void potent_cantrip_checks()
{
    auto module = rules();
    for (const unsigned level : {1u, 3u})
    {
        auto c = battle(*module, wizard(level, {"magic_missile"}), {4, 1}, {11, 5}, 5, {1, 2},
                        "armored");
        check(submit(*c, "fire_bolt", 98) && logged(*c, "misses"), "Fire Bolt misses AC 40");
        check(logged(*c, "Potent Cantrip: First takes") == (level == 3),
              "From level three a missed cantrip still deals half damage");
    }
}

void sculpt_spells_checks()
{
    auto module = rules();
    for (const unsigned level : {1u, 3u})
    {
        auto c = battle(*module, wizard(level, {"magic_missile", "burning_hands"}), {3, 1}, {11, 5},
                        5, {2, 1});
        check(submit(*c, "burning_hands") && aim(*c, Cell{3, 1}) && submit(*c, "area_cast"),
              "Burning Hands with an ally in the cone");
        check(logged(*c, "Ally is spared by Sculpt Spells.") == (level == 3) &&
              logged(*c, "Ally takes") == (level == 1) && logged(*c, "First takes"),
              "From level three the Evoker spares its ally");
    }
}
// Ends the Wizard's turn and returns to it.
void next_turn(CombatSession &c)
{
    check(submit(c, "end"), "End the Wizard's turn");
    for (unsigned turns = 0; c.snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(c, "end"), "Back to the Wizard");
}

void mage_armor_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "mage_armor"}), {6, 1}, {11, 5});
    const int ac = unit(*c, 1).armor_class;
    check(submit(*c, "mage_armor", 1) && unit(*c, 1).armor_class == ac + 3,
          "Mage Armor makes an unarmored base AC 13 + Dexterity");
    next_turn(*c);
    check(!submit(*c, "mage_armor", 1), "Mage Armor is not cast again on the warded Wizard");
}

void false_life_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "false_life"}), {6, 1}, {11, 5});
    check(submit(*c, "false_life", 1), "False Life is cast");
    const int temporary = unit(*c, 1).temporary_hp.amount;
    check(temporary >= 6 && temporary <= 12 && logged(*c, "Wizard gains False Life."),
          "False Life grants 2d4 + 4 Temporary Hit Points");
}

void expeditious_retreat_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "expeditious_retreat"}), {6, 1}, {11, 5});
    const int feet = unit(*c, 1).movement_feet;
    check(submit(*c, "expeditious_retreat", 1) && unit(*c, 1).movement_feet == feet + 30 &&
          unit(*c, 1).action,
          "Expeditious Retreat Dashes at once with the Bonus Action");
    next_turn(*c);
    check(submit(*c, "retreat_dash") && unit(*c, 1).movement_feet == feet + 30 &&
          unit(*c, 1).action,
          "Later turns Dash as a Bonus Action");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The extra Dash survives a checkpoint");
}
void ray_of_sickness_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "ray_of_sickness"}), {2, 1}, {11, 5});
    check(submit(*c, "ray_of_sickness", 98) && logged(*c, "First is Poisoned.") &&
          unit(*c, 98).conditions.size() == 1,
          "A hit Poisons the target");
    check(submit(*c, "end"), "End the Wizard's turn");
    while (c->snapshot().actor != 98)
        check(submit(*c, "end"), "Reach the Poisoned creature");
    check(submit(*c, "melee", 1) && logged(*c, "(disadvantage)"),
          "A Poisoned creature attacks with Disadvantage");
}

void ice_knife_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "ice_knife"}), {3, 1}, {4, 1});
    check(submit(*c, "ice_knife", 98) && logged(*c, "First Dexterity save") &&
          logged(*c, "Second Dexterity save") && !logged(*c, "Ally Dexterity save"),
          "The shard bursts on the target and the creatures beside it");
}

void chromatic_orb_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile", "chromatic_orb"}), {3, 1}, {11, 5});
    std::vector<std::string> labels;
    for (const auto &command : c->legal_commands())
        if (command.verb.starts_with("chromatic_orb_") && command.target == 98)
            labels.push_back(command.label);
    check(labels.size() == 6 && labels.front() == "Chromatic Orb: Acid",
          "Chromatic Orb is offered once per damage type");
    const auto before = slots(*c);
    check(submit(*c, "chromatic_orb_fire", 98) && logged(*c, "Wizard -> First") &&
          slots(*c).first == before.first - 1,
          "The orb is a spell attack from a level-one slot");
}

void acid_splash_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(1, {"magic_missile"}, {}, {"acid_splash", "fire_bolt", "ray_of_frost"}),
                    {3, 1}, {4, 1});
    const auto before = slots(*c);
    check(submit(*c, "acid_splash") && aim(*c, Cell{3, 1}) && submit(*c, "area_cast") &&
          logged(*c, "First Dexterity save") && logged(*c, "Second Dexterity save") &&
          slots(*c) == before,
          "Acid Splash's sphere catches both creatures and spends no slot");
}

bool has_condition(const CombatSession &c, EntityId id, std::string_view label)
{
    const auto conditions = unit(c, id).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [&](const auto & condition)
    {
        return condition.source == label;
    });
}

// Ends turns until the given creature acts.
void reach(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 6; ++turns)
        check(submit(c, "end"), "End a turn");
    check(c.snapshot().actor == id, "Reach the creature's turn");
}

void sleep_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, wizard(1, {"magic_missile", "sleep"}), {2, 1}, {3, 1}, seed);
        check(submit(*c, "sleep") && aim(*c, Cell{1, 0}) && submit(*c, "area_cast"),
              "Cast Sleep on the adjacent enemy");
        check(!logged(*c, "Second Wisdom save"), "The sphere stops short of the second enemy");
        check(!logged(*c, "Ally grows drowsy") && !logged(*c, "Ally Wisdom save"),
              "Sleep passes over the Wizard's allies");
        if (!logged(*c, "First grows drowsy."))
            continue;
        check(has_condition(*c, 98, "Incapacitated (Sleep)"), "A failed save leaves it Incapacitated");
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "Drowsiness survives a checkpoint");
        reach(*c, 99);
        check(submit(*c, "shake_awake", 98) && !has_condition(*c, 98, "Incapacitated (Sleep)") &&
              !has_condition(*c, 98, "Unconscious (Sleep)"),
              "A companion beside the sleeper shakes it awake");
        return;
    }
    throw std::runtime_error("No seed fails the Wisdom save");
}

void hideous_laughter_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, wizard(1, {"magic_missile", "hideous_laughter"}), {4, 1}, {11, 5},
                        seed);
        check(submit(*c, "hideous_laughter", 98), "Cast Hideous Laughter");
        if (!logged(*c, "First falls Prone, laughing."))
            continue;
        check(has_condition(*c, 98, "Prone and Incapacitated (laughing)"),
              "The target is Prone and Incapacitated");
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "The laughter survives a checkpoint");
        reach(*c, 98);
        for (const auto &command : c->legal_commands())
            check(command.verb == "end", "A laughing creature can only end its turn");
        return;
    }
    throw std::runtime_error("No seed fails the Wisdom save");
}

void color_spray_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, wizard(1, {"magic_missile", "color_spray"}), {3, 1}, {11, 5}, seed);
        check(submit(*c, "color_spray") && aim(*c, Cell{3, 1}) &&
              c->snapshot().area_targeting->cells.size() == 7 && submit(*c, "area_cast"),
              "Color Spray fills a 15-foot cone");
        check(!logged(*c, "Ally Constitution save") && !logged(*c, "Second Constitution save"),
              "Only creatures in the cone are touched");
        if (!logged(*c, "First is Blinded."))
            continue;
        check(!unit(*c, 98).conditions.empty(), "A failed save Blinds the target");
        return;
    }
    throw std::runtime_error("No seed fails the Constitution save");
}

bool difficult(const CombatSession &c, Cell cell)
{
    return c.snapshot().battlefield.at(cell) == 2;
}

void grease_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, wizard(1, {"magic_missile", "grease"}), {4, 1}, {11, 5}, seed);
        check(submit(*c, "grease") && aim(*c, Cell{4, 1}) && submit(*c, "area_cast"),
              "Cast Grease on the enemy");
        check(difficult(*c, {4, 1}) && difficult(*c, {5, 2}) && !difficult(*c, {6, 1}),
              "Grease makes a 10-foot square Difficult Terrain");
        if (!logged(*c, "First slips and falls Prone."))
            continue;
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "The grease survives a checkpoint");
        for (unsigned turns = 0; turns < 40; ++turns)
            check(submit(*c, "end"), "Let the minute pass");
        check(!difficult(*c, {4, 1}), "Grease vanishes after a minute");
        return;
    }
    throw std::runtime_error("No seed fails the Dexterity save");
}

bool move_to(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "move" && command.destination == cell)
            return c.submit(command);
    return false;
}

void web_checks()
{
    auto module = rules();
    bool caught_entering = false;
    for (std::uint64_t seed = 1; seed < 64 && !caught_entering; ++seed)
    {
        auto c = battle(*module, wizard(3, {"magic_missile"}, {"web", "shatter"}), {6, 2}, {10, 2},
                        seed);
        const auto before = slots(*c);
        check(submit(*c, "web") && aim(*c, Cell{6, 2}) && submit(*c, "area_cast") &&
              slots(*c).second == before.second - 1,
              "Web spends a level-two slot");
        check(difficult(*c, {5, 1}) && difficult(*c, {8, 4}) && !difficult(*c, {9, 2}),
              "Web fills a 20-foot cube");
        reach(*c, 99);
        check(move_to(*c, Cell{8, 2}) && logged(*c, "Second Dexterity save"),
              "Entering the webs calls for a save");
        caught_entering = logged(*c, "Second is Restrained by the webs.");
        if (!caught_entering)
            continue;
        check(unit(*c, 99).cell == Cell{8, 2}, "A caught creature stops where it entered");
        const auto offered = c->legal_commands();
        check(std::any_of(offered.begin(), offered.end(), [](const auto & command)
        {
            return command.verb == "escape" && command.label == "Escape the webs";
        }),
        "The webbed creature may try to break free");
    }
    check(caught_entering, "Some seed catches the entering creature");
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// The enemy beside the Wizard attacks until a hit asks the Wizard about Shield.
std::unique_ptr<CombatSession> shield_question(const RulesModule &module)
{
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(module, wizard(1, {"magic_missile", "shield"}), {2, 1}, {11, 5}, seed);
        reach(*c, 98);
        check(submit(*c, "melee", 1), "The enemy attacks the Wizard");
        if (!c->snapshot().reaction_pending)
            continue;
        check(c->snapshot().actor == 1 && offered(*c, "shield") && offered(*c, "decline"),
              "The hit Wizard is asked to cast Shield or decline");
        check(!logged(*c, "First -> Wizard"), "The attack waits for the answer");
        return c;
    }
    throw std::runtime_error("No seed hits the Wizard");
}

void shield_checks()
{
    auto module = rules();
    {
        auto c = shield_question(*module);
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "The Shield question survives a checkpoint");
        const int ac = unit(*c, 1).armor_class;
        const auto before = slots(*c);
        check(submit(*c, "shield") && logged(*c, "Wizard casts Shield.") &&
              unit(*c, 1).armor_class == ac + 5 && slots(*c).first == before.first - 1,
              "Shield spends the Reaction and a slot for +5 AC");
        check(count_logged(*c, "First -> Wizard") == 1 && c->snapshot().actor == 98,
              "The attack resolves once, against the shielded AC");
    }
    {
        auto c = shield_question(*module);
        const int hp = unit(*c, 1).hit_points;
        check(submit(*c, "decline") && !logged(*c, "casts Shield") &&
              count_logged(*c, "First -> Wizard") == 1 && unit(*c, 1).hit_points < hp,
              "Declining lets the hit land");
    }
}

void shield_missile_checks()
{
    auto module = rules();
    const auto hero = wizard(1, {"magic_missile", "shield"});
    const auto foe = wizard(1, {"magic_missile"});
    const auto profile = [&](const Character &who)
    {
        return module->character_profile(who.sheet(), std::vector<std::string> {}).data;
    };
    auto c = module->create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile(hero)},
            {98, "campaign-character", "Foe", 1, {6, 1}, profile(foe)}
        }},
    5);
    reach(*c, 98);
    check(submit(*c, "magic_missile", 1) && c->snapshot().actor == 1, "Magic Missile asks");
    const int hp = unit(*c, 1).hit_points;
    check(submit(*c, "shield") && logged(*c, "Wizard's Shield blocks Magic Missile.") &&
          unit(*c, 1).hit_points == hp,
          "Shield stops Magic Missile");
}

void misty_step_checks()
{
    auto module = rules();
    auto c = battle(*module, wizard(3, {"magic_missile"}, {"misty_step", "shatter"}), {2, 1}, {11, 5});
    const auto before = slots(*c);
    check(submit(*c, "misty_step") && !offered(*c, "area_cast") && !aim(*c, Cell{2, 1}) &&
          !aim(*c, Cell{8, 1}),
          "Misty Step needs an unoccupied square within 30 feet");
    check(aim(*c, Cell{6, 1}) && submit(*c, "area_cast") && unit(*c, 1).cell == Cell{6, 1},
          "The Wizard teleports to the chosen square");
    check(!c->snapshot().reaction_pending && unit(*c, 1).action && !unit(*c, 1).bonus_action &&
          slots(*c).second == before.second - 1,
          "A Bonus Action from a level-two slot, with no Opportunity Attack");
}

} // namespace

int main()
{
    try
    {
        burning_hands_checks();
        thunderwave_checks();
        shatter_checks();
        potent_cantrip_checks();
        sculpt_spells_checks();
        mage_armor_checks();
        false_life_checks();
        expeditious_retreat_checks();
        ray_of_sickness_checks();
        ice_knife_checks();
        chromatic_orb_checks();
        acid_splash_checks();
        sleep_checks();
        hideous_laughter_checks();
        color_spray_checks();
        grease_checks();
        web_checks();
        shield_checks();
        shield_missile_checks();
        misty_step_checks();
        std::cout << "Wizard spell tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
