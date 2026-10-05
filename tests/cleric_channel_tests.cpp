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

// A sturdy AC 1 target that hits back, and an Undead zombie of the same build.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature zombie 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "type zombie undead\n");
}


// A Cleric of the given level, advanced through the campaign, with the named
// level-one spells prepared.
Character cleric(unsigned level, std::vector<std::string> prepared = {"cure_wounds"})
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
    d.cantrips = std::vector<std::string> {"sacred_flame", "spare_the_dying"};
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {"protector"}}
    };
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
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
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
        module.character_profile(hero.sheet(), std::vector<std::string> {"mace"}).data;
    auto c = module.create({{12, 4, std::vector<std::uint8_t>(48)},
        {   {1, "campaign-character", "Cleric", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, {1, 2}, {}, VitalState{3, false, std::move(ally_state)}},
            {99, std::move(enemy), "Enemy", 1, {3, 1}}
        }},
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
}

// A battle in which Turn Undead turned the zombie: the first seed whose
// Wisdom save fails.
std::unique_ptr<CombatSession> turned(const RulesModule &module)
{
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(module, cleric(2), "zombie", seed);
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
    check(verbs(*c) == std::vector<std::string> {"move"}, "A turned Undead can only flee");
    check(submit(*c, "move") && verbs(*c) == std::vector<std::string> {"end"},
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
// A level-two Cleric beside a wounded ally and a vanguard, for
// tests/divine_spark_view_tests.gd. The game loads it with the standard rules content.
void write_ui_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile =
        module->character_profile(cleric(2).sheet(), std::vector<std::string> {"mace"}).data;
    auto c = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Cleric", 0, {1, 1}, profile},
            {2, "vanguard", "Ally", 0, {1, 2}, {}, VitalState{3, false, {}}},
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
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
    const auto log = c.snapshot().log;
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
          std::vector<std::string> {"aid", "bless", "cure_wounds", "lesser_restoration"},
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
    const std::string blinded =
        "SRD11 0 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 FX8 2 1 1 1 77 99 \"Caster\" 13 60000 6000 0";
    auto plain = battle(*module, cleric(3));
    check(!submit(*plain, "lesser_restoration", 2), "Lesser Restoration needs a Blinded creature");
    auto c = battle(*module, cleric(3), "target", 5, blinded);
    const auto conditions = [&]
    {
        return unit(*c, 2).conditions.size();
    };
    check(conditions() == 1, "The ally starts Blinded");
    check(submit(*c, "lesser_restoration", 2) && conditions() == 0 && !unit(*c, 1).bonus_action &&
          unit(*c, 1).action,
          "Lesser Restoration ends Blinded as a Bonus Action");
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
    state.roster.front().vitals = {sheet_maximum + 5, false,
        "SRD11 0 2 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 2 1 1 21 77 99 \"Caster\" 5 28800000 0 0"
    };
    party.restore(state);
    check(party.hit_point_maximum(id) == sheet_maximum + 5, "Aid raises the campaign maximum");
    party.advance_time(481);
    check(party.hit_point_maximum(id) == sheet_maximum &&
          party.member(id).vitals.hit_points == sheet_maximum,
          "When Aid ends after 8 hours the maximum and the extra Hit Points go");
}
// The attack bonus a log line shows: "... d20 N + B vs AC ...".
int logged_bonus(const CombatSession &c)
{
    const auto log = c.snapshot().log;
    for (auto line = log.rbegin(); line != log.rend(); ++line)
        if (const auto at = line->find(" + "); at != std::string::npos &&
                line->find(" vs AC ") != std::string::npos)
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
        module->character_profile(hero.sheet(), std::vector<std::string> {"mace"}).data;
    auto c = module->create({{12, 4, std::vector<std::uint8_t>(48)},
        {   {1, "campaign-character", "Cleric", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, {3, 1}, {}, VitalState{0, false, {}}},
            {99, "target", "Enemy", 1, {9, 1}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    check(submit(*c, "spare_the_dying", 2) && logged(*c, "Cleric casts Spare the Dying: Ally is Stable."),
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
    auto undead = battle(*module, hero, "zombie");
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
} // namespace

int main()
{
    try
    {
        divine_spark_checks();
        turn_undead_checks();
        life_domain_checks();
        lesser_restoration_checks();
        aid_checks();
        aid_expiry_checks();
        guiding_bolt_checks();
        bane_checks();
        spare_the_dying_checks();
        hold_person_checks();
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
