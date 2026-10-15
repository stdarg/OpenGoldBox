#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// A Paladin (Oath of Devotion) from level 5 to 10 (LEVELS-1): Extra Attack,
// level-2 and level-3 slots and spells with Shining Smite, Beacon of Hope and
// Dispel Magic, casting from higher slots, Aura of Protection, Aura of
// Devotion and Abjure Foes.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

CharacterDraft draft()
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
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds", "searing_smite"}, {}, {}};
    return d;
}

// A Paladin of the level, advanced through the rules' default level-ups.
Character paladin(unsigned level)
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
        check(hero.advance(*rules, life, rules->default_advancement(hero.sheet())),
              "An ordinary level-up");
    return hero;
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

// Spell slots of a level left: from the third field of the saved vital state,
// one field per level.
unsigned slots(const CombatSession &c, unsigned level)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
        in >> count;
    return count;
}

bool has_condition(const CombatSession &c, EntityId id, std::string_view label)
{
    const auto conditions = unit(c, id).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [&](const auto & condition)
    {
        return condition.source == label;
    });
}

// The Paladin beside an AC 1 dummy, with a second dummy farther off, on the
// Paladin's turn.
std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::uint64_t seed, std::optional<VitalState> state = {})
{
    const std::vector<std::string> gear{"longsword"};
    const auto profile = rules.character_profile(hero.sheet(), gear).data;
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
        {   {1, "campaign-character", "Paladin", Side::party, {1, 1}, profile, state},
            {98, "dummy", "Dummy", Side::opposition, {2, 1}},
            {97, "dummy", "Far Dummy", Side::opposition, {6, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    return c;
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(85000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    const auto &ten = party.member(id).character.sheet();
    check(ten.level == 10, "In the campaign a Paladin stops at the hall's level 10");
    check(!rules->advancement_options(paladin(12).sheet()).level,
          "Level 12 is the Paladin's cap until its level-4 spells exist");
    const auto holds = [&](std::string_view grant, unsigned level)
    {
        return std::any_of(ten.grants.begin(), ten.grants.end(), [&](const auto & g)
        {
            return g.id == grant && g.level == level;
        });
    };
    check(holds("feature:extra_attack", 5) && holds("feature:aura_of_protection", 6) &&
          holds("feature:aura_of_devotion", 7) && holds("feature:abjure_foes", 9) &&
          holds("feature:aura_of_courage", 10), "The level 5-10 features arrive at their levels");
    const auto access = rules->spell_access(ten);
    check(access.prepared_choices == 9 && access.prepared.size() == 9,
          "Nine prepared spells at level 10");
    const auto always = [&](std::string_view spell)
    {
        return std::find(access.always_prepared.begin(), access.always_prepared.end(), spell) !=
               access.always_prepared.end();
    };
    check(always("aid") && always("beacon_of_hope") && always("dispel_magic"),
          "Aid from level 5, Beacon of Hope and Dispel Magic from level 9 are always prepared");
    check(rules->spell_access(paladin(8).sheet()).prepared_choices == 7,
          "Seven prepared spells at level 8");
    check(rules->spell_access(paladin(5).sheet()).prepared_choices == 6,
          "Six prepared spells at level 5");
    const auto options = rules->spell_choice_options(paladin(5).sheet(),
                         SpellChoiceContext::long_rest);
    const auto preparable = [&](std::string_view spell)
    {
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto & o)
        {
            return o.id == spell;
        });
    };
    check(preparable("shining_smite") && preparable("magic_weapon") &&
          preparable("warding_bond") && preparable("lesser_restoration") &&
          preparable("prayer_of_healing") && preparable("protection_from_poison") &&
          !preparable("aid"), "The level-2 Paladin spells can be prepared");
    const auto dummies = dummy_rules();
    auto five = duel(*dummies, paladin(5), 1);
    check(slots(*five, 1) == 4 && slots(*five, 2) == 2,
          "Four level-1 and two level-2 slots at level 5");
    check(slots(*duel(*dummies, paladin(7), 1), 2) == 3,
          "Three level-2 slots from level 7");
    check(slots(*duel(*dummies, paladin(9), 1), 3) == 2 &&
          slots(*duel(*dummies, paladin(8), 1), 3) == 0 &&
          slots(*duel(*dummies, paladin(11), 1), 3) == 3, "Two level-3 slots from level 9, three from 11");
    const auto twelve = paladin(12);
    check(std::any_of(twelve.sheet().grants.begin(), twelve.sheet().grants.end(), [](const auto & g)
    {
        return g.id == "feature:radiant_strikes" && g.level == 11;
    }), "Radiant Strikes at level 11");
    const auto bytes = encode_campaign(party, nullptr, "paladin-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "paladin-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "paladin-levels") == bytes,
          "A level-10 Paladin saves and reloads");
}

void extra_attack_checks()
{
    const auto rules = dummy_rules();
    auto four = duel(*rules, paladin(4), 1);
    check(submit(*four, "melee", 98) && !offered(*four, "melee"), "One attack at level 4");
    auto five = duel(*rules, paladin(5), 1);
    check(submit(*five, "melee", 98) && offered(*five, "melee"),
          "Extra Attack: a second attack at level 5");
}

// After a hit, Divine Smite from a level-2 slot and Shining Smite.
void smite_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*duel(*rules, paladin(4), 1), "shining_smite"), "No Shining Smite before a hit");
    auto c = duel(*rules, paladin(5), 1);
    check(submit(*c, "melee", 98), "The Paladin hits the AC 1 dummy");
    check(offered(*c, "divine_smite_2") && offered(*c, "shining_smite") &&
          offered(*c, "divine_smite"), "Both level-2 smites are offered after the hit");
    check(submit(*c, "shining_smite", 98) && logged(*c, "Paladin casts Shining Smite") &&
          logged(*c, "Dummy shines.") && has_condition(*c, 98, "Shining (Shining Smite)") &&
          slots(*c, 2) == 1, "Shining Smite spends a level-2 slot and lights the dummy");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the shining dummy");
    check(!offered(*c, "divine_smite_2"), "One smite a turn");

    auto again = duel(*rules, paladin(5), 1);
    check(submit(*again, "melee", 98) && submit(*again, "divine_smite_2", 98) &&
          logged(*again, "Paladin casts Divine Smite") &&
          slots(*again, 2) == 1 && slots(*again, 1) == 4,
          "Divine Smite from a level-2 slot");
}

// The Constitution save bonus of the first save logged for `name`, if any.
std::optional<int> save_bonus(const CombatSession &c, const std::string &name)
{
    const std::string prefix = name + " Constitution save: d20 ";
    for (const auto &line : c.snapshot().log())
        if (const auto at = line.find(prefix); at != std::string::npos)
        {
            const auto plus = line.find(" + ", at + prefix.size());
            return std::stoi(line.substr(plus + 3));
        }
    return std::nullopt;
}

// A Blindness adept's target saves: Ally's bonus, with the Paladin at `cell`
// and the ally at (4, 1).
std::optional<int> ally_bonus(const Character &hero, Cell paladin_cell)
{
    const auto rules = module();
    const auto ally = paladin(4);
    for (std::uint64_t seed = 1; seed < 200; ++seed)
    {
        auto c = rules->create({{10, 3, std::vector<Terrain>(30)},
            {   {1, "campaign-character", "Paladin", Side::party, paladin_cell,
                    rules->character_profile(hero.sheet(), {}).data},
                {2, "campaign-character", "Ally", Side::party, {4, 1},
                    rules->character_profile(ally.sheet(), {}).data},
                {98, "blindness-adept", "Adept", Side::opposition, {9, 1}}
            }},
        seed);
        for (unsigned turns = 0; turns < 8 && !save_bonus(*c, "Ally"); ++turns)
            if (c->snapshot().actor != 98 || c->snapshot().reaction_pending)
                check(submit(*c, "decline") || submit(*c, "end"), "The party waits");
            else
                check(c->submit(choose_demo_command(*c)), "The adept acts");
        if (const auto bonus = save_bonus(*c, "Ally"))
            return bonus;
    }
    return std::nullopt;
}

// Aura of Protection: allies within 10 feet add the Paladin's Charisma
// modifier (at least +1) to saves.
void aura_checks()
{
    const auto six = paladin(6);
    const int charisma = std::max(1, (six.sheet().scores[Ability::charisma] - 10) / 2);
    const auto below = ally_bonus(paladin(5), {3, 1}), near = ally_bonus(six, {3, 1}),
               edge = ally_bonus(six, {2, 0}), far = ally_bonus(six, {0, 1});
    check(below && near && edge && far, "The adept blinds the ally");
    check(*near == *below + charisma && *edge == *below + charisma,
          "Allies within 10 feet add the Paladin's Charisma modifier");
    check(*far == *below, "An ally 20 feet away does not");
}
// A dying ally beside the Paladin adds the aura to its death saves.
bool aura_death_save(const Character &hero)
{
    const auto rules = dummy_rules();
    auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
        {   {1, "campaign-character", "Paladin", Side::party, {1, 1},
                rules->character_profile(hero.sheet(), {}).data},
            {2, "campaign-character", "Ally", Side::party, {2, 1},
                rules->character_profile(paladin(4).sheet(), {}).data, VitalState{0, false, {}, {}}},
            {98, "dummy", "Dummy", Side::opposition, {7, 1}}
        }},
    1);
    for (unsigned turns = 0; turns < 6 && !logged(*c, "Ally death save:"); ++turns)
        check(submit(*c, "end"), "Turns pass");
    check(logged(*c, "Ally death save:"), "The ally makes a death save");
    return logged(*c, " (Aura of Protection)");
}

void death_save_checks()
{
    check(aura_death_save(paladin(6)) && !aura_death_save(paladin(5)),
          "Aura of Protection adds to death saves, shown with the roll");
}
bool aim(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "area_move" && command.destination == cell)
            return c.submit(command);
    return false;
}

// Each turn until the Paladin's next one.
void next_turn(CombatSession &c)
{
    check(submit(c, "end"), "The Paladin ends its turn");
    for (unsigned turns = 0; c.snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(c, "decline") || submit(c, "end") || c.submit(choose_demo_command(c)),
              "Reach the Paladin's turn");
}

// Casting from higher slots: offered when it adds something, and paid from
// that slot.
void upcast_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, paladin(9), 1, VitalState{10, false, {}, {}});
    check(offered(*c, "cure_wounds") && offered(*c, "cure_wounds_2") &&
          offered(*c, "cure_wounds_3"), "Cure Wounds is offered from each slot level");
    check(offered(*c, "protection_from_evil_and_good") &&
          !offered(*c, "protection_from_evil_and_good_2") &&
          !offered(*c, "protection_from_evil_and_good_3"),
          "A spell a higher slot does not change is offered at its own level");
    check(submit(*c, "cure_wounds_3", 1) && logged(*c, "Paladin recovers") && slots(*c, 3) == 1 &&
          slots(*c, 1) == 4, "Cure Wounds from a level-3 slot spends it");

    auto smite = duel(*rules, paladin(9), 1);
    check(submit(*smite, "melee", 98) && offered(*smite, "divine_smite_3") &&
          offered(*smite, "shining_smite_3") && submit(*smite, "divine_smite_3", 98) &&
          slots(*smite, 3) == 1, "Divine Smite from a level-3 slot");
}

// Abjure Foes: a Channel Divinity use against creatures chosen one at a time.
void abjure_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*duel(*rules, paladin(8), 1), "abjure_foes"), "No Abjure Foes before level 9");
    const auto hero = paladin(9);
    std::unique_ptr<CombatSession> c;
    for (std::uint64_t seed = 1;
            seed < 40 && !(c && has_condition(*c, 98, "Frightened (Abjure Foes)")); ++seed)
    {
        c = duel(*rules, hero, seed);
        check(submit(*c, "abjure_foes", 98), "Abjure Foes starts with the first creature");
        if (c->snapshot().spell_targeting)
            check(submit(*c, "abjure_foes", 97) &&
                  (!c->snapshot().spell_targeting || submit(*c, "spell_cast")),
                  "A second creature, then the use");
        check(logged(*c, "Paladin uses Abjure Foes.") && !unit(*c, 1).action,
              "Abjure Foes takes the Magic action");
    }
    check(has_condition(*c, 98, "Frightened (Abjure Foes)"), "A failed Wisdom save frightens");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the abjured dummy");
    next_turn(*c);
    check(submit(*c, "melee", 98) && !has_condition(*c, 98, "Frightened (Abjure Foes)"),
          "Damage ends it");
}

// Beacon of Hope: allies within 30 feet; their healing is maximized.
void beacon_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, paladin(9), 1, VitalState{10, false, {}, {}});
    check(submit(*c, "beacon_of_hope", 1) && logged(*c, "Paladin gains Beacon of Hope.") &&
          has_condition(*c, 1, "Beacon of Hope"), "Beacon of Hope lights the Paladin");
    next_turn(*c);
    check(submit(*c, "cure_wounds", 1) &&
          logged(*c, "Paladin's healing is maximized (Beacon of Hope)."),
          "Healing is maximized under Beacon of Hope");
}

// Dispel Magic, aimed at a square, ends the spells on the creature there.
void dispel_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, paladin(9), 1);
    check(submit(*c, "melee", 98) && submit(*c, "shining_smite", 98) &&
          has_condition(*c, 98, "Shining (Shining Smite)"), "Shining Smite lights the dummy");
    next_turn(*c);
    check(submit(*c, "dispel_magic") && aim(*c, Cell{2, 1}) && submit(*c, "area_cast") &&
          logged(*c, "Dispel Magic ends a spell on Dummy.") &&
          !has_condition(*c, 98, "Shining (Shining Smite)") &&
          logged(*c, "Paladin loses Concentration."), "Dispel Magic ends Shining Smite");
}
// Radiant Strikes: melee hits deal 1d8 Radiant more; Channel Divinity has three uses.
void radiant_checks()
{
    const auto rules = dummy_rules();
    auto ten = duel(*rules, paladin(10), 1);
    check(submit(*ten, "melee", 98) && !logged(*ten, "Radiant Strikes"), "None at level 10");
    auto c = duel(*rules, paladin(11), 1);
    check(submit(*c, "melee", 98) &&
          logged(*c, "Dummy takes ") && logged(*c, " Radiant damage from Radiant Strikes."),
          "A melee hit adds Radiant Strikes");
    const auto pools = unit(*c, 1).resources;
    check(std::any_of(pools.begin(), pools.end(), [](const auto & pool)
    {
        return pool.id == "channel_divinity" && pool.capacity == 3;
    }), "Three Channel Divinity uses from level 11");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        extra_attack_checks();
        smite_checks();
        aura_checks();
        death_save_checks();
        upcast_checks();
        abjure_checks();
        beacon_checks();
        dispel_checks();
        radiant_checks();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Paladin level checks passed\n";
}
