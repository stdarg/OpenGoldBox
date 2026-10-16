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

// A Paladin (Oath of Devotion) from level 5 to 15 (LEVELS-1, LEVELS-2): Extra
// Attack, slots of levels 2-4 and their spells, casting from higher slots, the
// auras, Abjure Foes, Radiant Strikes, Restoring Touch and Smite of Protection.
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
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
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

// An AC 1 dummy, a Fey one and a brute that always hits for 1d4.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature fey-dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "type fey-dummy fey\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
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
    return std::any_of(conditions.begin(), conditions.end(),
                       [&](const auto &condition)
                       {
                           return condition.source == label;
                       });
}

// The Paladin's sheet with `spells` prepared in place of its last ones.
CharacterSheet preparing(const Character &hero, std::vector<std::string> spells)
{
    auto sheet = hero.sheet();
    auto &prepared = sheet.prepared_spells;
    std::copy(spells.begin(), spells.end(),
              prepared.end() - static_cast<std::ptrdiff_t>(spells.size()));
    return sheet;
}

// The Paladin beside an AC 1 dummy, with a second dummy farther off, on the
// Paladin's turn.
std::unique_ptr<CombatSession> duel(const RulesModule &rules, const CharacterSheet &sheet,
                                    std::uint64_t seed, std::optional<VitalState> state = {})
{
    const std::vector<std::string> gear{"longsword"};
    const auto profile = rules.character_profile(sheet, gear).data;
    auto c =
        rules.create({{8, 3, std::vector<Terrain>(24)},
                      {{1, "campaign-character", "Paladin", Side::party, {1, 1}, profile, state},
                       {98, "dummy", "Dummy", Side::opposition, {2, 1}},
                       {97, "dummy", "Far Dummy", Side::opposition, {6, 1}}}},
                     seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    return c;
}

std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::uint64_t seed, std::optional<VitalState> state = {})
{
    return duel(rules, hero.sheet(), seed, std::move(state));
}

// The Paladin and one creature of `kind`, beside it, on the Paladin's turn.
std::unique_ptr<CombatSession> alone_with(const RulesModule &rules, const CharacterSheet &sheet,
                                          std::string kind, std::uint64_t seed,
                                          std::optional<VitalState> state = {})
{
    const std::vector<std::string> gear{"longsword"};
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
                           {{1,
                             "campaign-character",
                             "Paladin",
                             Side::party,
                             {1, 1},
                             rules.character_profile(sheet, gear).data,
                             state},
                            {98, std::move(kind), "Foe", Side::opposition, {2, 1}}}},
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
    party.award_experience(64000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    const auto &ten = party.member(id).character.sheet();
    check(ten.level == 10, "64,000 XP trains a Paladin to level 10");
    const auto fifteen = paladin(15);
    check(!rules->advancement_options(fifteen.sheet()).level, "Level 15 is the Paladin's cap");
    check(std::any_of(fifteen.sheet().grants.begin(), fifteen.sheet().grants.end(),
                      [](const auto &g)
                      {
                          return g.id == "feature:restoring_touch" && g.level == 14;
                      }) &&
              std::any_of(fifteen.sheet().grants.begin(), fifteen.sheet().grants.end(),
                          [](const auto &g)
                          {
                              return g.id == "feature:smite_of_protection" && g.level == 15;
                          }),
          "Restoring Touch at 14, Smite of Protection at 15");
    const auto top = rules->spell_access(fifteen.sheet());
    const auto always_top = [&](std::string_view spell)
    {
        return std::find(top.always_prepared.begin(), top.always_prepared.end(), spell) !=
               top.always_prepared.end();
    };
    check(
        top.prepared_choices == 12 && always_top("freedom_of_movement") &&
            always_top("guardian_of_faith"),
        "Twelve prepared spells at 15; Freedom of Movement and Guardian of Faith always prepared");
    const auto holds = [&](std::string_view grant, unsigned level)
    {
        return std::any_of(ten.grants.begin(), ten.grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == grant && g.level == level;
                           });
    };
    check(holds("feature:extra_attack", 5) && holds("feature:aura_of_protection", 6) &&
              holds("feature:aura_of_devotion", 7) && holds("feature:abjure_foes", 9) &&
              holds("feature:aura_of_courage", 10),
          "The level 5-10 features arrive at their levels");
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
    const auto options =
        rules->spell_choice_options(paladin(5).sheet(), SpellChoiceContext::long_rest);
    const auto preparable = [&](std::string_view spell)
    {
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto &o)
                           {
                               return o.id == spell;
                           });
    };
    check(preparable("shining_smite") && preparable("magic_weapon") && preparable("warding_bond") &&
              preparable("lesser_restoration") && preparable("prayer_of_healing") &&
              preparable("protection_from_poison") && !preparable("aid"),
          "The level-2 Paladin spells can be prepared");
    const auto dummies = dummy_rules();
    auto five = duel(*dummies, paladin(5), 1);
    check(slots(*five, 1) == 4 && slots(*five, 2) == 2,
          "Four level-1 and two level-2 slots at level 5");
    check(slots(*duel(*dummies, paladin(7), 1), 2) == 3, "Three level-2 slots from level 7");
    check(slots(*duel(*dummies, paladin(9), 1), 3) == 2 &&
              slots(*duel(*dummies, paladin(8), 1), 3) == 0 &&
              slots(*duel(*dummies, paladin(11), 1), 3) == 3,
          "Two level-3 slots from level 9, three from 11");
    check(slots(*duel(*dummies, paladin(13), 1), 4) == 1 &&
              slots(*duel(*dummies, fifteen, 1), 4) == 2,
          "A level-4 slot at 13, two at 15");
    const auto twelve = paladin(12);
    check(std::any_of(twelve.sheet().grants.begin(), twelve.sheet().grants.end(),
                      [](const auto &g)
                      {
                          return g.id == "feature:radiant_strikes" && g.level == 11;
                      }),
          "Radiant Strikes at level 11");
    const auto bytes = encode_campaign(party, nullptr, "paladin-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "paladin-levels", nullptr)
                     .party);
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
              offered(*c, "divine_smite"),
          "Both level-2 smites are offered after the hit");
    check(submit(*c, "shining_smite", 98) && logged(*c, "Paladin casts Shining Smite") &&
              logged(*c, "Dummy shines.") && has_condition(*c, 98, "Shining (Shining Smite)") &&
              slots(*c, 2) == 1,
          "Shining Smite spends a level-2 slot and lights the dummy");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the shining dummy");
    check(!offered(*c, "divine_smite_2"), "One smite a turn");

    auto again = duel(*rules, paladin(5), 1);
    check(submit(*again, "melee", 98) && submit(*again, "divine_smite_2", 98) &&
              logged(*again, "Paladin casts Divine Smite") && slots(*again, 2) == 1 &&
              slots(*again, 1) == 4,
          "Divine Smite from a level-2 slot");
}

// The first Constitution save logged for `name`, if any.
std::optional<std::string> save_line(const CombatSession &c, const std::string &name)
{
    for (const auto &line : c.snapshot().log())
        if (line.starts_with(name + " Constitution save: d20 "))
            return line;
    return std::nullopt;
}

// The save line's bonus, after "d20 N + ".
int bonus_of(const std::string &line)
{
    const auto plus = line.find(" + ");
    return std::stoi(line.substr(plus + 3));
}

// A Blindness adept's target saves: Ally's save line, with the Paladin at
// `cell` and the ally at (4, 1).
std::optional<std::string> ally_save(const Character &hero, Cell paladin_cell)
{
    const auto rules = module();
    const auto ally = paladin(4);
    for (std::uint64_t seed = 1; seed < 200; ++seed)
    {
        auto c = rules->create({{10, 3, std::vector<Terrain>(30)},
                                {{1, "campaign-character", "Paladin", Side::party, paladin_cell,
                                  rules->character_profile(hero.sheet(), {}).data},
                                 {2,
                                  "campaign-character",
                                  "Ally",
                                  Side::party,
                                  {4, 1},
                                  rules->character_profile(ally.sheet(), {}).data},
                                 {98, "blindness-adept", "Adept", Side::opposition, {9, 1}}}},
                               seed);
        for (unsigned turns = 0; turns < 8 && !save_line(*c, "Ally"); ++turns)
            if (c->snapshot().actor != 98 || c->snapshot().reaction_pending)
                check(submit(*c, "decline") || submit(*c, "end"), "The party waits");
            else
                check(c->submit(choose_demo_command(*c)), "The adept acts");
        if (auto line = save_line(*c, "Ally"))
            return line;
    }
    return std::nullopt;
}

// Aura of Protection: allies within 10 feet add the Paladin's Charisma
// modifier (at least +1) to saves, named in the save's log line.
void aura_checks()
{
    const auto six = paladin(6);
    const int charisma = std::max(1, (six.sheet().scores[Ability::charisma] - 10) / 2);
    const auto below = ally_save(paladin(5), {3, 1}), near = ally_save(six, {3, 1}),
               edge = ally_save(six, {2, 0}), far = ally_save(six, {0, 1});
    check(below && near && edge && far, "The adept blinds the ally");
    check(bonus_of(*near) == bonus_of(*below) + charisma &&
              bonus_of(*edge) == bonus_of(*below) + charisma,
          "Allies within 10 feet add the Paladin's Charisma modifier");
    check(near->find(" (Aura of Protection +" + std::to_string(charisma) + ") vs DC ") !=
                  std::string::npos &&
              below->find("Aura of Protection") == std::string::npos,
          "The save line names the aura's share");
    check(bonus_of(*far) == bonus_of(*below), "An ally 20 feet away does not");
}

// A dying ally beside the Paladin adds the aura to its death saves.
bool aura_death_save(const Character &hero)
{
    const auto rules = dummy_rules();
    auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                            {{1,
                              "campaign-character",
                              "Paladin",
                              Side::party,
                              {1, 1},
                              rules->character_profile(hero.sheet(), {}).data},
                             {2,
                              "campaign-character",
                              "Ally",
                              Side::party,
                              {2, 1},
                              rules->character_profile(paladin(4).sheet(), {}).data,
                              VitalState{0, false, {}, {}}},
                             {98, "dummy", "Dummy", Side::opposition, {7, 1}}}},
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
              offered(*c, "cure_wounds_3"),
          "Cure Wounds is offered from each slot level");
    check(offered(*c, "protection_from_evil_and_good") &&
              !offered(*c, "protection_from_evil_and_good_2") &&
              !offered(*c, "protection_from_evil_and_good_3"),
          "A spell a higher slot does not change is offered at its own level");
    check(submit(*c, "cure_wounds_3", 1) && logged(*c, "Paladin recovers") && slots(*c, 3) == 1 &&
              slots(*c, 1) == 4,
          "Cure Wounds from a level-3 slot spends it");

    auto smite = duel(*rules, paladin(9), 1);
    check(submit(*smite, "melee", 98) && offered(*smite, "divine_smite_3") &&
              offered(*smite, "shining_smite_3") && submit(*smite, "divine_smite_3", 98) &&
              slots(*smite, 3) == 1,
          "Divine Smite from a level-3 slot");
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
              has_condition(*c, 1, "Beacon of Hope"),
          "Beacon of Hope lights the Paladin");
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
              has_condition(*c, 98, "Shining (Shining Smite)"),
          "Shining Smite lights the dummy");
    next_turn(*c);
    check(submit(*c, "dispel_magic") && aim(*c, Cell{2, 1}) && submit(*c, "area_cast") &&
              logged(*c, "Dispel Magic ends a spell on Dummy.") &&
              !has_condition(*c, 98, "Shining (Shining Smite)") &&
              logged(*c, "Paladin loses Concentration."),
          "Dispel Magic ends Shining Smite");
}

// Radiant Strikes: melee hits deal 1d8 Radiant more; Channel Divinity has three uses.
void radiant_checks()
{
    const auto rules = dummy_rules();
    auto ten = duel(*rules, paladin(10), 1);
    check(submit(*ten, "melee", 98) && !logged(*ten, "Radiant Strikes"), "None at level 10");
    auto c = duel(*rules, paladin(11), 1);
    check(submit(*c, "melee", 98) && logged(*c, "Dummy takes ") &&
              logged(*c, " Radiant damage from Radiant Strikes."),
          "A melee hit adds Radiant Strikes");
    const auto pools = unit(*c, 1).resources;
    check(std::any_of(pools.begin(), pools.end(),
                      [](const auto &pool)
                      {
                          return pool.id == "channel_divinity" && pool.capacity == 3;
                      }),
          "Three Channel Divinity uses from level 11");
}

// Death Ward: the first drop to 0 Hit Points leaves 1.
void death_ward_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(13), {"death_ward"});
    auto c = alone_with(*rules, sheet, "brute", 1, VitalState{1, false, {}, {}});
    check(submit(*c, "death_ward", 1) && has_condition(*c, 1, "Death Ward"),
          "Death Ward on the Paladin");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps Death Ward");
    for (unsigned turns = 0; turns < 6 && !logged(*c, "(Death Ward)"); ++turns)
        if (c->snapshot().actor == 1 || c->snapshot().reaction_pending)
            check(submit(*c, "decline") || submit(*c, "end"), "The Paladin waits");
        else
            check(c->submit(choose_demo_command(*c)), "The brute attacks");
    check(logged(*c, "Paladin drops to 1 Hit Point instead (Death Ward).") &&
              unit(*c, 1).hit_points == 1 && !has_condition(*c, 1, "Death Ward"),
          "The brute's hit leaves 1 Hit Point and ends the ward");
}

// Aura of Life: an ally at 0 Hit Points in it regains 1 as its turn starts.
void aura_of_life_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(13), {"aura_of_life"});
    auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                            {{1,
                              "campaign-character",
                              "Paladin",
                              Side::party,
                              {1, 1},
                              rules->character_profile(sheet, {}).data},
                             {2,
                              "campaign-character",
                              "Ally",
                              Side::party,
                              {2, 1},
                              rules->character_profile(paladin(4).sheet(), {}).data,
                              VitalState{0, false, {}, {}}},
                             {98, "dummy", "Dummy", Side::opposition, {7, 1}}}},
                           1);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    check(submit(*c, "aura_of_life", 1) && logged(*c, "Paladin radiates an Aura of Life."),
          "The Paladin casts Aura of Life");
    for (unsigned turns = 0; turns < 6 && !logged(*c, "Ally stirs in the Aura of Life."); ++turns)
        check(submit(*c, "end"), "Turns pass");
    check(logged(*c, "Ally stirs in the Aura of Life.") && unit(*c, 2).hit_points >= 1,
          "The fallen ally regains 1 Hit Point");
}

// Freedom of Movement: Difficult Terrain costs nothing extra.
void freedom_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = paladin(13).sheet();
    auto c = rules->create({{10, 3, std::vector<Terrain>(30, Terrain::difficult)},
                            {{1,
                              "campaign-character",
                              "Paladin",
                              Side::party,
                              {0, 1},
                              rules->character_profile(sheet, {}).data},
                             {98, "dummy", "Dummy", Side::opposition, {9, 0}}}},
                           1);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    const auto farthest = [&]
    {
        int best = 0;
        for (const auto cell : c->movement_reach(1))
            best = std::max(best, cell.x);
        return best;
    };
    check(farthest() == 3, "Difficult Terrain halves the Paladin's 30 feet");
    check(submit(*c, "freedom_of_movement", 1) && has_condition(*c, 1, "Freedom of Movement"),
          "Freedom of Movement on the Paladin");
    check(farthest() == 6, "Difficult Terrain costs nothing extra");
}

// Guardian of Faith: an enemy that starts its turn within 10 feet saves.
void guardian_checks()
{
    const auto rules = dummy_rules();
    auto c = alone_with(*rules, paladin(13).sheet(), "dummy", 1);
    check(submit(*c, "guardian_of_faith") && aim(*c, Cell{3, 1}) && submit(*c, "area_cast") &&
              logged(*c, "A Guardian of Faith appears."),
          "The guardian appears beside the foe");
    check(c->snapshot().guardians == std::vector<Cell>{Cell{3, 1}}, "The snapshot shows it");
    check(submit(*c, "end") && logged(*c, "Foe Dexterity save") &&
              logged(*c, "Radiant damage from the Guardian of Faith."),
          "The foe's turn starts within 10 feet: a Dexterity save and Radiant damage");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the guardian");
}

// Banishment: off the field while the Paladin concentrates; a Fey never comes
// back after a full minute.
void banishment_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(13), {"banishment"});
    std::unique_ptr<CombatSession> c;
    for (std::uint64_t seed = 1; seed < 40 && !(c && unit(*c, 98).banished); ++seed)
    {
        c = alone_with(*rules, sheet, "dummy", seed);
        check(submit(*c, "banishment", 98), "The Paladin casts Banishment");
    }
    check(unit(*c, 98).banished && has_condition(*c, 98, "Banished") &&
              logged(*c, "Foe is banished."),
          "A failed Charisma save banishes the foe");
    const auto commands = c->legal_commands();
    check(std::none_of(commands.begin(), commands.end(),
                       [](const auto &command)
                       {
                           return command.target == 98;
                       }),
          "Nothing can be aimed at the banished foe");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the banished foe");
    check(submit(*c, "end") && c->snapshot().actor == 1, "The banished foe's turn passes");
    check(submit(*c, "shield_of_faith", 1) && logged(*c, "Foe returns from its banishment.") &&
              !unit(*c, 98).banished,
          "A new Concentration spell ends Banishment: the foe returns");

    std::unique_ptr<CombatSession> fey;
    for (std::uint64_t seed = 1; seed < 40 && !(fey && unit(*fey, 98).banished); ++seed)
    {
        fey = alone_with(*rules, sheet, "fey-dummy", seed);
        check(submit(*fey, "banishment", 98), "The Paladin banishes the Fey");
    }
    for (unsigned rounds = 0; rounds < 12 && !unit(*fey, 98).dead; ++rounds)
        check(submit(*fey, "end"), "A round passes");
    check(logged(*fey, "Foe does not return from its banishment.") && unit(*fey, 98).dead,
          "After a full minute the Fey is defeated");
}

// Restoring Touch: Lay On Hands ends Blinded for 5 points.
void restoring_touch_checks()
{
    const auto rules = module();
    const auto profile = rules->character_profile(paladin(14).sheet(), {}).data;
    for (std::uint64_t seed = 1; seed < 300; ++seed)
    {
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                                {{1, "campaign-character", "Paladin", Side::party, {1, 1}, profile},
                                 {98, "blindness-adept", "Adept", Side::opposition, {6, 1}}}},
                               seed);
        for (unsigned turns = 0;
             turns < 8 && !(c->snapshot().actor == 1 && has_condition(*c, 1, "Blinded")); ++turns)
            if (c->snapshot().actor == 1 || c->snapshot().reaction_pending)
                check(submit(*c, "decline") || submit(*c, "end"), "The Paladin waits");
            else
                check(c->submit(choose_demo_command(*c)), "The adept acts");
        if (c->snapshot().actor != 1 || !has_condition(*c, 1, "Blinded"))
            continue;
        check(submit(*c, "lay_on_hands_restore", 1) &&
                  logged(*c, "Paladin is no longer Blinded.") && !has_condition(*c, 1, "Blinded"),
              "Restoring Touch ends Blinded");
        return;
    }
    throw std::runtime_error("No seed's Blindness lands on the Paladin");
}

// Smite of Protection: Divine Smite gives the aura +2 AC until the next turn.
void smite_of_protection_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, paladin(15), 1);
    const int before = unit(*c, 1).armor_class;
    check(submit(*c, "melee", 98) && submit(*c, "divine_smite", 98) &&
              logged(*c, "Paladin's aura shields its allies (Smite of Protection).") &&
              unit(*c, 1).armor_class == before + 2,
          "Divine Smite raises the aura's AC by 2");
    next_turn(*c);
    check(unit(*c, 1).armor_class == before, "It lasts until the Paladin's next turn");
}

// A level-1 Warlock with Hex prepared.
Character hexer()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "warlock";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Warlock";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.training = {{"class:warlock", {"investigation", "deception"}}};
    d.cantrips = {"eldritch_blast", "chill_touch"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"hex", "hellish_rebuke"}, {}, {}};
    return Character(*srd5::character_rules(), d, {});
}

// Remove Curse: offered on a cursed creature only; it ends Hex.
void remove_curse_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(9), {"remove_curse"});
    auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                            {{1,
                              "campaign-character",
                              "Paladin",
                              Side::party,
                              {1, 1},
                              rules->character_profile(sheet, {}).data},
                             {2,
                              "campaign-character",
                              "Warlock",
                              Side::party,
                              {1, 2},
                              rules->character_profile(hexer().sheet(), {}).data},
                             {98, "dummy", "Dummy", Side::opposition, {2, 1}}}},
                           1);
    for (unsigned turns = 0; c->snapshot().actor != 2 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Warlock's turn");
    check(!offered(*c, "remove_curse"), "Nothing to uncurse yet");
    check(submit(*c, "hex", 98) && logged(*c, "Dummy is hexed."), "The Warlock hexes the dummy");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    check(submit(*c, "remove_curse", 98) && logged(*c, "Dummy's curses end.") &&
              !has_condition(*c, 98, "Hexed"),
          "Remove Curse ends Hex");
}

// A dying character's vital state with two failed death saves.
VitalState two_failures()
{
    return {0, false, "SRD13 0 0 0 0 0 0 0 0 0 0 0 2 0 0 0 0 0 \"\" 0 0 0 0 0 0 0 FX8 1 0 0", {}};
}

// Revivify: an adjacent ally dead no more than a minute, for 300 gp times its
// level from the party purse, which the campaign pays.
void revivify_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto hero = paladin(9);
    const auto paladin_id = party.add_pc(hero);
    const auto prepared = rules->character_profile(preparing(hero, {"revivify"}), {}).data;
    const auto ally_id = party.add_pc(paladin(4));
    Purse purse{};
    coins(purse, Coin::gold) = 2000;
    party.set_wealth(purse);
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        auto participants = party.participants();
        for (auto &p : participants)
        {
            // Two squares apart: Revivify reaches 10 feet.
            p.cell = p.id == paladin_id ? Cell{1, 1} : Cell{3, 1};
            if (p.id == paladin_id)
                p.character_profile = prepared;
            if (p.id == ally_id)
                p.state = two_failures();
        }
        participants.push_back({98, "vanguard", "Foe", Side::opposition, {7, 1}});
        party.begin_combat();
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                                participants,
                                party.state().next_combat_scope,
                                100,
                                coins(party.state().wealth, Coin::gold)},
                               seed);
        party.apply_combat(c->snapshot());
        for (unsigned turns = 0;
             turns < 6 && !(unit(*c, ally_id).dead && c->snapshot().actor == paladin_id); ++turns)
            check(submit(*c, "end"), "Turns pass");
        if (!unit(*c, ally_id).dead || c->snapshot().actor != paladin_id)
        {
            party.end_combat();
            continue;
        }
        check(offered(*c, "revivify") && has_condition(*c, ally_id, "Fallen"),
              "Revivify is offered on the ally who just died, 10 feet away");
        // In camp right after the fight, the same fallen ally may be raised.
        const auto camp_sheet = preparing(hero, {"revivify"});
        auto caster_state = unit(*c, paladin_id).persistent;
        auto fallen_state = unit(*c, ally_id).persistent;
        const auto camp = rules->camp_actions(camp_sheet, caster_state);
        RandomState random{1};
        check(std::any_of(camp.begin(), camp.end(),
                          [](const auto &action)
                          {
                              return action.id == "revivify" && !action.whole_party;
                          }) &&
                  rules->camp_action_gold(paladin(4).sheet(), "revivify") == 1200,
              "The camp's Cast / Use list offers Revivify for 1200 gp");
        rules->use_camp_action(camp_sheet, caster_state, paladin(4).sheet(), fallen_state,
                               "revivify", random);
        check(!fallen_state.dead && fallen_state.hit_points == 1,
              "Revivify in camp raises the ally");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the time of death");
        check(
            submit(*c, "revivify", ally_id) &&
                logged(
                    *c,
                    "Paladin casts Revivify: Paladin returns to life (an offering of 1200 gp).") &&
                unit(*c, ally_id).hit_points == 1 && c->snapshot().offerings_gold == 1200,
            "The ally returns with 1 Hit Point for 1200 gp");
        party.apply_combat(c->snapshot());
        check(coins(party.state().wealth, Coin::gold) == 800, "The party purse pays the offering");
        party.apply_combat(c->snapshot());
        check(coins(party.state().wealth, Coin::gold) == 800, "Only once");
        party.end_combat();
        return;
    }
    throw std::runtime_error("No seed's death save kills the ally");
}

// Revivify is not offered without the purse for it, nor on the long dead.
void revivify_limit_checks()
{
    const auto rules = module();
    const auto sheet = preparing(paladin(9), {"revivify"});
    const auto start = [&](unsigned gold, VitalState ally)
    {
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                                {{1,
                                  "campaign-character",
                                  "Paladin",
                                  Side::party,
                                  {1, 1},
                                  rules->character_profile(sheet, {}).data},
                                 {2,
                                  "campaign-character",
                                  "Ally",
                                  Side::party,
                                  {2, 1},
                                  rules->character_profile(paladin(4).sheet(), {}).data,
                                  ally},
                                 {98, "vanguard", "Foe", Side::opposition, {7, 1}}},
                                1,
                                100,
                                gold},
                               1);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
            check(submit(*c, "end"), "Reach the Paladin's turn");
        return c;
    };
    check(!offered(*start(5000, VitalState{0, true, {}, {}}), "revivify"),
          "Dead before the fight is dead too long");
}

// Magic Circle in a fight: a minute of casting, then a Fey cannot enter it.
void magic_circle_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(9), {"magic_circle"});
    const auto start = [&]
    {
        const std::vector<std::string> gear{"longsword"};
        auto c = rules->create({{10, 3, std::vector<Terrain>(30)},
                                {{1,
                                  "campaign-character",
                                  "Paladin",
                                  Side::party,
                                  {1, 1},
                                  rules->character_profile(sheet, gear).data},
                                 {98, "fey-dummy", "Fey", Side::opposition, {8, 1}}}},
                               1);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
            check(submit(*c, "end"), "Reach the Paladin's turn");
        return c;
    };
    auto c = start();
    check(offered(*c, "magic_circle_fey") && offered(*c, "magic_circle_undead"),
          "One Magic Circle entry per creature type");
    check(submit(*c, "magic_circle_fey") && aim(*c, Cell{1, 1}) && submit(*c, "area_cast") &&
              logged(*c, "Paladin begins a Magic Circle against Fey.") && slots(*c, 3) == 1,
          "The Paladin begins the circle on its own square, spending a level-3 slot");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the circle being cast");
    next_turn(*c);
    check(logged(*c, "Paladin keeps casting Magic Circle.") && !unit(*c, 1).action,
          "Casting takes the next turn's Action");
    for (unsigned turns = 0; turns < 30 && !logged(*c, "The Magic Circle is complete."); ++turns)
        check(submit(*c, "end"), "A turn passes");
    check(logged(*c, "The Magic Circle is complete."), "After a minute the circle stands");
    const auto reach = c->movement_reach(98);
    check(std::none_of(reach.begin(), reach.end(),
                       [](Cell cell)
                       {
                           return std::max(std::abs(cell.x - 1), std::abs(cell.y - 1)) <= 2;
                       }),
          "The Fey cannot enter the circle");

    auto broken = start();
    check(submit(*broken, "magic_circle_fey") && aim(*broken, Cell{1, 1}) &&
              submit(*broken, "area_cast"),
          "The Paladin begins the circle");
    next_turn(*broken);
    check(submit(*broken, "shield_of_faith", 1) &&
              logged(*broken, "The Magic Circle fails; the spell slot is kept.") &&
              slots(*broken, 3) == 2,
          "Another Concentration spell breaks the casting; the slot comes back");
}

// Magic Circle in camp: a ward that keeps its creature type from interrupting the rest.
void camp_circle_checks()
{
    const auto rules = dummy_rules();
    const auto sheet = preparing(paladin(9), {"magic_circle"});
    VitalState state{sheet.hit_points, false, {}, {}};
    const auto actions = rules->camp_actions(sheet, state);
    check(std::any_of(actions.begin(), actions.end(),
                      [](const auto &action)
                      {
                          return action.id == "magic_circle_fey" && action.whole_party;
                      }),
          "The Cast / Use list offers a Magic Circle against Fey");
    std::vector<CampTarget> party{{sheet, state}};
    RandomState random{1};
    const std::vector<std::string> fey{"fey-dummy"}, mixed{"fey-dummy", "dummy"};
    check(!rules->keeps_away(party, fey), "No ward before the circle");
    rules->use_party_camp_action(sheet, state, party, "magic_circle_fey", random);
    check(rules->keeps_away(party, fey) && !rules->keeps_away(party, mixed),
          "The ward keeps Fey away, not a band with others in it");
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
        death_ward_checks();
        aura_of_life_checks();
        freedom_checks();
        guardian_checks();
        banishment_checks();
        restoring_touch_checks();
        smite_of_protection_checks();
        remove_curse_checks();
        revivify_checks();
        revivify_limit_checks();
        magic_circle_checks();
        camp_circle_checks();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Paladin level checks passed\n";
}
