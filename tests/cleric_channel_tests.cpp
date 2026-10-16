#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <chrono>
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

// A sturdy AC 1 target that hits back, an Undead zombie, a poisonous viper and
// a shade that resists Radiant, all of the same build.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature walker 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "type walker undead\n" +
                               "creature viper 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "damage_types viper poison poison\n" +
                               "creature shade 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "affinity shade radiant_ward resistance radiant\n");
}

// A Cleric of the given level, advanced through the campaign, with the named
// level-one spells prepared.
Character cleric(unsigned level, std::vector<std::string> prepared = {"cure_wounds"},
                 std::vector<std::string> cantrips = {"sacred_flame", "spare_the_dying",
                                                      "resistance"})
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Cleric";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::move(cantrips);
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
                  {"class:cleric:divine_order", {"protector"}}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(900, "cleric-xp");
    for (unsigned n = 1; n < level; ++n)
        party.advance(id, party.default_advancement(id));
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

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(),
                       [&](const auto &line)
                       {
                           return line.find(text) != std::string::npos;
                       });
}

unsigned channel(const CombatSession &c)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == "channel_divinity")
            return resource.remaining;
    return 0;
}

// The Cleric (1), a wounded ally (2) and an enemy (99) of the given kind.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::string enemy = "target", std::uint64_t seed = 5,
                                      std::string ally_state = {})
{
    const auto profile =
        module.character_profile(hero.sheet(), std::vector<std::string>{"mace"}).data;
    auto c = module.create({{12, 4, std::vector<Terrain>(48)},
                            {{1, "campaign-character", "Cleric", Side::party, {1, 1}, profile},
                             {2,
                              "target",
                              "Ally",
                              Side::party,
                              {1, 2},
                              {},
                              VitalState{3, false, std::move(ally_state)}},
                             {99, std::move(enemy), "Enemy", Side::opposition, {3, 1}}}},
                           seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    check(c->snapshot().actor == 1, "The Cleric acts");
    return c;
}

void divine_spark_checks()
{
    auto module = rules();
    auto low = battle(*module, cleric(1));
    check(!submit(*low, "divine_spark", 2), "Channel Divinity waits for level two");

    auto heal = battle(*module, cleric(2));
    check(channel(*heal) == 2, "A level-two Cleric has two Channel Divinity uses");
    check(submit(*heal, "divine_spark", 2) && unit(*heal, 2).hit_points > 3 &&
              channel(*heal) == 1 && !unit(*heal, 1).action,
          "Divine Spark heals an ally and spends a use and the Action");

    auto harm = battle(*module, cleric(2));
    check(submit(*harm, "divine_spark", 99) && logged(*harm, "Enemy Constitution save") &&
              logged(*harm, "Radiant damage") && unit(*harm, 99).hit_points < 1000,
          "Divine Spark harms an enemy, a Constitution save for half");
    check(!submit(*harm, "turn_undead", 1), "Turn Undead needs an Undead");

    auto resisted = battle(*module, cleric(2), "shade");
    check(submit(*resisted, "divine_spark", 99) && logged(*resisted, "Radiant damage") &&
              !logged(*resisted, "Necrotic damage"),
          "Divine Spark deals Radiant even to a creature that resists it (SPARK-1)");
}

// A battle in which Turn Undead turned the zombie: the first seed whose
// Wisdom save fails.
std::unique_ptr<CombatSession> turned(const RulesModule &module)
{
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(module, cleric(2), "walker", seed);
        check(submit(*c, "turn_undead", 1) && channel(*c) == 1, "Turn Undead spends a use");
        if (logged(*c, "Enemy is turned."))
            return c;
    }
    throw std::runtime_error("No seed fails the Wisdom save");
}

std::vector<std::string> verbs(const CombatSession &c)
{
    std::vector<std::string> result;
    for (const auto &command : c.legal_commands())
        result.push_back(command.verb);
    return result;
}

void turn_undead_checks()
{
    auto module = rules();
    auto c = turned(*module);
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Turned survives a checkpoint");
    check(submit(*c, "end"), "End the Cleric's turn");
    while (c->snapshot().actor != 99)
        check(submit(*c, "end"), "Reach the zombie");
    check(verbs(*c) == std::vector<std::string>{"move"}, "A turned Undead can only flee");
    check(submit(*c, "move") && verbs(*c) == std::vector<std::string>{"end"},
          "It flees as far as it can, then its turn ends");

    auto damaged = turned(*module);
    check(submit(*damaged, "end"), "End the Cleric's turn");
    // The fleeing zombie must move before it may end its turn.
    while (damaged->snapshot().actor != 1)
        check(submit(*damaged, "end") || submit(*damaged, "move"), "Back to the Cleric");
    // Sacred Flame reaches the fled zombie; this seed's save fails.
    check(submit(*damaged, "sacred_flame", 99) && unit(*damaged, 99).hit_points < 1000,
          "Sacred Flame harms the turned zombie");
    check(submit(*damaged, "end"), "End the Cleric's turn");
    while (damaged->snapshot().actor != 99)
        check(submit(*damaged, "end"), "Reach the zombie");
    check(verbs(*damaged).size() > 2, "Damage ends the turning");
}

// An enemy Wizard with Hideous Laughter prepared.
Character laughing_wizard()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "male";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_evil";
    d.name = "Foe";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = {"fire_bolt", "ray_of_frost", "chill_touch"};
    const std::vector<std::string> spells{"magic_missile", "hideous_laughter"};
    d.spells = SpellChoices{{{"spellbook:1", spells}}, spells, {}, {}};
    d.training = {{"class:wizard", {"medicine", "nature"}}};
    return Character(*srd5::character_rules(), d, {});
}

// Turn Undead ends once the Cleric is Incapacitated: the zombie stops fleeing.
void incapacitated_turning_checks()
{
    auto module = rules();
    const auto profile = [&](const Character &who, std::vector<std::string> equipment)
    {
        return module->character_profile(who.sheet(), equipment).data;
    };
    const auto hero = profile(cleric(2), {"mace"});
    const auto foe = profile(laughing_wizard(), {});
    for (std::uint64_t seed = 1; seed < 128; ++seed)
    {
        auto c = module->create({{12, 4, std::vector<Terrain>(48)},
                                 {{1, "campaign-character", "Cleric", Side::party, {1, 1}, hero},
                                  {98, "campaign-character", "Foe", Side::opposition, {7, 1}, foe},
                                  {99, "walker", "Enemy", Side::opposition, {3, 1}}}},
                                seed);
        // The turned zombie must move before it may end its turn.
        const auto reach = [&](EntityId id)
        {
            while (c->snapshot().actor != id)
                check(submit(*c, "end") || submit(*c, "move"), "Pass the turn");
        };
        reach(1);
        check(submit(*c, "turn_undead", 1), "The Cleric uses Turn Undead");
        if (!logged(*c, "Enemy is turned."))
            continue;
        reach(98);
        check(submit(*c, "hideous_laughter", 1), "The Foe casts Hideous Laughter");
        if (!logged(*c, "Cleric falls Prone, laughing."))
            continue;
        reach(99);
        check(verbs(*c).size() > 2, "The laughing Cleric's Turn Undead ends");
        return;
    }
    throw std::runtime_error("No seed turns the zombie and makes the Cleric laugh");
}

// A level-two Cleric beside a wounded ally and a vanguard, for
// tests/divine_spark_view_tests.gd. The game loads it with the standard rules content.
void write_ui_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile =
        module->character_profile(cleric(2).sheet(), std::vector<std::string>{"mace"}).data;
    auto c =
        module->create({{12, 9, std::vector<Terrain>(108)},
                        {{1, "campaign-character", "Cleric", Side::party, {1, 1}, profile},
                         {2, "vanguard", "Ally", Side::party, {1, 2}, {}, VitalState{3, false, {}}},
                         {99, "vanguard", "Enemy", Side::opposition, {6, 1}}}},
                       2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "cleric-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream out(path / "spark.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");
}

// HP restored, read from the last "recovers N HP" line.
int recovered(const CombatSession &c)
{
    const auto log = c.snapshot().log();
    for (auto line = log.rbegin(); line != log.rend(); ++line)
        if (const auto at = line->find(" recovers "); at != std::string::npos)
            return std::stoi(line->substr(at + 10));
    throw std::runtime_error("No healing in the log");
}

void life_domain_checks()
{
    auto module = rules();
    const auto life = cleric(3);
    const auto access = module->spell_access(life.sheet());
    check(access.always_prepared ==
              std::vector<std::string>{"aid", "bless", "cure_wounds", "lesser_restoration"},
          "Life Domain spells are always prepared from level three");

    // The same rolls, with and without Disciple of Life.
    auto before = battle(*module, cleric(2));
    auto after = battle(*module, life);
    check(submit(*before, "cure_wounds", 2) && submit(*after, "cure_wounds", 2) &&
              recovered(*after) == recovered(*before) + 3,
          "Disciple of Life adds 2 + the slot's level to healing");

    auto preserve = battle(*module, life);
    check(submit(*preserve, "preserve_life", 1) && unit(*preserve, 2).hit_points == 3 + 15 &&
              channel(*preserve) == 1 && logged(*preserve, "Cleric uses Preserve Life."),
          "Preserve Life restores five times the Cleric level to a Bloodied ally");
}

void lesser_restoration_checks()
{
    auto module = rules();
    // Blinded, Paralyzed and Poisoned, with saves the ally cannot make.
    const std::string afflicted =
        "SRD13 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 0 FX8 4 3 "
        "1 1 77 99 \"Caster\" 30 60000 6000 2 24 77 99 \"Caster\" 30 60000 6000 "
        "3 32 77 99 \"Caster\" 0 12000 0 0";
    auto plain = battle(*module, cleric(3));
    check(!submit(*plain, "lesser_restoration_blinded", 2),
          "Lesser Restoration needs a condition to end");
    auto c = battle(*module, cleric(3), "target", 5, afflicted);
    std::vector<std::string> choices;
    for (const auto &command : c->legal_commands())
        if (command.verb.starts_with("lesser_restoration") && command.target == 2)
            choices.push_back(command.label);
    check(choices == std::vector<std::string>{"Lesser Restoration: Blinded",
                                              "Lesser Restoration: Paralyzed",
                                              "Lesser Restoration: Poisoned"},
          "Each of the ally's conditions is a choice");
    const auto conditions = [&]
    {
        return unit(*c, 2).conditions.size();
    };
    const auto before = conditions();
    check(submit(*c, "lesser_restoration_paralyzed", 2) &&
              logged(*c, "Ally is no longer Paralyzed.") && conditions() == before - 1 &&
              !unit(*c, 1).bonus_action && unit(*c, 1).action,
          "Lesser Restoration ends the chosen condition as a Bonus Action");
}

void aid_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric(3));
    const auto ally_before = unit(*c, 2), cleric_before = unit(*c, 1);
    check(submit(*c, "aid", 2) && c->snapshot().spell_targeting && submit(*c, "aid", 1) &&
              submit(*c, "spell_cast"),
          "Aid chooses up to three creatures, cast early on two");
    check(unit(*c, 2).max_hit_points == ally_before.max_hit_points + 5 &&
              unit(*c, 2).hit_points == ally_before.hit_points + 5 &&
              unit(*c, 1).max_hit_points == cleric_before.max_hit_points + 5 &&
              unit(*c, 1).hit_points == cleric_before.hit_points + 5,
          "Aid raises the Hit Point maximum and current Hit Points by 5");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Aid survives a checkpoint");
}

void aid_expiry_checks()
{
    // An aided member above its sheet's maximum, as after a fight.
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(cleric(1));
    const int sheet_maximum = party.member(id).character.sheet().hit_points;
    auto state = party.checkpoint();
    state.roster.front().vitals = {
        sheet_maximum + 5, false,
        "SRD13 0 2 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 0 FX8 2 1 1 21 77 99 \"Caster\" 5 28800000 0 0"};
    party.restore(std::move(state));
    check(party.hit_point_maximum(id) == sheet_maximum + 5, "Aid raises the campaign maximum");
    party.advance_time(std::chrono::minutes(481));
    check(party.hit_point_maximum(id) == sheet_maximum &&
              party.member(id).vitals.hit_points == sheet_maximum,
          "When Aid ends after 8 hours the maximum and the extra Hit Points go");
}

// The attack bonus a log line shows: "... d20 N + B vs AC ...".
int logged_bonus(const CombatSession &c)
{
    const auto log = c.snapshot().log();
    for (auto line = log.rbegin(); line != log.rend(); ++line)
        if (const auto at = line->find(" + ");
            at != std::string::npos && line->find(" vs AC ") != std::string::npos)
            return std::stoi(line->substr(at + 3));
    throw std::runtime_error("No attack in the log");
}

void guiding_bolt_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric(1, {"guiding_bolt"}));
    check(submit(*c, "guiding_bolt", 99) && logged(*c, "Enemy is lit by Guiding Bolt.") &&
              unit(*c, 99).conditions.size() == 1,
          "Guiding Bolt hits and marks the target");
    check(submit(*c, "end"), "End the Cleric's turn");
    // The enemy's own attack is not against itself; the mark waits.
    while (c->snapshot().actor != 1)
        check(submit(*c, "end"), "Back to the Cleric");
    check(c->submit({c->snapshot().revision, 1, 0, "move", "Move", Cell{2, 1}}) &&
              submit(*c, "melee", 99) && logged(*c, "(advantage)") &&
              unit(*c, 99).conditions.empty(),
          "The next attack roll against it has Advantage, and spends the mark");
}

void bane_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, cleric(1, {"bane"}), "target", seed);
        check(submit(*c, "bane", 99) && c->snapshot().spell_targeting && submit(*c, "spell_cast"),
              "Bane is cast on the chosen creature");
        if (!logged(*c, "Enemy is weakened by Bane."))
            continue;
        check(submit(*c, "end"), "End the Cleric's turn");
        while (c->snapshot().actor != 99)
            check(submit(*c, "end"), "Reach the enemy");
        check(c->submit({c->snapshot().revision, 99, 0, "move", "Move", Cell{2, 1}}) &&
                  submit(*c, "melee", 1) && logged_bonus(*c) < 20 && logged_bonus(*c) >= 16,
              "Bane subtracts 1d4 from the creature's attack roll");
        return;
    }
    throw std::runtime_error("No seed fails the Charisma save");
}

void spare_the_dying_checks()
{
    auto module = rules();
    auto hero = cleric(1);
    const auto profile =
        module->character_profile(hero.sheet(), std::vector<std::string>{"mace"}).data;
    auto c =
        module->create({{12, 4, std::vector<Terrain>(48)},
                        {{1, "campaign-character", "Cleric", Side::party, {1, 1}, profile},
                         {2, "target", "Ally", Side::party, {3, 1}, {}, VitalState{0, false, {}}},
                         {99, "target", "Enemy", Side::opposition, {9, 1}}}},
                       5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    check(submit(*c, "spare_the_dying", 2) &&
              logged(*c, "Cleric casts Spare the Dying: Ally is Stable."),
          "Spare the Dying stabilizes a dying ally within 15 feet");
    check(!submit(*c, "spare_the_dying", 2), "A Stable creature is not offered again");
}

// A level-three Cleric who prepared Hold Person at level three.
Character holding_cleric()
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(cleric(1));
    party.award_experience(900, "hold-xp");
    party.advance(id, party.default_advancement(id));
    auto third = party.default_advancement(id);
    third.spells.back() = "hold_person";
    party.advance(id, third);
    return party.member(id).character;
}

void hold_person_checks()
{
    auto module = rules();
    const auto hero = holding_cleric();
    auto undead = battle(*module, hero, "walker");
    check(!submit(*undead, "hold_person", 99), "Hold Person holds only a Humanoid");
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, hero, "target", seed);
        check(submit(*c, "hold_person", 99), "Hold Person is cast");
        if (!logged(*c, "Enemy is Paralyzed."))
            continue;
        const auto saved = c->save();
        check(module->restore(saved)->save() == saved, "Paralyzed survives a checkpoint");
        check(submit(*c, "end"), "End the Cleric's turn");
        while (c->snapshot().actor != 99)
            check(submit(*c, "end"), "Reach the held creature");
        const auto offered = c->legal_commands();
        check(offered.size() == 1 && offered.front().verb == "end",
              "A Paralyzed creature can only end its turn");
        check(submit(*c, "end") && logged(*c, "Enemy Wisdom save"),
              "It repeats the Wisdom save at the end of its turn");
        while (c->snapshot().actor != 1)
            check(submit(*c, "end"), "Back to the Cleric");
        if (logged(*c, "Enemy is no longer Paralyzed."))
            continue;
        check(submit(*c, "sacred_flame", 99) &&
                  logged(*c, "Enemy automatically fails the Dexterity save while Paralyzed."),
              "A Paralyzed creature fails Dexterity saves");
        return;
    }
    throw std::runtime_error("No seed keeps the creature Paralyzed");
}

// A level-three Cleric who prepared the named level-two spell at level three.
Character cleric_with(std::string spell)
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(cleric(1, {"sanctuary"}));
    party.award_experience(900, "ward-xp");
    party.advance(id, party.default_advancement(id));
    auto third = party.default_advancement(id);
    third.spells.back() = std::move(spell);
    party.advance(id, third);
    return party.member(id).character;
}

// Moves the enemy (99) beside the ally (2) on its turn and attacks the ally.
void enemy_attacks_ally(CombatSession &c)
{
    check(submit(c, "end"), "End the Cleric's turn");
    while (c.snapshot().actor != 99)
        check(submit(c, "end"), "Reach the enemy");
    check(c.submit({c.snapshot().revision, 99, 0, "move", "Move", Cell{2, 2}}) &&
              submit(c, "melee", 2),
          "The enemy attacks the ally");
}

void sanctuary_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, cleric(1, {"sanctuary"}), "target", seed);
        check(submit(*c, "sanctuary", 2) && !unit(*c, 1).bonus_action && unit(*c, 1).action,
              "Sanctuary wards an ally as a Bonus Action");
        const int hp = unit(*c, 2).hit_points;
        enemy_attacks_ally(*c);
        check(logged(*c, "Enemy Wisdom save"), "Attacking the warded ally calls for a Wisdom save");
        if (!logged(*c, "Enemy's attack on Ally is lost to Sanctuary."))
            continue;
        check(unit(*c, 2).hit_points == hp, "A failed save loses the attack");
        return;
    }
    throw std::runtime_error("No seed fails the Wisdom save");
}

void warding_bond_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric_with("warding_bond"));
    const int ac = unit(*c, 2).armor_class;
    check(!submit(*c, "warding_bond", 1), "Warding Bond binds another creature");
    check(submit(*c, "warding_bond", 2) && unit(*c, 2).armor_class == ac + 1,
          "Warding Bond adds 1 to the ally's AC");
    const int cleric_hp = unit(*c, 1).hit_points;
    enemy_attacks_ally(*c);
    check(logged(*c, "Ally: Slashing damage") || logged(*c, "Ally: Bludgeoning damage") ||
              logged(*c, "Ally: Piercing damage"),
          "The bonded ally resists the damage");
    check(logged(*c, "Cleric shares") && unit(*c, 1).hit_points < cleric_hp,
          "The Cleric takes the same damage");
}

void protection_from_poison_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric_with("protection_from_poison"), "viper");
    check(submit(*c, "protection_from_poison", 2) &&
              logged(*c, "Ally gains Protection from Poison."),
          "Protection from Poison wards an ally");
    enemy_attacks_ally(*c);
    check(logged(*c, "Ally: Poison damage"), "The ally resists Poison damage");
}

void resistance_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric(1), "viper");
    std::vector<std::string> labels;
    for (const auto &command : c->legal_commands())
        if (command.verb.starts_with("resistance_") && command.target == 2)
            labels.push_back(command.label);
    check(labels.size() == 11 && labels.front() == "Resistance: Acid",
          "Resistance is offered once per damage type");
    check(submit(*c, "resistance_poison", 2) &&
              logged(*c, "Ally gains Resistance against Poison damage."),
          "Resistance wards the ally against the chosen type");
    enemy_attacks_ally(*c);
    check(logged(*c, "Ally's Resistance reduces the damage by"), "It takes 1d4 less of that type");
}

// GUIDE-1: Guidance is offered for each skill the game checks in a fight and
// adds 1d4 to the creature's checks with the chosen one.
void guidance_checks()
{
    auto module = rules();
    const auto guide = cleric(1, {"cure_wounds"}, {"sacred_flame", "spare_the_dying", "guidance"});
    const auto profile =
        module->character_profile(guide.sheet(), std::vector<std::string>{"mace"}).data;
    auto c =
        module->create({{12, 4, std::vector<Terrain>(48)},
                        {{1, "campaign-character", "Cleric", Side::party, {1, 1}, profile},
                         {2, "campaign-character", "Healer", Side::party, {2, 1}, profile},
                         {3, "target", "Ally", Side::party, {3, 1}, {}, VitalState{0, false, {}}},
                         {99, "target", "Enemy", Side::opposition, {9, 1}}}},
                       5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    std::vector<std::string> labels;
    for (const auto &command : c->legal_commands())
        if (command.verb.starts_with("guidance_") && command.target == 2)
            labels.push_back(command.label);
    check(labels == std::vector<std::string>{"Guidance: Athletics", "Guidance: Medicine"},
          "Guidance is offered for Athletics and Medicine");
    check(submit(*c, "guidance_medicine", 2) &&
              logged(*c, "Healer gains Guidance for Medicine checks."),
          "Guidance touches an ally for the chosen skill");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Guidance survives a checkpoint");
    check(submit(*c, "end"), "End the Cleric's turn");
    for (unsigned turns = 0; c->snapshot().actor != 2 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Healer's turn");
    check(submit(*c, "stabilize", 3) && logged(*c, "Healer's Guidance adds ") &&
              logged(*c, "Healer Medicine: "),
          "The guided creature adds 1d4 to its Medicine check");
}

void silence_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric_with("silence"));
    check(submit(*c, "silence") &&
              c->submit({c->snapshot().revision, 1, 0, "area_move", "", Cell{1, 1}}) &&
              submit(*c, "area_cast") && logged(*c, "Cleric casts Silence."),
          "Silence is aimed and cast");
    const auto silenced = c->snapshot().silenced;
    check(std::find(silenced.begin(), silenced.end(), Cell{1, 1}) != silenced.end(),
          "The sphere is shown as silenced");
    check(submit(*c, "end"), "End the Cleric's turn");
    while (c->snapshot().actor != 1)
        check(submit(*c, "end") || submit(*c, "move"), "Back to the Cleric");
    check(submit(*c, "sacred_flame", 99),
          "Spells have no components (CLASS-11), so Silence does not stop casting");
}

void spiritual_weapon_checks()
{
    auto module = rules();
    auto c = battle(*module, cleric_with("spiritual_weapon"));
    check(submit(*c, "spiritual_weapon", 99) && !unit(*c, 1).bonus_action && unit(*c, 1).action,
          "Spiritual Weapon is a Bonus Action spell");
    const auto forces = c->snapshot().spiritual_weapons;
    const auto enemy = unit(*c, 99).cell;
    check(forces.size() == 1 &&
              std::max(std::abs(forces[0].x - enemy.x), std::abs(forces[0].y - enemy.y)) == 1 &&
              logged(*c, "Cleric -> Enemy"),
          "The force appears beside the target and attacks it");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The force survives a checkpoint");
    check(!submit(*c, "spiritual_weapon_strike", 99),
          "Its later attack waits for a new Bonus Action");
    check(submit(*c, "end"), "End the Cleric's turn");
    while (c->snapshot().actor != 1)
        check(submit(*c, "end"), "Back to the Cleric");
    check(submit(*c, "spiritual_weapon_strike", 99) &&
              logged(*c, "Cleric's Spiritual Weapon strikes."),
          "A Bonus Action strikes again");
    check(submit(*c, "bless", 1) && submit(*c, "spell_cast") &&
              c->snapshot().spiritual_weapons.empty(),
          "A new Concentration spell ends the force");
}

// Channel Divinity's uses are spent in fights, kept by a campaign save, and
// recovered one by a Short Rest and all by a Long Rest.
void channel_divinity_rest_checks()
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(cleric(2));
    const auto uses = [](const CampaignParty &p, MemberId member)
    {
        const auto &m = p.member(member);
        for (const auto &pool :
             p.rule_module().recovery_info(m.character.sheet(), m.vitals).resources)
            if (pool.id == "channel_divinity")
                return static_cast<int>(pool.remaining);
        return -1;
    };
    check(uses(party, id) == 2, "A level-two Cleric has two Channel Divinity uses");
    for (int fight = 0; fight < 2; ++fight)
    {
        Encounter e{{12, 6, std::vector<Terrain>(72)}, party.participants()};
        e.participants.front().cell = {1, 1};
        e.participants.push_back({99, "bandit", "Enemy", Side::opposition, {3, 1}});
        auto c = party.rule_module().create(e, 5);
        for (unsigned turns = 0; c->snapshot().actor != id && turns < 4; ++turns)
            check(submit(*c, "end"), "Reach the Cleric's turn");
        check(submit(*c, "divine_spark", 99), "Divine Spark at the enemy");
        party.begin_combat();
        party.apply_combat(c->snapshot());
        party.end_combat();
    }
    check(uses(party, id) == 0, "Both uses are spent after two fights");
    const auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto bytes = encode_campaign(party, nullptr, "channel");
    CampaignParty copy(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    copy.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "channel", nullptr).party);
    check(uses(copy, id) == 0, "Spent uses survive a campaign save");
    check(bool(copy.rest(RestKind::short_rest)) && uses(copy, id) == 1,
          "A Short Rest restores one use");
    if (copy.state().short_rest)
        copy.finish_short_rest(copy.state().short_rest->ticket);
    copy.advance_time(std::chrono::hours{24});
    check(bool(copy.rest(RestKind::long_rest)) && uses(copy, id) == 2,
          "A Long Rest restores all of them");
}

} // namespace

int main()
{
    try
    {
        divine_spark_checks();
        channel_divinity_rest_checks();
        turn_undead_checks();
        incapacitated_turning_checks();
        life_domain_checks();
        lesser_restoration_checks();
        aid_checks();
        aid_expiry_checks();
        guiding_bolt_checks();
        bane_checks();
        spare_the_dying_checks();
        hold_person_checks();
        sanctuary_checks();
        warding_bond_checks();
        protection_from_poison_checks();
        resistance_checks();
        guidance_checks();
        silence_checks();
        spiritual_weapon_checks();
        write_ui_fixture();
        std::cout << "Cleric Channel Divinity tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
