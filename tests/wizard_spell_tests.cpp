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
Character wizard(unsigned level, std::vector<std::string> spells, std::vector<std::string> second = {})
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
    d.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost", "chill_touch"};
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
    check(submit(*c, "area_cast") && logged(*c, "First takes") && !logged(*c, "Second takes"),
          "Only the creature in the sphere takes Thunder damage");
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
        std::cout << "Wizard spell tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
