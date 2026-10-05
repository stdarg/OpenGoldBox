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

// A sturdy AC 1 target that hits back, the same as a Fiend, and a one-Hit-Point weakling.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature fiend 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "type fiend fiend\n" + "affinity fiend hide resistance slashing\n" +
                               "creature weakling 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A level-one Paladin who prepared the two named spells.
Character paladin(std::vector<std::string> prepared)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "paladin";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Paladin";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    return Character(*srd5::character_rules(), d, {});
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
    const auto offered = c.legal_commands();
    for (const auto &command : offered)
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view prefix)
{
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(prefix) != std::string::npos;
    });
}

// The Paladin (1) with an ally (2) beside it and an enemy (99) adjacent.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::string enemy = "target")
{
    const auto profile =
        module.character_profile(hero.sheet(), std::vector<std::string> {"longsword"}).data;
    const auto ally = module.character_profile(paladin({}).sheet(), {}).data;
    auto c = module.create({{8, 4, std::vector<std::uint8_t>(32)},
        {   {1, "campaign-character", "Paladin", 0, {1, 1}, profile},
            {2, "campaign-character", "Ally", 0, {1, 2}, ally},
            {99, std::move(enemy), "Enemy", 1, {2, 1}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    check(c->snapshot().actor == 1, "The Paladin acts");
    return c;
}

void shield_of_faith_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"shield_of_faith", "heroism"}));
    const int ally_ac = unit(*c, 2).armor_class;
    check(submit(*c, "shield_of_faith", 2), "Shield of Faith is a Bonus Action on an ally");
    check(unit(*c, 2).armor_class == ally_ac + 2 && unit(*c, 1).action,
          "Shield of Faith adds 2 to AC and keeps the Action");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Concentration survives a checkpoint");
    // Another Concentration spell ends the first.
    check(!submit(*c, "heroism", 2), "One spell slot per turn");
    check(submit(*c, "end"), "End the Paladin's turn");
    while (c->snapshot().actor != 1)
        check(submit(*c, "end"), "Back to the Paladin");
    check(submit(*c, "heroism", 2), "Heroism replaces the held Concentration");
    check(unit(*c, 2).armor_class == ally_ac && logged(*c, "Paladin loses Concentration."),
          "Starting Heroism ends Shield of Faith");
}

void heroism_checks()
{
    auto module = rules();
    const auto hero = paladin({"heroism", "cure_wounds"});
    auto c = battle(*module, hero);
    check(submit(*c, "heroism", 2), "Heroism touches the ally");
    while (c->snapshot().actor != 2)
        check(submit(*c, "end"), "Reach the ally's turn");
    const int charisma = hero.sheet().modifiers[5];
    check(unit(*c, 2).temporary_hp.amount == std::max(0, charisma),
          "Heroism grants Temporary HP equal to the caster's spellcasting modifier");
}

void divine_favor_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"divine_favor", "cure_wounds"}));
    check(submit(*c, "divine_favor", 1), "Divine Favor is a Bonus Action on the caster");
    check(submit(*c, "melee", 99), "The Paladin attacks");
    check(logged(*c, "Radiant damage from Divine Favor"), "A weapon hit adds Divine Favor's 1d4");
}

void concentration_damage_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"shield_of_faith", "heroism"}));
    check(submit(*c, "shield_of_faith", 1), "Shield of Faith on the Paladin");
    const int shielded = unit(*c, 1).armor_class;
    // The enemy, +20 to hit, strikes the Paladin on its turn.
    while (c->snapshot().actor != 99)
        check(submit(*c, "end"), "Reach the enemy");
    const int hp = unit(*c, 1).hit_points;
    check(submit(*c, "melee", 1), "The enemy attacks the Paladin");
    check(unit(*c, 1).hit_points < hp, "The Paladin is hit");
    check(logged(*c, "Paladin Constitution save"), "Damage calls for a Concentration save");
    const bool kept = unit(*c, 1).armor_class == shielded;
    check(kept != logged(*c, "Paladin loses Concentration."),
          "A failed save ends Shield of Faith; a success keeps it");
}

// Whether the enemy's attack on a Paladin warded by Protection from Evil and Good
// is rolled with Disadvantage.
bool warded_attack_disadvantaged(std::string enemy)
{
    auto module = rules();
    auto c = battle(*module, paladin({"protection_from_evil_and_good"}), std::move(enemy));
    check(submit(*c, "protection_from_evil_and_good", 1), "Protection from Evil and Good on the Paladin");
    check(!unit(*c, 1).action, "Protection from Evil and Good takes the Action");
    while (c->snapshot().actor != 99)
        check(submit(*c, "end"), "Reach the enemy");
    check(submit(*c, "melee", 1), "The enemy attacks the Paladin");
    return logged(*c, "(disadvantage)");
}

void protection_from_evil_and_good_checks()
{
    check(warded_attack_disadvantaged("fiend"), "A Fiend attacks the warded Paladin with Disadvantage");
    check(!warded_attack_disadvantaged("target"), "A Humanoid attacks the warded Paladin normally");
}

// The attack bonus a log line shows: "... d20 N + B vs AC ...".
int logged_bonus(const CombatSession &c)
{
    const auto log = c.snapshot().log;
    for (auto line = log.rbegin(); line != log.rend(); ++line)
        if (const auto at = line->find(" + "); at != std::string::npos && line->find(" vs AC ") != std::string::npos)
            return std::stoi(line->substr(at + 3));
    throw std::runtime_error("No attack in the log");
}

void bless_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"bless", "cure_wounds"}));
    const auto before = unit(*c, 1);
    check(submit(*c, "bless", 2), "Bless begins with a first creature");
    auto targeting = c->snapshot().spell_targeting;
    check(targeting && targeting->chosen == std::vector<EntityId> {2} && targeting->maximum == 3 &&
          unit(*c, 1).action,
          "Choosing a creature starts the choice and spends nothing");
    for (const auto &command : c->legal_commands())
        check(command.verb == "bless" || command.verb == "spell_cast" ||
              command.verb == "spell_cancel",
              "Only the choice is open while choosing");
    const auto choosing = c->save();
    check(module->restore(choosing)->save() == choosing, "The open choice survives a checkpoint");
    check(submit(*c, "bless", 2) && c->snapshot().spell_targeting->chosen.empty(),
          "Choosing a chosen creature again removes it");
    check(submit(*c, "spell_cancel") && !c->snapshot().spell_targeting &&
          unit(*c, 1).action && unit(*c, 1).persistent.resources == before.persistent.resources,
          "Cancel spends nothing");

    check(submit(*c, "bless", 1) && submit(*c, "bless", 2) && submit(*c, "bless", 99),
          "Three creatures are chosen");
    check(!c->snapshot().spell_targeting && !unit(*c, 1).action &&
          logged(*c, "Paladin casts Bless."),
          "The third creature casts the spell");

    auto early = battle(*module, paladin({"bless", "cure_wounds"}));
    check(submit(*early, "bless", 1) && submit(*early, "spell_cast"),
          "Bless can be cast on fewer creatures");
    // A blessed attack adds 1d4 to the roll.
    auto plain = battle(*module, paladin({"cure_wounds", "heroism"}));
    check(submit(*plain, "melee", 99), "An unblessed attack");
    const int base = logged_bonus(*plain);
    check(submit(*early, "end"), "End the casting turn");
    while (early->snapshot().actor != 1)
        check(submit(*early, "end"), "Back to the Paladin");
    check(submit(*early, "melee", 99), "A blessed attack");
    const int blessed = logged_bonus(*early);
    check(blessed >= base + 1 && blessed <= base + 4, "Bless adds 1d4 to attack rolls");
}

// A Paladin with Bless prepared beside an ally, for tests/bless_view_tests.gd.
// The game loads it with the standard rules content.
void write_ui_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile =
        module->character_profile(paladin({"bless", "cure_wounds"}).sheet(), std::vector<std::string> {"longsword"}).data;
    const auto ally = module->character_profile(paladin({}).sheet(), {}).data;
    auto c = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Paladin", 0, {1, 1}, profile},
            {2, "campaign-character", "Ally", 0, {2, 1}, ally},
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
    2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "bless-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream out(path / "paladin.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");
}

#include "command_checks.h"
#include "devotion_checks.h"

void combat_end_checks()
{
    auto module = rules();
    auto c = battle(*module, paladin({"shield_of_faith", "heroism"}), "weakling");
    check(submit(*c, "shield_of_faith", 1), "Shield of Faith on the Paladin");
    check(submit(*c, "melee", 99), "The Paladin strikes the weakling down");
    check(c->snapshot().outcome == Outcome::victory, "The fight is won");
    check(logged(*c, "Paladin loses Concentration."), "Concentration ends with the combat");
}
} // namespace

int main()
{
    try
    {
        shield_of_faith_checks();
        heroism_checks();
        divine_favor_checks();
        bless_checks();
        protection_from_evil_and_good_checks();
        command_checks();
        devotion_checks();
        concentration_damage_checks();
        combat_end_checks();
        write_ui_fixture();
        std::cout << "Paladin spell tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
