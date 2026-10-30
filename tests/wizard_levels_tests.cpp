#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
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
#include <tuple>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// A Wizard (Evoker) from level 5 to 10 (LEVELS-1, LEVELS-4): level-3 to
// level-5 spells, summoned creatures, Memorize Spell, Arcane Recovery's larger
// choices, Sculpt Spells at 6 and Empowered Evocation at 10.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// An AC 1 dummy, a brute that always hits, and a Wizard foe's book.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature slayer 1 1000 0 30 30 10 20 30 0 0 0 0 0 0 0 0 0 1 0\n");
}

CharacterDraft draft(std::string name = "Wizard")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.rolls[3] = {{6, 6, 6, 1}, 3};
    d.cantrips = std::vector<std::string>{"fire_bolt", "ray_of_frost", "chill_touch"};
    const std::vector<std::string> book{"magic_missile", "burning_hands", "shield",
                                        "mage_armor",    "false_life",    "thunderwave"};
    d.spells =
        SpellChoices{{{"spellbook:1", book}}, std::vector<std::string>{"magic_missile"}, {}, {}};
    d.training = {{"class:wizard", {"medicine", "nature"}}};
    return d;
}

// A Wizard of the level, advanced through the rules' default level-ups.
Character wizard(unsigned level, std::string name = "Wizard")
{
    Character hero(*srd5::character_rules(), draft(std::move(name)), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        check(hero.advance(*rules, life, rules->default_advancement(hero.sheet())),
              "An ordinary level-up");
    }
    return hero;
}

// The Wizard's sheet with `spells` written into its book in place of what it
// learned at level 5 or later, and prepared in place of its first ones.
CharacterSheet with_spells(const Character &hero, const std::vector<std::string> &spells)
{
    auto sheet = hero.sheet();
    std::size_t next = 0;
    for (const auto &spell : spells)
    {
        const auto known = std::find_if(sheet.grants.begin(), sheet.grants.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == "spell:" + spell;
                                        });
        if (known == sheet.grants.end())
        {
            // The latest spell learned, whose level allows any new spell.
            auto learned = sheet.grants.end();
            for (auto g = sheet.grants.begin(); g != sheet.grants.end(); ++g)
            {
                if (g->id.starts_with("spell:") && g->level >= 5 &&
                    g->source_id == "class:wizard:spellcasting" && g->choices.contains("access") &&
                    g->choices.at("access") == "spellbook" &&
                    std::find(spells.begin(), spells.end(), g->id.substr(6)) == spells.end() &&
                    (learned == sheet.grants.end() || g->level > learned->level))
                {
                    learned = g;
                }
            }
            check(learned != sheet.grants.end(), "A level-5 spell to replace");
            const auto old = learned->id.substr(6);
            learned->id = "spell:" + spell;
            std::erase(sheet.prepared_spells, old);
        }
        auto &prepared = sheet.prepared_spells;
        if (std::find(prepared.begin(), prepared.end(), spell) == prepared.end())
        {
            if (next < prepared.size())
            {
                prepared[next] = spell;
            }
            else
            {
                prepared.push_back(spell);
            }
        }
        ++next;
    }
    return sheet;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && (!target || command.target == target);
                       });
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

bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && command.destination == cell)
        {
            return c.submit(command);
        }
    }
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

// Spell slots of a level left: from the third field of the saved vital state.
unsigned slots(const CombatSession &c, unsigned level, EntityId id = 1)
{
    std::istringstream in(unit(c, id).persistent.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
    {
        in >> count;
    }
    return count;
}

// A member's spell slots of a level left, read from its saved vital state.
unsigned slots_left(const CampaignParty &party, MemberId id, unsigned level)
{
    std::istringstream in(party.member(id).vitals.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
    {
        in >> count;
    }
    return count;
}

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
    {
        check(submit(c, "end") || submit(c, "decline"), "End a turn");
    }
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Wizard (1) at (1, 2) and its creatures, on the Wizard's turn.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet,
                                     std::vector<Participant> others, std::uint64_t seed)
{
    const std::vector<std::string> gear{"quarterstaff"};
    others.insert(others.begin(), {1,
                                   "campaign-character",
                                   "Wizard",
                                   Side::party,
                                   {1, 2},
                                   rules.character_profile(sheet, gear).data});
    auto c = rules.create({{14, 7, std::vector<Terrain>(98)}, std::move(others)}, seed);
    to_turn(*c, 1);
    return c;
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
    {
        party.advance(id, party.default_advancement(id));
    }
    check(party.member(id).character.sheet().level == 15, "A Wizard reaches 15");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>{{5, 9},
                                                                                 {6, 10},
                                                                                 {7, 11},
                                                                                 {8, 12},
                                                                                 {9, 14},
                                                                                 {10, 15},
                                                                                 {11, 16},
                                                                                 {12, 16},
                                                                                 {13, 17},
                                                                                 {14, 18},
                                                                                 {15, 19}})
    {
        check(rules->spell_access(wizard(level).sheet()).prepared_choices == count,
              "Prepared spells: 9 at 5 to 19 at 15");
    }
    check(rules->spell_access(wizard(10).sheet()).cantrip_choices == 5, "A fifth cantrip at 10");
    const auto grants = wizard(10).sheet().grants;
    check(std::any_of(grants.begin(), grants.end(),
                      [](const auto &g)
                      {
                          return g.id == "feature:empowered_evocation" && g.level == 10;
                      }),
          "Empowered Evocation at 10");
    const auto later = wizard(14).sheet().grants;
    check(std::any_of(later.begin(), later.end(),
                      [](const auto &g)
                      {
                          return g.id == "feature:overchannel" && g.level == 14;
                      }),
          "Overchannel at 14");
    auto four = wizard(4).sheet();
    four.level = 5;
    const auto learning =
        rules->spell_choice_options(four, SpellChoiceContext::advancement).learning;
    const auto book = std::find_if(learning.begin(), learning.end(),
                                   [](const auto &group)
                                   {
                                       return group.id == "spellbook:5";
                                   });
    check(book != learning.end() && book->count == 2 &&
              std::any_of(book->options.begin(), book->options.end(),
                          [](const auto &o)
                          {
                              return o.id == "fireball";
                          }),
          "Level 5 learns two spells, level-3 ones among them");
    const auto dummies = dummy_rules();
    auto c5 =
        field(*dummies, wizard(5).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c6 =
        field(*dummies, wizard(6).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c5, 3) == 2 && slots(*c6, 3) == 3, "Two level-3 slots at 5, three at 6");
    auto c7 =
        field(*dummies, wizard(7).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c8 =
        field(*dummies, wizard(8).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c7, 4) == 1 && slots(*c8, 4) == 2, "One level-4 slot at 7, two at 8");
    auto c9 =
        field(*dummies, wizard(9).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c10 =
        field(*dummies, wizard(10).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c9, 5) == 1 && slots(*c10, 5) == 2, "One level-5 slot at 9, two at 10");
    auto c11 =
        field(*dummies, wizard(11).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c11, 6) == 1, "A level-6 slot at 11");
    auto c13 =
        field(*dummies, wizard(13).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c15 =
        field(*dummies, wizard(15).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c13, 7) == 1 && slots(*c13, 8) == 0 && slots(*c15, 8) == 1,
          "A level-7 slot at 13, a level-8 slot at 15");
    const auto bytes = encode_campaign(party, nullptr, "wizard-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "wizard-levels", nullptr)
                     .party);
    check(encode_campaign(next, nullptr, "wizard-levels") == bytes,
          "A level-15 Wizard saves and reloads");
}

void memorize_spell_checks()
{
    const auto rules = module();
    check(
        !rules->spell_choice_options(wizard(4).sheet(), SpellChoiceContext::short_rest).may_prepare,
        "No Memorize Spell before 5");
    const auto sheet = wizard(5).sheet();
    const auto options = rules->spell_choice_options(sheet, SpellChoiceContext::short_rest);
    check(options.may_prepare && !options.may_replace, "Memorize Spell prepares, never replaces");
    std::vector<std::string> unprepared;
    for (const auto &spell : rules->spell_access(sheet).spellbook)
    {
        if (std::find(sheet.prepared_spells.begin(), sheet.prepared_spells.end(), spell.id) ==
            sheet.prepared_spells.end())
        {
            unprepared.push_back(spell.id);
        }
    }
    check(unprepared.size() >= 2, "Two unprepared spells in the book");
    auto one = sheet.prepared_spells, two = sheet.prepared_spells;
    one[0] = unprepared[0];
    two[0] = unprepared[0];
    two[1] = unprepared[1];
    auto swapped = sheet;
    rules->apply_spell_choices(swapped, SpellChoices{{}, one, {}, {}},
                               SpellChoiceContext::short_rest, ChoiceCompleteness::complete);
    check(swapped.prepared_spells == one, "One prepared spell changes on a Short Rest");
    bool refused = false;
    try
    {
        auto copy = sheet;
        rules->apply_spell_choices(copy, SpellChoices{{}, two, {}, {}},
                                   SpellChoiceContext::short_rest, ChoiceCompleteness::complete);
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused, "Two changes on a Short Rest are refused");
    // The campaign offers the change once the Short Rest's spending is done.
    CampaignParty party(module());
    const auto id = party.add_pc(wizard(5));
    (void)party.rest(RestKind::short_rest);
    check(party.state().short_rest.has_value(), "A Short Rest");
    party.finish_short_rest(party.state().short_rest->ticket);
    check(party.state().spell_rest && party.state().spells_after_short_rest &&
              party.rest_spell_context() == SpellChoiceContext::short_rest,
          "After a Short Rest the Wizard may change a prepared spell");
    party.choose_spells(id, SpellChoices{{}, one, {}, {}});
    check(!party.state().spell_rest && party.member(id).character.sheet().prepared_spells == one,
          "Memorize Spell changes one prepared spell");
    const auto bytes = encode_campaign(party, nullptr, "memorize");
    CampaignParty next(module());
    next.restore(
        decode_campaign(bytes, *srd5::character_rules(), party.rule_module(), "memorize", nullptr)
            .party);
    check(encode_campaign(next, nullptr, "memorize") == bytes,
          "The changed spell saves and reloads");
}

void arcane_recovery_checks()
{
    const auto rules = module();
    const auto sheet = wizard(5).sheet();
    // Both level-3 slots spent.
    const auto dummies = dummy_rules();
    auto c = field(*dummies, with_spells(wizard(5), {"fireball"}),
                   {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast") && slots(*c, 3) == 1,
          "A level-3 slot is spent");
    const auto state = unit(*c, 1).persistent;
    const auto info = rules->recovery_info(sheet, state);
    check(std::any_of(info.choices.begin(), info.choices.end(),
                      [](const auto &choice)
                      {
                          return choice.id == "arcane_recovery:0:0:1";
                      }),
          "Arcane Recovery at 5 can restore a level-3 slot");
}

void fireball_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(5), {"fireball", "lightning_bolt"}),
                   {{97, "dummy", "Near", Side::opposition, {6, 2}},
                    {98, "dummy", "Far", Side::opposition, {10, 2}}},
                   1);
    check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast") && logged(*c, "Near takes ") && logged(*c, "Far takes ") &&
              logged(*c, " Fire damage."),
          "Fireball's 20-foot Sphere burns both foes");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "lightning_bolt") && submit_cell(*c, "area_move", Cell{10, 2}) &&
              submit(*c, "area_cast") && logged(*c, " Lightning damage."),
          "Lightning Bolt's Line strikes along the row");
}

void counterspell_checks()
{
    const auto rules = dummy_rules();
    bool stopped = false;
    for (std::uint64_t seed = 1; seed < 40 && !stopped; ++seed)
    {
        const std::vector<std::string> gear{"quarterstaff"};
        const auto mine = with_spells(wizard(5), {"counterspell"});
        const auto theirs = wizard(5, "Rival").sheet();
        auto c = rules->create({{14, 7, std::vector<Terrain>(98)},
                                {{1,
                                  "campaign-character",
                                  "Wizard",
                                  Side::party,
                                  {1, 2},
                                  rules->character_profile(mine, gear).data},
                                 {98,
                                  "campaign-character",
                                  "Rival",
                                  Side::opposition,
                                  {6, 2},
                                  rules->character_profile(theirs, gear).data}}},
                               seed);
        to_turn(*c, 98);
        check(submit(*c, "fire_bolt", 1), "The rival casts Fire Bolt");
        if (!c->snapshot().reaction_pending)
        {
            continue;
        }
        check(offered(*c, "counterspell") && submit(*c, "counterspell"),
              "The Wizard is asked about Counterspell");
        check(logged(*c, "Wizard casts Counterspell.") && slots(*c, 3) == 1 &&
                  logged(*c, "Rival Constitution save:"),
              "Counterspell spends a level-3 slot");
        stopped = logged(*c, "Rival's spell dissipates.");
        if (stopped)
        {
            check(!logged(*c, "Rival -> Wizard") && !logged(*c, "Wizard takes "),
                  "A stopped spell does nothing");
        }
    }
    check(stopped, "A seed where the rival fails its save");
}

void haste_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(5), {"haste"}),
                   {{98, "dummy", "Foe", Side::opposition, {12, 2}}}, 1);
    const auto ac = unit(*c, 1).armor_class;
    check(submit(*c, "haste", 1) && logged(*c, "Wizard gains Haste.") &&
              unit(*c, 1).armor_class == ac + 2,
          "Haste: +2 AC");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps Haste");
    to_turn(*c, 98);
    to_turn(*c, 1);
    check(unit(*c, 1).movement_feet == 60, "Haste doubles the Speed");
    check(submit(*c, "dash") && offered(*c, "dash") && offered(*c, "fire_bolt", 98),
          "Haste's action Dashes and keeps the Action");
    check(submit(*c, "dash") && !offered(*c, "dash") && !offered(*c, "fire_bolt", 98),
          "The second Dash spends the Action");
    for (unsigned turns = 0; turns < 40 && !logged(*c, "A wave of lethargy washes over Wizard.");
         ++turns)
    {
        check(submit(*c, "end"), "Pass the minute");
    }
    check(logged(*c, "A wave of lethargy washes over Wizard."), "Haste's end brings lethargy");
}

void hypnotic_pattern_checks()
{
    const auto rules = dummy_rules();
    bool entranced = false;
    for (std::uint64_t seed = 1; seed < 40 && !entranced; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"hypnotic_pattern"}),
                       {{2, "dummy", "Ally", Side::party, {7, 1}},
                        {98, "dummy", "Foe", Side::opposition, {8, 2}}},
                       seed);
        check(submit(*c, "hypnotic_pattern") && submit_cell(*c, "area_move", Cell{10, 2}) &&
                  submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"),
              "The pattern appears");
        if (!logged(*c, "Foe is entranced by the pattern."))
        {
            continue;
        }
        entranced = true;
        check(unit(*c, 98).status.find("Charmed") != std::string::npos, "Entranced: Charmed");
        to_turn(*c, 98);
        check(!offered(*c, "move") && !offered(*c, "melee"), "An entranced creature does nothing");
        to_turn(*c, 2);
        check(submit(*c, "shake_awake", 98) && logged(*c, "Ally shakes Foe awake.") &&
                  unit(*c, 98).status.find("Charmed") == std::string::npos,
              "An action shakes it awake");
    }
    check(entranced, "A seed where the foe fails its save");
}

void fear_checks()
{
    const auto rules = dummy_rules();
    bool frightened = false;
    for (std::uint64_t seed = 1; seed < 40 && !frightened; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"fear"}),
                       {{98, "dummy", "Foe", Side::opposition, {3, 2}}}, seed);
        check(submit(*c, "fear") && submit_cell(*c, "area_move", Cell{3, 2}) &&
                  submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"),
              "Fear's cone");
        if (!logged(*c, "Foe is Frightened."))
        {
            continue;
        }
        frightened = true;
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(commands.size() == 1 &&
                  (commands.front().verb == "move" || commands.front().verb == "dash"),
              "A frightened creature only flees");
    }
    check(frightened, "A seed where the foe fails its save");
}

void cloud_checks()
{
    const auto rules = dummy_rules();
    auto sleet = field(*rules, with_spells(wizard(5), {"sleet_storm"}),
                       {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*sleet, "sleet_storm") && submit_cell(*sleet, "area_move", Cell{9, 2}) &&
              submit(*sleet, "area_cast") &&
              sleet->snapshot().battlefield.at({9, 3}) == Terrain::difficult &&
              !sleet->snapshot().obscured.empty(),
          "Sleet Storm: obscured, Difficult Terrain");
    to_turn(*sleet, 98);
    check(logged(*sleet, "Foe Dexterity save:"), "A turn starting in the sleet calls for a save");
    const auto saved = sleet->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the sleet");
    bool retched = false;
    for (std::uint64_t seed = 1; seed < 40 && !retched; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"stinking_cloud"}),
                       {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, seed);
        check(submit(*c, "stinking_cloud") && submit_cell(*c, "area_move", Cell{9, 2}) &&
                  submit(*c, "area_cast"),
              "Stinking Cloud");
        to_turn(*c, 98);
        retched = logged(*c, "Foe retches in the cloud and can take no action this turn.");
        if (retched)
        {
            check(!unit(*c, 98).action && !unit(*c, 98).bonus_action, "No action or Bonus Action");
        }
    }
    check(retched, "A seed where the foe fails its save");
}

// Species save traits against an enemy's spell, with the same rolls as a
// Human ally's, so Advantage keeps the first die or a higher one: Dwarven
// Resilience against Stinking Cloud's Poisoned, Fey Ancestry against Hypnotic
// Pattern's Charmed and Brave against Fear's Frightened.
void species_save_checks()
{
    const auto rules = dummy_rules();
    const auto ally_save = [&](std::string_view spell, Cell aim, std::string_view ability,
                               std::string race, std::uint64_t seed)
    {
        auto d = draft("Ally");
        d.race = std::move(race);
        const Character ally(*srd5::character_rules(), d, {});
        const std::vector<std::string> gear{"quarterstaff"};
        auto c = field(*rules, with_spells(wizard(5), {std::string(spell)}),
                       {{2, "campaign-character", "Ally", Side::party, aim,
                         rules->character_profile(ally.sheet(), gear).data},
                        {98, "dummy", "Foe", Side::opposition, {13, 6}}},
                       seed);
        check(submit(*c, spell) && submit_cell(*c, "area_move", aim) && submit(*c, "area_cast"),
              "A spell over an ally");
        to_turn(*c, 2);
        const auto prefix = "Ally " + std::string(ability) + " save: d20 ";
        for (const auto &line : c->snapshot().log())
        {
            if (const auto at = line.find(prefix); at != std::string::npos)
            {
                return std::stoi(line.substr(at + prefix.size()));
            }
        }
        throw std::runtime_error("No save in the spell");
    };
    for (const auto &[spell, aim, ability, race] :
         std::vector<std::tuple<std::string, Cell, std::string, std::string>>{
             {"stinking_cloud", Cell{9, 2}, "Constitution", "dwarf"},
             {"hypnotic_pattern", Cell{9, 2}, "Wisdom", "elf"},
             {"fear", Cell{4, 2}, "Wisdom", "halfling"}})
    {
        bool higher = false;
        for (std::uint64_t seed = 1; seed <= 24; ++seed)
        {
            const int plain = ally_save(spell, aim, ability, "human", seed),
                      helped = ally_save(spell, aim, ability, race, seed);
            check(helped >= plain, "A species save trait never lowers the save");
            // A Halfling's Luck rerolls only a 1, so a higher die over another
            // first roll is Advantage's.
            higher |= helped > plain && plain > 1;
        }
        check(higher, "A " + race + " ally has Advantage against " + spell);
    }

    // Trance: magic cannot put an Elf to sleep.
    for (const auto &[race, sleeps] : {std::pair{"elf", false}, std::pair{"human", true}})
    {
        auto d = draft("Sleeper");
        d.race = race;
        const Character sleeper(*srd5::character_rules(), d, {});
        auto c = field(*rules, with_spells(wizard(5), {"sleep"}),
                       {{2, "campaign-character", "Sleeper", Side::opposition, {6, 2},
                         rules->character_profile(sleeper.sheet(), {}).data}},
                       1);
        check(submit(*c, "sleep") && submit_cell(*c, "area_move", Cell{6, 2}) &&
                  submit(*c, "area_cast") && logged(*c, "Sleeper does not sleep.") != sleeps,
              "Sleep passes over an Elf and not a Human");
    }
}

void vampiric_touch_checks()
{
    const auto rules = dummy_rules();
    bool drained = false;
    for (std::uint64_t seed = 1; seed < 40 && !drained; ++seed)
    {
        const std::vector<std::string> gear{"quarterstaff"};
        const auto sheet = with_spells(wizard(5), {"vampiric_touch"});
        auto c = rules->create({{14, 7, std::vector<Terrain>(98)},
                                {{1,
                                  "campaign-character",
                                  "Wizard",
                                  Side::party,
                                  {1, 2},
                                  rules->character_profile(sheet, gear).data,
                                  VitalState{5, false, {}}},
                                 {98, "dummy", "Foe", Side::opposition, {2, 2}}}},
                               seed);
        to_turn(*c, 1);
        check(submit(*c, "vampiric_touch", 98), "Cast Vampiric Touch");
        drained = logged(*c, "Wizard siphons life.") && unit(*c, 1).hit_points > 5;
        to_turn(*c, 98);
        to_turn(*c, 1);
        check(offered(*c, "vampiric_touch_again", 98), "A Magic action touches again");
    }
    check(drained, "The touch heals the Wizard half the damage");
}

void blink_checks()
{
    const auto rules = dummy_rules();
    bool vanished = false;
    for (std::uint64_t seed = 1; seed < 40 && !vanished; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"blink"}),
                       {{98, "brute", "Foe", Side::opposition, {2, 2}}}, seed);
        check(submit(*c, "blink", 1) && submit(*c, "end"), "Cast Blink and end the turn");
        if (!logged(*c, "Wizard blinks into the Ethereal Plane"))
        {
            continue;
        }
        vanished = true;
        check(!offered(*c, "melee", 1), "The Wizard is out of reach");
        to_turn(*c, 1);
        check(logged(*c, "Wizard returns from the Ethereal Plane."), "It returns on its turn");
    }
    check(vanished, "A seed where the d6 rolls 4 or more");
}

void tiny_hut_checks()
{
    const auto rules = module();
    const auto caster = with_spells(wizard(5), {"tiny_hut"});
    VitalState caster_state{caster.hit_points, false, {}};
    const auto actions = rules->camp_actions(caster, caster_state);
    check(std::any_of(actions.begin(), actions.end(),
                      [](const auto &a)
                      {
                          return a.id == "tiny_hut" && a.whole_party;
                      }),
          "Tiny Hut is a camp action for the party");
    std::vector<CampTarget> party{{caster, caster_state}};
    RandomState random{1};
    const std::vector<std::string> wolves{"wolf"};
    check(!rules->keeps_away(party, wolves), "Without the hut, a rest can be interrupted");
    rules->use_party_camp_action(caster, caster_state, party, "tiny_hut", random);
    check(rules->keeps_away(party, wolves), "Tiny Hut keeps every creature out");
}

void tentacles_checks()
{
    const auto rules = dummy_rules();
    bool held = false;
    for (std::uint64_t seed = 1; seed < 40 && !held; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"black_tentacles"}),
                       {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, seed);
        check(submit(*c, "black_tentacles") && submit_cell(*c, "area_move", Cell{8, 2}) &&
                  submit(*c, "area_cast") && slots(*c, 4) == 0 &&
                  c->snapshot().battlefield.at({8, 3}) == Terrain::difficult &&
                  logged(*c, "Foe Strength save:"),
              "Black Tentacles rise");
        held = logged(*c, " Bludgeoning damage from the tentacles.") &&
               logged(*c, "Foe is Restrained.");
        if (!held)
        {
            continue;
        }
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the tentacles");
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(std::any_of(commands.begin(), commands.end(),
                          [](const auto &command)
                          {
                              return command.verb == "escape" &&
                                     command.label == "Escape the tentacles";
                          }),
              "A held creature may try to escape");
    }
    check(held, "A seed where the foe fails its save");
}

void blight_and_charm_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"blight", "charm_monster"}),
                   {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, 1);
    check(submit(*c, "blight", 98) && logged(*c, "Foe Constitution save:") &&
              logged(*c, " Necrotic damage"),
          "Blight withers the foe");
    bool charmed = false;
    for (std::uint64_t seed = 1; seed < 40 && !charmed; ++seed)
    {
        auto m = field(*rules, with_spells(wizard(7), {"charm_monster"}),
                       {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, seed);
        check(submit(*m, "charm_monster", 98) ||
                  (submit(*m, "charm_monster", 98) && submit(*m, "spell_cast")),
              "Cast Charm Monster");
        if (m->snapshot().spell_targeting)
        {
            check(submit(*m, "spell_cast"), "Charm the chosen foe");
        }
        charmed = logged(*m, "Foe is Charmed by Wizard.");
    }
    check(charmed, "Charm Monster charms a creature that is no Humanoid");
}

void confusion_checks()
{
    const auto rules = dummy_rules();
    bool confused = false;
    for (std::uint64_t seed = 1; seed < 40 && !confused; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"confusion"}),
                       {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, seed);
        check(submit(*c, "confusion") && submit_cell(*c, "area_move", Cell{8, 2}) &&
                  submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"),
              "Confusion");
        if (!logged(*c, "Foe is Confused."))
        {
            continue;
        }
        confused = true;
        check(submit(*c, "end"), "End the Wizard's turn");
        check(logged(*c, "Foe wanders in confusion.") || logged(*c, "Foe stands confused") ||
                  logged(*c, "Foe lashes out in confusion.") ||
                  logged(*c, "Foe shakes off the confusion"),
              "A d10 decides the confused creature's turn");
    }
    check(confused, "A seed where the foe fails its save");
}

// Slow: only the caster's enemies in the Cube, -2 AC and half Speed; a slowed
// caster saves to cast and takes an action or a Bonus Action, not both.
void slow_checks()
{
    const auto rules = dummy_rules();
    bool slowed = false;
    for (std::uint64_t seed = 1; seed < 40 && !slowed; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"slow"}),
                       {{2, "dummy", "Ally", Side::party, {7, 1}},
                        {98, "dummy", "Foe", Side::opposition, {8, 2}}},
                       seed);
        const auto ac = unit(*c, 98).armor_class;
        check(submit(*c, "slow") && submit_cell(*c, "area_move", Cell{8, 2}) &&
                  submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:") &&
                  !logged(*c, "Ally Wisdom save:"),
              "Slow takes only enemies");
        if (!logged(*c, "Foe is slowed."))
        {
            continue;
        }
        slowed = true;
        check(unit(*c, 98).armor_class == ac - 2 &&
                  unit(*c, 98).status.find("Slow") != std::string::npos,
              "Slow: -2 AC");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps Slow");
        to_turn(*c, 98);
        check(unit(*c, 98).movement_feet == 15, "Slow halves the Speed");
    }
    check(slowed, "A seed where the foe fails its save");
    const std::vector<std::string> gear{"quarterstaff"};
    bool failed = false, cast = false;
    for (std::uint64_t seed = 1; seed < 120 && !(failed && cast); ++seed)
    {
        auto c = field(
            *rules, with_spells(wizard(5), {"misty_step"}),
            {{3,
              "campaign-character",
              "Rival",
              Side::opposition,
              {9, 2},
              rules->character_profile(with_spells(wizard(5, "Rival"), {"slow"}), gear).data}},
            seed);
        to_turn(*c, 3);
        check(submit(*c, "slow") && submit_cell(*c, "area_move", Cell{2, 2}) &&
                  submit(*c, "area_cast"),
              "The rival casts Slow");
        if (!logged(*c, "Wizard is slowed."))
        {
            continue;
        }
        to_turn(*c, 1);
        check(submit(*c, "misty_step") && submit_cell(*c, "area_move", Cell{1, 5}) &&
                  submit(*c, "area_cast") && !unit(*c, 1).action && !unit(*c, 1).bonus_action,
              "A slowed creature's Bonus Action spends its action too");
        if (logged(*c, "Wizard's spell fails under Slow."))
        {
            failed = unit(*c, 1).cell == (Cell{1, 2});
        }
        else
        {
            cast = unit(*c, 1).cell == (Cell{1, 5});
        }
    }
    check(failed && cast, "A slowed caster's spell fails on a failed Wisdom save");
}

void dimension_door_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"dimension_door"}),
                   {{2, "dummy", "Ally", Side::party, {1, 3}},
                    {98, "dummy", "Foe", Side::opposition, {12, 2}}},
                   1);
    check(offered(*c, "dimension_door_with", 2), "Dimension Door may take the ally beside");
    check(submit(*c, "dimension_door_with", 2) && submit_cell(*c, "area_move", Cell{11, 5}) &&
              submit(*c, "area_cast") && unit(*c, 1).cell == (Cell{11, 5}) &&
              logged(*c, "Ally steps through with Wizard."),
          "The Wizard and the ally step across");
    check(std::max(std::abs(unit(*c, 2).cell.x - 11), std::abs(unit(*c, 2).cell.y - 5)) <= 1,
          "The ally arrives within 5 feet");
}

void fire_shield_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"fire_shield"}),
                   {{98, "brute", "Foe", Side::opposition, {2, 2}}}, 1);
    check(offered(*c, "fire_shield_chill") && submit(*c, "fire_shield_warm") &&
              logged(*c, "Wizard is wreathed in a warm shield."),
          "Fire Shield's warm shield");
    to_turn(*c, 98);
    check(submit(*c, "melee", 1), "The brute strikes");
    // The Wizard declines to cast Shield.
    if (c->snapshot().reaction_pending)
    {
        check(submit(*c, "decline"), "Decline Shield");
    }
    check(logged(*c, "Foe takes ") && logged(*c, " Fire damage from the fire shield."),
          "The shield burns a melee attacker");
}

// A Monk of the level, its creation choices the first ones offered.
Character monk(unsigned level)
{
    CharacterDraft d;
    d.race = "dwarf";
    d.gender = "female";
    d.character_class = "monk";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Rival";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    const auto creation = srd5::character_rules();
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : creation->training_options(d))
        {
            if (!d.training.contains(group.id))
            {
                for (unsigned i = 0; i < group.count; ++i)
                {
                    d.training[group.id].push_back(group.options.at(i).id);
                }
                filled = true;
                break;
            }
        }
    }
    Character hero(*creation, d, {});
    VitalState life;
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        check(hero.advance(*rules, life), "An ordinary level-up");
    }
    return hero;
}

// An Unarmed Strike is made through a borrowed loadout (#235): Fire Shield's
// damage back must reach the Monk itself.
void borrowed_attack_checks()
{
    const auto rules = dummy_rules();
    const std::vector<std::string> bare;
    // The fire shield's damage logged from the `from`th line on.
    const auto burns = [](const CombatSession &c, std::size_t from)
    {
        int total = 0;
        const auto log = c.snapshot().log();
        for (auto line = log.begin() + static_cast<std::ptrdiff_t>(from); line != log.end(); ++line)
        {
            if (const auto at = line->find(" Fire damage from the fire shield.");
                line->starts_with("Rival takes ") && at != std::string::npos)
            {
                total += std::stoi(line->substr(12, at - 12));
            }
        }
        return total;
    };
    const auto decline = [](CombatSession &c)
    {
        while (c.snapshot().reaction_pending)
        {
            check(submit(c, "decline"), "Decline Shield");
        }
    };
    bool flurry = false, martial_arts = false;
    for (std::uint64_t seed = 1; seed < 40 && !(flurry && martial_arts); ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"fire_shield"}),
                       {{3,
                         "campaign-character",
                         "Rival",
                         Side::opposition,
                         {2, 2},
                         rules->character_profile(monk(5).sheet(), bare).data}},
                       seed);
        check(submit(*c, "fire_shield_warm"), "Fire Shield's warm shield");
        to_turn(*c, 3);
        // Flurry of Blows: two Unarmed Strikes.
        auto from = c->snapshot().log().size();
        int before = unit(*c, 3).hit_points;
        check(submit(*c, "flurry_of_blows", 1), "A Flurry of Blows");
        decline(*c);
        if (const int burned = burns(*c, from); burned && !flurry)
        {
            flurry = true;
            check(unit(*c, 3).hit_points == std::max(0, before - burned),
                  "A Flurry's striker loses the Hit Points Fire Shield deals it");
        }
        // Martial Arts' Unarmed Strike after the Attack action, next turn.
        if (unit(*c, 3).hit_points == 0)
        {
            continue;
        }
        to_turn(*c, 1);
        to_turn(*c, 3);
        check(submit(*c, "melee", 1), "The Monk attacks");
        decline(*c);
        from = c->snapshot().log().size();
        before = unit(*c, 3).hit_points;
        if (before == 0 || !submit(*c, "martial_arts", 1))
        {
            continue;
        }
        decline(*c);
        if (const int burned = burns(*c, from); burned && !martial_arts)
        {
            martial_arts = true;
            check(unit(*c, 3).hit_points == std::max(0, before - burned),
                  "A Martial Arts striker loses the Hit Points Fire Shield deals it");
        }
    }
    check(flurry && martial_arts, "Seeds where both Unarmed Strikes hit the shielded Wizard");
}

void invisibility_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"greater_invisibility"}),
                   {{98, "dummy", "Foe", Side::opposition, {3, 2}}}, 1);
    check(submit(*c, "greater_invisibility", 1) && logged(*c, "Wizard turns Invisible."),
          "Greater Invisibility");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "fire_bolt", 98) && unit(*c, 1).status.find("Invisible") != std::string::npos,
          "Attacking does not end it");
}

void ice_storm_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"ice_storm"}),
                   {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*c, "ice_storm") && submit_cell(*c, "area_move", Cell{9, 2}) &&
              submit(*c, "area_cast") && logged(*c, " Bludgeoning damage.") &&
              logged(*c, " Cold damage."),
          "Ice Storm deals Bludgeoning and Cold");
    check(c->snapshot().battlefield.at({9, 3}) == Terrain::difficult,
          "Hail makes Difficult Terrain");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the hail");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "end"), "End the next turn");
    check(c->snapshot().battlefield.at({9, 3}) == Terrain::open,
          "The hail melts after the next turn");
}

void phantasmal_killer_checks()
{
    const auto rules = dummy_rules();
    bool haunted = false;
    for (std::uint64_t seed = 1; seed < 40 && !haunted; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"phantasmal_killer"}),
                       {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, seed);
        check(submit(*c, "phantasmal_killer", 98) && logged(*c, "Foe Wisdom save:"),
              "Phantasmal Killer");
        if (!logged(*c, "Foe is gripped by its worst fears."))
        {
            continue;
        }
        haunted = true;
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the nightmare");
        to_turn(*c, 98);
        const auto before = c->snapshot().log().size();
        check(submit(*c, "end"), "The foe ends its turn");
        const auto log = c->snapshot().log();
        check(std::any_of(log.begin() + static_cast<std::ptrdiff_t>(before), log.end(),
                          [](const auto &line)
                          {
                              return line.starts_with("Foe Wisdom save:");
                          }),
              "The foe saves again at the end of its turn");
    }
    check(haunted, "A seed where the foe fails its save");
}

void polymorph_checks()
{
    const auto rules = dummy_rules();
    bool turned = false;
    for (std::uint64_t seed = 1; seed < 40 && !turned; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"polymorph"}),
                       {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, seed);
        check(offered(*c, "polymorph_boar", 98) && submit(*c, "polymorph_wolf", 98) &&
                  logged(*c, "Foe Wisdom save:"),
              "Polymorph");
        if (!logged(*c, "Foe turns into a Wolf."))
        {
            continue;
        }
        turned = true;
        check(unit(*c, 98).armor_class == 12 && unit(*c, 98).temporary_hp.amount == 11,
              "The Wolf's AC and 11 Temporary Hit Points");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the form");
    }
    check(turned, "A seed where the foe fails its save");
}

void sphere_checks()
{
    const auto rules = dummy_rules();
    bool sealed = false;
    for (std::uint64_t seed = 1; seed < 40 && !sealed; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"resilient_sphere"}),
                       {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, seed);
        check(submit(*c, "resilient_sphere", 98) && logged(*c, "Foe Dexterity save:"),
              "Resilient Sphere");
        if (!logged(*c, "Foe is sealed in a shimmering sphere."))
        {
            continue;
        }
        sealed = true;
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(std::all_of(commands.begin(), commands.end(),
                          [](const auto &command)
                          {
                              return command.verb == "move" || command.verb == "end";
                          }),
              "A sealed creature only rolls its sphere");
        to_turn(*c, 1);
        check(!offered(*c, "fire_bolt", 98), "Nothing reaches into the sphere");
    }
    check(sealed, "A seed where the foe fails its save");
}

void vitriol_checks()
{
    const auto rules = dummy_rules();
    bool burned = false;
    for (std::uint64_t seed = 1; seed < 40 && !burned; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(7), {"vitriolic_sphere"}),
                       {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, seed);
        check(submit(*c, "vitriolic_sphere") && submit_cell(*c, "area_move", Cell{9, 2}) &&
                  submit(*c, "area_cast") && logged(*c, " Acid damage."),
              "Vitriolic Sphere");
        to_turn(*c, 98);
        check(submit(*c, "end"), "The foe ends its turn");
        burned = logged(*c, "Foe takes ") && logged(*c, " Acid damage from the vitriol.");
    }
    check(burned, "A failed save burns again at the end of the next turn");
}

void wall_of_fire_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"wall_of_fire"}),
                   {{97, "dummy", "Inside", Side::opposition, {8, 2}},
                    {98, "dummy", "Beside", Side::opposition, {9, 3}}},
                   1);
    check(offered(*c, "wall_of_fire_vertical_east") && offered(*c, "wall_of_fire_ring_inner"),
          "Wall of Fire offers each direction's two sides and a ring's");
    check(submit(*c, "wall_of_fire_vertical_east") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast") && logged(*c, "Inside Dexterity save:") &&
              !c->snapshot().walls.empty(),
          "The wall rises through a foe");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wall");
    to_turn(*c, 98);
    check(submit(*c, "end") && logged(*c, "Beside takes ") &&
              logged(*c, " Fire damage from the wall of fire."),
          "Its burning side burns a creature ending its turn there");
}

void hound_and_elementals_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(8), {"faithful_hound", "conjure_minor_elementals"}),
                   {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(submit(*c, "faithful_hound") && submit_cell(*c, "area_move", Cell{5, 2}) &&
              submit(*c, "area_cast") && logged(*c, "A phantom watchdog appears."),
          "Faithful Hound");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the hound");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(logged(*c, "The hound bites at Foe."),
          "The hound bites at the start of the Wizard's turn");
    check(offered(*c, "conjure_minor_elementals_cold") &&
              submit(*c, "conjure_minor_elementals_fire") &&
              logged(*c, "Elemental spirits flit around Wizard."),
          "Conjure Minor Elementals: Fire");
    bool burned = false;
    for (unsigned turns = 0; turns < 8 && !burned; ++turns)
    {
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        Command step;
        for (const auto &command : c->legal_commands())
        {
            if (command.verb == "move" && command.destination == Cell{5, 2})
            {
                step = command;
            }
        }
        if (step.actor)
        {
            check(c->submit(step), "Step beside the foe");
        }
        if (submit(*c, "melee", 98))
        {
            burned = logged(*c, " Fire damage from the elemental spirits.");
        }
    }
    check(burned, "A hit on a creature in the Emanation deals the chosen type");
}

bool has_combatant(const CombatSession &c, std::string_view form)
{
    const auto all = c.snapshot().combatants;
    return std::any_of(all.begin(), all.end(),
                       [&](const auto &a)
                       {
                           return a.form == form;
                       });
}

EntityId combatant_with(const CombatSession &c, std::string_view form)
{
    for (const auto &a : c.snapshot().combatants)
    {
        if (a.form == form)
        {
            return a.id;
        }
    }
    return 0;
}

void level_five_checks()
{
    const auto rules = dummy_rules();
    // Cone of Cold.
    auto cone = field(*rules, with_spells(wizard(9), {"cone_of_cold"}),
                      {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, 1);
    check(submit(*cone, "cone_of_cold") && submit_cell(*cone, "area_move", Cell{5, 2}) &&
              submit(*cone, "area_cast") && logged(*cone, " Cold damage."),
          "Cone of Cold");
    // Hold Monster and Dominate Person.
    bool held = false, dominated = false;
    for (std::uint64_t seed = 1; seed < 40 && (!held || !dominated); ++seed)
    {
        auto c = field(*rules, with_spells(wizard(9), {"hold_monster", "dominate_person"}),
                       {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, seed);
        if (!held)
        {
            check(submit(*c, "hold_monster", 98) ||
                      (c->snapshot().spell_targeting && submit(*c, "spell_cast")),
                  "Hold Monster");
            if (c->snapshot().spell_targeting)
            {
                check(submit(*c, "spell_cast"), "Hold the chosen foe");
            }
            held = unit(*c, 98).status.find("Paralyzed") != std::string::npos;
            continue;
        }
        check(submit(*c, "dominate_person", 98), "Dominate Person");
        dominated = logged(*c, "Foe is dominated by Wizard.");
    }
    check(held && dominated, "Hold Monster paralyzes and Dominate Person dominates");
}

void cloudkill_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"cloudkill"}),
                   {{98, "dummy", "Foe", Side::opposition, {7, 2}}}, 1);
    check(submit(*c, "cloudkill") && submit_cell(*c, "area_move", Cell{7, 2}) &&
              submit(*c, "area_cast"),
          "Cast Cloudkill");
    check(logged(*c, " Poison damage from the fog.") && !c->snapshot().obscured.empty(),
          "Cloudkill's fog");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the fog");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(logged(*c, "The cloud drifts."), "The fog drifts at the start of the Wizard's turn");
}

void conjure_elemental_checks()
{
    const auto rules = dummy_rules();
    bool held = false;
    for (std::uint64_t seed = 1; seed < 40 && !held; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(9), {"conjure_elemental"}),
                       {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, seed);
        check(offered(*c, "conjure_elemental_water") && submit(*c, "conjure_elemental_fire") &&
                  submit_cell(*c, "area_move", Cell{6, 2}) && submit(*c, "area_cast") &&
                  logged(*c, "An elemental spirit appears."),
              "Conjure Elemental");
        to_turn(*c, 98);
        held = logged(*c, "Foe is Restrained by the spirit.") &&
               logged(*c, " Fire damage from the elemental spirit.");
        if (held)
        {
            const auto saved = c->save();
            check(rules->restore(saved)->save() == saved, "A checkpoint keeps the spirit");
        }
    }
    check(held, "The spirit seizes a foe starting its turn beside it");
}

void summon_dragon_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"summon_dragon"}),
                   {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(offered(*c, "summon_dragon_acid") && submit(*c, "summon_dragon_fire") &&
              submit_cell(*c, "area_move", Cell{4, 2}) && submit(*c, "area_cast") &&
              logged(*c, "A Draconic Spirit answers Wizard.") &&
              has_combatant(*c, "draconic_spirit"),
          "A Draconic Spirit appears with the dragon's art");
    const auto spirit = combatant_with(*c, "draconic_spirit");
    const auto order = c->snapshot().combatants;
    check(order[1].id == spirit || std::find_if(order.begin(), order.end(),
                                                [&](const auto &a)
                                                {
                                                    return a.id == 1;
                                                }) +
                                           1 ==
                                       std::find_if(order.begin(), order.end(),
                                                    [&](const auto &a)
                                                    {
                                                        return a.id == spirit;
                                                    }),
          "It acts right after the Wizard");
    check(unit(*c, spirit).armor_class == 19 && unit(*c, spirit).max_hit_points == 50,
          "A level-5 spirit: AC 19, 50 Hit Points");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the spirit");
    check(submit(*c, "end"), "End the Wizard's turn");
    check(c->snapshot().actor == spirit && offered(*c, "spirit_breath"),
          "The spirit acts next and may breathe");
    check(submit(*c, "spirit_breath") && submit_cell(*c, "area_move", Cell{9, 2}) &&
              submit(*c, "area_cast") && logged(*c, "Draconic Spirit breathes.") &&
              !offered(*c, "spirit_breath"),
          "It breathes once a turn");
}

void animate_objects_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"animate_objects"}),
                   {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*c, "animate_objects", 1) && logged(*c, " objects animate for Wizard."),
          "Animate Objects");
    unsigned objects = 0;
    for (const auto &a : c->snapshot().combatants)
    {
        objects += a.form == "animated_object";
    }
    check(objects >= 3, "As many objects as the Intelligence modifier");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the objects");
}

void mislead_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"mislead"}),
                   {{98, "brute", "Foe", Side::opposition, {4, 2}}}, 1);
    check(submit(*c, "mislead", 1) && unit(*c, 1).status.find("Invisible") != std::string::npos &&
              has_combatant(*c, "copy:1"),
          "Mislead: Invisible, with a double wearing the Wizard's art");
    const auto twin = combatant_with(*c, "copy:1");
    to_turn(*c, 98);
    Command near;
    for (const auto &command : c->legal_commands())
    {
        if (command.verb == "move" &&
            std::max(std::abs(command.destination.x - unit(*c, twin).cell.x),
                     std::abs(command.destination.y - unit(*c, twin).cell.y)) == 1)
        {
            near = command;
        }
    }
    if (near.actor)
    {
        check(c->submit(near), "The foe closes on the double");
    }
    check(submit(*c, "melee", twin) &&
              logged(*c, "The attack passes through an illusory double.") &&
              !offered(*c, "melee", twin),
          "An attack passes through the double, and foes ignore it after");
}

void telekinesis_checks()
{
    const auto rules = dummy_rules();
    bool moved = false;
    for (std::uint64_t seed = 1; seed < 40 && !moved; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(9), {"telekinesis"}),
                       {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, seed);
        check(submit(*c, "telekinesis", 98) && c->snapshot().area_targeting &&
                  submit_cell(*c, "area_move", Cell{10, 2}) && submit(*c, "area_cast") &&
                  logged(*c, "Foe Strength save:"),
              "Telekinesis seizes the foe");
        if (unit(*c, 98).cell != (Cell{10, 2}))
        {
            continue;
        }
        moved = true;
        check(unit(*c, 98).status.find("Restrained") != std::string::npos, "The foe is held");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(offered(*c, "telekinesis_again", 98), "A Magic action seizes it again");
    }
    check(moved, "A seed where the foe fails its save");
}

void walls_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(10), {"wall_of_force", "wall_of_stone"}),
                   {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(offered(*c, "fire_bolt", 98), "Fire Bolt before the wall");
    check(offered(*c, "wall_of_force_ring") && submit(*c, "wall_of_force_vertical") &&
              submit_cell(*c, "area_move", Cell{5, 2}) && submit(*c, "area_cast") &&
              c->snapshot().battlefield.at({5, 2}) == Terrain::obstacle &&
              logged(*c, "A wall of force springs up."),
          "A Wall of Force across the field");
    check(!offered(*c, "fire_bolt", 98), "Nothing passes through it");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wall");

    auto stone = field(*rules, with_spells(wizard(10), {"wall_of_force", "wall_of_stone"}),
                       {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(submit(*stone, "wall_of_stone_vertical") &&
              submit_cell(*stone, "area_move", Cell{8, 2}) && submit(*stone, "area_cast") &&
              logged(*stone, "A wall of stone rises.") &&
              stone->snapshot().battlefield.at({8, 1}) == Terrain::obstacle &&
              stone->snapshot().battlefield.at({8, 2}) != Terrain::obstacle,
          "A Wall of Stone rises around a creature's square, not in it");
    const auto kept = stone->save();
    check(rules->restore(kept)->save() == kept, "A checkpoint keeps the stone");
}

void arcane_hand_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"arcane_hand"}),
                   {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*c, "arcane_hand") && submit_cell(*c, "area_move", Cell{5, 2}) &&
              submit(*c, "area_cast") && logged(*c, "A hand of shimmering force appears."),
          "Arcane Hand");
    check(offered(*c, "hand_push", 98) && submit(*c, "hand_fist", 98) &&
              logged(*c, "The hand strikes Foe.") && unit(*c, 1).bonus_action,
          "The hand strikes as it appears, without the Bonus Action");
    check(!offered(*c, "hand_fist", 98) || unit(*c, 1).bonus_action,
          "Later, with the Bonus Action");
}

void level_six_checks()
{
    const auto rules = dummy_rules();
    // Chain Lightning: the first foe and others within 30 feet of it.
    auto chain = field(*rules, with_spells(wizard(11), {"chain_lightning"}),
                       {{97, "dummy", "First", Side::opposition, {6, 2}},
                        {98, "dummy", "Near", Side::opposition, {8, 2}},
                        {99, "dummy", "Far", Side::opposition, {13, 6}}},
                       1);
    check(submit(*chain, "chain_lightning", 97) && submit(*chain, "chain_lightning", 98) &&
              submit(*chain, "chain_lightning", 99) && submit(*chain, "spell_cast") &&
              logged(*chain, "First takes ") && logged(*chain, "Near takes ") &&
              !logged(*chain, "Far takes "),
          "Chain Lightning leaps only within 30 feet of the first foe");
    // Circle of Death and Freezing Sphere.
    auto circle = field(*rules, with_spells(wizard(11), {"circle_of_death"}),
                        {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*circle, "circle_of_death") && submit_cell(*circle, "area_move", Cell{9, 2}) &&
              submit(*circle, "area_cast") && logged(*circle, " Necrotic damage."),
          "Circle of Death");
    auto sphere = field(*rules, with_spells(wizard(11), {"freezing_sphere"}),
                        {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*sphere, "freezing_sphere") && submit_cell(*sphere, "area_move", Cell{9, 2}) &&
              submit(*sphere, "area_cast") && logged(*sphere, "Foe Constitution save: d20 ") &&
              logged(*sphere, " Cold damage."),
          "Freezing Sphere");
    // Disintegrate turns a creature it drops to dust.
    bool dust = false;
    for (std::uint64_t seed = 1; seed < 40 && !dust; ++seed)
    {
        auto c = field(
            *rules, with_spells(wizard(11), {"disintegrate"}),
            {{98, "dummy", "Foe", Side::opposition, {5, 2}, {}, VitalState{30, false, {}}}}, seed);
        check(submit(*c, "disintegrate", 98), "Disintegrate");
        dust = logged(*c, "Foe turns to dust.");
    }
    check(dust, "Disintegrate turns a creature it drops to dust");
}

void eyebite_checks()
{
    const auto rules = dummy_rules();
    bool asleep = false;
    for (std::uint64_t seed = 1; seed < 40 && !asleep; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(11), {"eyebite"}),
                       {{97, "dummy", "Other", Side::opposition, {6, 4}},
                        {98, "dummy", "Foe", Side::opposition, {6, 2}}},
                       seed);
        check(offered(*c, "eyebite_sickened", 98) && submit(*c, "eyebite_asleep", 98) &&
                  logged(*c, "Foe Wisdom save:"),
              "Eyebite");
        if (!logged(*c, "Foe falls asleep."))
        {
            continue;
        }
        asleep = true;
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(offered(*c, "evil_eye_panicked", 97) && !offered(*c, "evil_eye_panicked", 98),
              "A Magic action targets another creature");
    }
    check(asleep, "A seed where the foe falls asleep");
    // A sickened creature is Poisoned: Disadvantage on its attack rolls.
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(11), {"eyebite"}),
                       {{98, "brute", "Foe", Side::opposition, {2, 2}}}, seed);
        check(submit(*c, "eyebite_sickened", 98), "Eyebite");
        if (!logged(*c, "Foe is sickened."))
        {
            continue;
        }
        check(submit(*c, "end"), "End the turn");
        check(c->snapshot().actor == 98 && submit(*c, "melee", 1), "The sickened foe attacks");
        // The Wizard declines any Reaction the attack offers.
        submit(*c, "decline");
        check(logged(*c, "Foe -> Wizard: d20") && logged(*c, " (disadvantage)"),
              "Its attack roll has Disadvantage");
        return;
    }
    throw std::runtime_error("No seed sickens the foe");
}

void flesh_and_dance_checks()
{
    const auto rules = dummy_rules();
    bool stone = false, dancing = false;
    for (std::uint64_t seed = 1; seed < 60 && (!stone || !dancing); ++seed)
    {
        auto c = field(*rules, with_spells(wizard(11), {"flesh_to_stone"}),
                       {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, seed);
        if (!stone)
        {
            check(submit(*c, "flesh_to_stone", 98), "Flesh to Stone");
            for (unsigned turns = 0; turns < 8 && !logged(*c, "Foe turns to stone.") &&
                                     logged(*c, "Foe begins to turn to stone.");
                 ++turns)
            {
                check(submit(*c, "end"), "Pass a turn");
            }
            stone = logged(*c, "Foe turns to stone.");
            if (stone)
            {
                check(unit(*c, 98).status.find("Petrified") != std::string::npos, "Petrified");
            }
        }
        auto d = field(*rules, with_spells(wizard(11), {"irresistible_dance"}),
                       {{98, "dummy", "Foe", Side::opposition, {3, 2}}}, seed);
        if (!dancing && submit(*d, "irresistible_dance", 98) &&
            logged(*d, "Foe breaks into a helpless dance."))
        {
            dancing = true;
            to_turn(*d, 98);
            check(offered(*d, "stop_dancing") && !offered(*d, "move"),
                  "The dancer can only try to stop");
        }
    }
    check(stone && dancing, "Flesh to Stone petrifies and Irresistible Dance sets a foe dancing");
}

void globe_checks()
{
    const auto rules = dummy_rules();
    const std::vector<std::string> gear{"quarterstaff"};
    auto c = rules->create(
        {{14, 7, std::vector<Terrain>(98)},
         {{1,
           "campaign-character",
           "Wizard",
           Side::party,
           {1, 2},
           rules->character_profile(with_spells(wizard(11), {"globe_of_invulnerability"}), gear)
               .data},
          {98,
           "campaign-character",
           "Rival",
           Side::opposition,
           {8, 2},
           rules->character_profile(wizard(5, "Rival").sheet(), gear).data}}},
        1);
    to_turn(*c, 1);
    check(submit(*c, "globe_of_invulnerability", 1) &&
              logged(*c, "A shimmering globe rises around Wizard."),
          "Globe of Invulnerability");
    to_turn(*c, 98);
    check(!offered(*c, "fire_bolt", 1), "A spell of level 5 or lower is not cast into the globe");
}

void magic_jar_checks()
{
    const auto rules = dummy_rules();
    bool possessed = false;
    for (std::uint64_t seed = 1; seed < 40 && !possessed; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(11), {"magic_jar"}),
                       {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, seed);
        check(submit(*c, "magic_jar", 98) && logged(*c, "Foe Charisma save:"), "Magic Jar");
        if (!logged(*c, "Foe is dominated by Wizard."))
        {
            continue;
        }
        possessed = true;
        check(unit(*c, 1).status.find("Magic Jar") != std::string::npos,
              "The Wizard's body lies helpless");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 98);
        check(submit(*c, "leave_host", 98) && logged(*c, "Wizard's soul returns to its body."),
              "The soul returns to its body");
    }
    check(possessed, "A seed where the foe fails its save");
}

void wall_of_ice_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(11), {"wall_of_ice"}),
                   {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(submit(*c, "wall_of_ice_vertical") && submit_cell(*c, "area_move", Cell{6, 2}) &&
              submit(*c, "area_cast") && logged(*c, "A wall of ice rises.") &&
              logged(*c, "Foe is pushed aside by the wall.") && logged(*c, " Cold damage.") &&
              c->snapshot().battlefield.at({6, 2}) == Terrain::obstacle,
          "Wall of Ice");
}

void arcane_sword_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(13), {"arcane_sword"}),
                   {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(submit(*c, "arcane_sword", 98) && logged(*c, "Wizard -> Foe") &&
              c->snapshot().spiritual_weapons.size() == 1 &&
              !offered(*c, "arcane_sword_strike", 98),
          "Arcane Sword strikes as it appears, not again this turn");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "arcane_sword_strike", 98) && logged(*c, "Wizard's Arcane Sword strikes."),
          "A Bonus Action strikes again on a later turn");
}

void delayed_blast_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(13), {"delayed_blast_fireball"}),
                   {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*c, "delayed_blast_fireball") && submit_cell(*c, "area_move", Cell{9, 2}) &&
              submit(*c, "area_cast") && logged(*c, "A glowing bead hangs in the air.") &&
              !logged(*c, "The bead bursts."),
          "The bead waits");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "detonate") && logged(*c, "The bead bursts.") && logged(*c, " Fire damage."),
          "Ending the spell bursts the bead");
}

void finger_of_death_checks()
{
    const auto rules = dummy_rules();
    bool risen = false;
    for (std::uint64_t seed = 1; seed < 20 && !risen; ++seed)
    {
        auto c =
            field(*rules, with_spells(wizard(13), {"finger_of_death"}),
                  {{97, "dummy", "Other", Side::opposition, {12, 6}},
                   {98, "dummy", "Foe", Side::opposition, {5, 2}, {}, VitalState{20, false, {}}}},
                  seed);
        check(submit(*c, "finger_of_death", 98), "Finger of Death");
        if (!unit(*c, 98).dead)
        {
            continue;
        }
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        risen = logged(*c, "Foe rises as a Zombie.");
    }
    check(risen, "A Humanoid slain by Finger of Death rises as a Zombie");
}

void forcecage_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(13), {"forcecage"}),
                   {{98, "dummy", "Foe", Side::opposition, {8, 3}}}, 1);
    check(submit(*c, "forcecage_cage") && submit_cell(*c, "area_move", Cell{8, 3}) &&
              submit(*c, "area_cast") && logged(*c, "A cage of force closes."),
          "Forcecage");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 98);
    // The 20-foot cage around (8, 3) spans columns 7 to 10 and rows 2 to 5.
    const auto commands = c->legal_commands();
    check(std::none_of(commands.begin(), commands.end(),
                       [](const auto &command)
                       {
                           return command.verb == "move" &&
                                  (command.destination.x < 7 || command.destination.x > 10 ||
                                   command.destination.y < 2 || command.destination.y > 5);
                       }),
          "No move leaves the cage");
}

void prismatic_and_gravity_checks()
{
    const auto rules = dummy_rules();
    auto spray = field(*rules, with_spells(wizard(13), {"prismatic_spray"}),
                       {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, 1);
    check(submit(*spray, "prismatic_spray") && submit_cell(*spray, "area_move", Cell{5, 2}) &&
              submit(*spray, "area_cast") && logged(*spray, "Foe is struck by the "),
          "Prismatic Spray");
    bool lifted = false;
    for (std::uint64_t seed = 1; seed < 30 && !lifted; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(13), {"reverse_gravity"}),
                       {{98, "dummy", "Foe", Side::opposition, {12, 2}}}, seed);
        check(submit(*c, "reverse_gravity") && submit_cell(*c, "area_move", Cell{12, 5}) &&
                  submit(*c, "area_cast"),
              "Reverse Gravity");
        if (!logged(*c, "Foe falls upward."))
        {
            continue;
        }
        lifted = true;
        check(logged(*c, "Foe takes ") &&
                  unit(*c, 98).status.find("Fallen upward") != std::string::npos,
              "The foe falls upward and is hurt");
    }
    check(lifted, "A seed where the foe fails its save");
}

void level_eight_checks()
{
    const auto rules = dummy_rules();
    // Power Word Stun: Stunned at 150 Hit Points or fewer, held in place above.
    auto stun =
        field(*rules, with_spells(wizard(15), {"power_word_stun"}),
              {{97, "dummy", "Small", Side::opposition, {5, 2}, {}, VitalState{100, false, {}}},
               {98, "dummy", "Big", Side::opposition, {6, 4}}},
              1);
    check(submit(*stun, "power_word_stun", 97) && logged(*stun, "Small is Stunned."),
          "Power Word Stun stuns a creature of 150 Hit Points or fewer");
    auto held = field(*rules, with_spells(wizard(15), {"power_word_stun"}),
                      {{98, "dummy", "Big", Side::opposition, {6, 4}}}, 1);
    check(submit(*held, "power_word_stun", 98) &&
              logged(*held, "Big cannot move until Wizard's next turn."),
          "Above 150 Hit Points its Speed is 0");
    // Maze: no save; the lost creature's turns go to seeking the way out.
    auto maze = field(*rules, with_spells(wizard(15), {"maze"}),
                      {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(submit(*maze, "maze", 98) && logged(*maze, "Foe is lost in a maze.") &&
              unit(*maze, 98).banished,
          "Maze");
    check(submit(*maze, "end") && logged(*maze, "Foe Investigation check: d20 "),
          "The lost creature seeks the way out");
    // Mind Blank: Immunity to Psychic damage and charms.
    auto blank = field(*rules, with_spells(wizard(15), {"mind_blank"}),
                       {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(submit(*blank, "mind_blank", 1) &&
              unit(*blank, 1).status.find("Mind Blank") != std::string::npos,
          "Mind Blank");
    // Befuddlement and Dominate Monster.
    bool befuddled = false, dominated = false;
    for (std::uint64_t seed = 1; seed < 40 && (!befuddled || !dominated); ++seed)
    {
        auto c = field(*rules, with_spells(wizard(15), {"befuddlement"}),
                       {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, seed);
        check(submit(*c, "befuddlement", 98) && logged(*c, " Psychic damage."), "Befuddlement");
        befuddled |= unit(*c, 98).status.find("Befuddled") != std::string::npos;
        auto d = field(*rules, with_spells(wizard(15), {"dominate_monster"}),
                       {{98, "brute", "Brute", Side::opposition, {6, 2}}}, seed);
        check(submit(*d, "dominate_monster", 98), "Dominate Monster");
        dominated |= logged(*d, "Brute is dominated by Wizard.");
    }
    check(befuddled && dominated, "Befuddlement and Dominate Monster land");
    // Befuddlement lasts the fight (LEVELS-5).
    const std::vector<std::string> gear{"quarterstaff"};
    bool lifted = false;
    for (std::uint64_t seed = 1; seed < 40 && !lifted; ++seed)
    {
        auto c = field(
            *rules, wizard(15).sheet(),
            {{2, "slayer", "Slayer", Side::party, {8, 2}},
             {3,
              "campaign-character",
              "Rival",
              Side::opposition,
              {9, 2},
              rules->character_profile(with_spells(wizard(15, "Rival"), {"befuddlement"}), gear)
                  .data}},
            seed);
        to_turn(*c, 3);
        check(submit(*c, "befuddlement", 1), "The rival casts Befuddlement");
        if (unit(*c, 1).status.find("Befuddled") == std::string::npos)
        {
            continue;
        }
        for (unsigned turns = 0; turns < 40 && c->snapshot().outcome == Outcome::ongoing; ++turns)
        {
            check(submit(*c, "melee", 3) || submit(*c, "end") || submit(*c, "decline"),
                  "The slayer strikes the rival down");
        }
        check(c->snapshot().outcome == Outcome::victory, "The slayer ends the fight");
        lifted = unit(*c, 1).status.find("Befuddled") == std::string::npos &&
                 unit(*c, 1).persistent.resources.find(" 172 ") == std::string::npos;
        check(lifted, "Befuddlement ends with the fight");
    }
    check(lifted, "A seed where the Wizard is befuddled");
    // Incendiary Cloud burns as it appears.
    auto cloud = field(*rules, with_spells(wizard(15), {"incendiary_cloud"}),
                       {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*cloud, "incendiary_cloud") && submit_cell(*cloud, "area_move", Cell{9, 2}) &&
              submit(*cloud, "area_cast") && logged(*cloud, "Fire damage from the embers."),
          "Incendiary Cloud");
}

void overchannel_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, wizard(14).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    const auto commands = c->legal_commands();
    check(std::any_of(commands.begin(), commands.end(),
                      [](const auto &command)
                      {
                          return command.verb == "overchannel" &&
                                 command.label == "Overchannel (free)";
                      }),
          "The first Overchannel is free");
    // Magic Missile's three darts are aimed one click at a time.
    check(submit(*c, "overchannel") && submit(*c, "magic_missile", 98) &&
              submit(*c, "magic_missile", 98) && submit(*c, "magic_missile", 98) &&
              logged(*c, "Wizard overchannels the spell.") &&
              logged(*c, "Wizard casts Magic Missile for 15 force damage."),
          "Overchannel deals Magic Missile's maximum damage");
    check(!logged(*c, "from Overchannel"), "The first use costs nothing");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "overchannel") && submit(*c, "magic_missile", 98) &&
              submit(*c, "magic_missile", 98) && submit(*c, "magic_missile", 98) &&
              logged(*c, "Necrotic damage from Overchannel."),
          "A second use hurts the Wizard");
}

// A Wizard of the level whose level-up to `learned_at` learns and prepares `spell`.
Character wizard_knowing(unsigned level, const std::string &spell, unsigned learned_at)
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (hero.sheet().level + 1 == static_cast<int>(learned_at))
        {
            auto &book = choice.spell_learning->at("spellbook:" + std::to_string(learned_at));
            if (std::find(book.begin(), book.end(), spell) == book.end())
            {
                book.front() = spell;
            }
            if (std::find(choice.spells.begin(), choice.spells.end(), spell) == choice.spells.end())
            {
                choice.spells.back() = spell;
            }
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

void simulacrum_checks()
{
    const auto rules = module();
    // The copy: half the Hit Point maximum, a Construct, healed only by repair.
    const auto original = wizard(13).sheet();
    auto copy = rules->simulacrum_state(original);
    const int maximum = rules->hit_point_maximum(original, {original.hit_points, false, {}});
    check(rules->hit_point_maximum(original, copy) == maximum / 2 && copy.hit_points == maximum / 2,
          "A simulacrum has half the original's Hit Point maximum");
    rules->set_hit_points(copy, original, maximum / 2 - 5);
    check(rules->camp_action_gold(original, copy, "simulacrum_repair") == 500,
          "A repair costs 100 gp per missing Hit Point");
    rules->recover(copy, original);
    check(copy.hit_points == maximum / 2 - 5, "A simulacrum takes no Long Rest");
    // The campaign: the copy joins in an NPC position and gains no levels.
    CampaignParty party(module());
    const auto id = party.add_pc(wizard_knowing(13, "simulacrum", 13));
    party.award_experience(165000, "simulacrum-xp");
    const auto actions = party.camp_actions(id);
    check(std::any_of(actions.begin(), actions.end(),
                      [](const auto &a)
                      {
                          return a.id == "simulacrum";
                      }) &&
              std::any_of(actions.begin(), actions.end(),
                          [](const auto &a)
                          {
                              return a.id == "simulacrum_repair";
                          }),
          "Simulacrum and its repair are camp actions");
    const auto minutes = party.state().time_minutes;
    const auto first = party.create_simulacrum(id, id, "simulacrum", "Wizard's simulacrum");
    check(party.state().slots[6] == first && party.is_simulacrum(first) &&
              !party.can_advance(first) &&
              party.member(first).character.sheet().name == "Wizard's simulacrum" &&
              party.state().time_minutes == minutes + 720,
          "The simulacrum joins after twelve hours");
    check(slots_left(party, id, 7) == 0, "Simulacrum spends the level-7 slot");
    // A second casting destroys the first simulacrum.
    CampaignParty again(module());
    const auto caster = again.add_pc(wizard_knowing(15, "simulacrum", 13));
    const auto older = again.create_simulacrum(caster, caster, "simulacrum", "First copy");
    const auto newer = again.create_simulacrum(caster, caster, "simulacrum_8", "Second copy");
    check(again.state().slots[6] == newer && again.member(older).vitals.dead &&
              std::count(again.state().slots.begin(), again.state().slots.end(), older) == 0,
          "A new simulacrum destroys the old one");
    const auto bytes = encode_campaign(party, nullptr, "simulacrum");
    CampaignParty next(module());
    next.restore(
        decode_campaign(bytes, *srd5::character_rules(), party.rule_module(), "simulacrum", nullptr)
            .party);
    check(encode_campaign(next, nullptr, "simulacrum") == bytes, "A simulacrum saves and reloads");
    // A simulacrum melts away at 0 Hit Points.
    const auto dummies = dummy_rules();
    const std::vector<std::string> gear{"quarterstaff"};
    auto weak = copy;
    dummies->set_hit_points(weak, original, 1);
    auto c = dummies->create({{14, 7, std::vector<Terrain>(98)},
                              {{1,
                                "campaign-character",
                                "Copy",
                                Side::party,
                                {1, 2},
                                dummies->character_profile(original, gear).data,
                                weak},
                               {98, "brute", "Brute", Side::opposition, {2, 2}},
                               {99, "dummy", "Other", Side::opposition, {12, 6}}}},
                             1);
    to_turn(*c, 98);
    // The copy's Shield is declined.
    check(submit(*c, "melee", 1) && (!c->snapshot().reaction_pending || submit(*c, "decline")) &&
              logged(*c, "Copy melts away.") && unit(*c, 1).dead,
          "A simulacrum melts away at 0 Hit Points");
}

void contingency_checks()
{
    const auto rules = module();
    const auto caster = with_spells(wizard(11), {"contingency", "false_life"});
    VitalState state{caster.hit_points, false, {}};
    const auto actions = rules->camp_actions(caster, state);
    check(std::any_of(actions.begin(), actions.end(),
                      [](const auto &a)
                      {
                          return a.id == "contingency_false_life_fight";
                      }),
          "Contingency offers each spell with each trigger");
    std::vector<CampTarget> party{{caster, state}};
    RandomState random{1};
    rules->use_party_camp_action(caster, state, party, "contingency_false_life_fight", random);
    const auto dummies = dummy_rules();
    const std::vector<std::string> gear{"quarterstaff"};
    auto c = dummies->create({{14, 7, std::vector<Terrain>(98)},
                              {{1,
                                "campaign-character",
                                "Wizard",
                                Side::party,
                                {1, 2},
                                dummies->character_profile(caster, gear).data,
                                state},
                               {98, "dummy", "Foe", Side::opposition, {9, 2}}}},
                             1);
    check(logged(*c, "Contingency: False Life takes effect on Wizard.") &&
              unit(*c, 1).temporary_hp.amount > 0,
          "The stored spell takes effect as the fight starts");
}

void sculpt_checks()
{
    const auto rules = dummy_rules();
    for (const unsigned level : {5u, 6u})
    {
        auto c = field(*rules, with_spells(wizard(level), {"fireball"}),
                       {{2, "dummy", "Ally", Side::party, {8, 1}},
                        {98, "dummy", "Foe", Side::opposition, {8, 3}}},
                       1);
        check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
                  submit(*c, "area_cast"),
              "Fireball over an ally");
        check(logged(*c, "Ally is spared by Sculpt Spells.") == (level == 6),
              "Sculpt Spells from level 6");
    }
}

// The damage on each log line holding `text`, as "Foe takes 13 Acid damage
// from Acid Arrow." reports it.
std::vector<int> damage_taken(const CombatSession &c, std::string_view text)
{
    std::vector<int> amounts;
    for (const auto &line : c.snapshot().log())
    {
        const auto at = line.find(" takes ");
        if (line.find(text) != std::string::npos && at != std::string::npos)
        {
            amounts.push_back(std::stoi(line.substr(at + 7)));
        }
    }
    return amounts;
}

// Whether every amount is at most `most`, and some exceed `fewer_dice_most`,
// the most the spell's own slot could deal.
bool grew(const std::vector<int> &amounts, int fewer_dice_most, int most)
{
    return std::all_of(amounts.begin(), amounts.end(),
                       [&](int amount)
                       {
                           return amount <= most;
                       }) &&
           std::any_of(amounts.begin(), amounts.end(),
                       [&](int amount)
                       {
                           return amount > fewer_dice_most;
                       });
}

// Casting from higher slots (SRD 5.2.1's "Using a Higher-Level Spell Slot"):
// one more ray or creature, or one more die, for each level above the spell's,
// kept for the damage that comes later.
void higher_slot_checks()
{
    const auto rules = dummy_rules();
    const auto book = with_spells(wizard(9), {"scorching_ray", "hold_person", "acid_arrow",
                                              "flaming_sphere", "dragons_breath", "bestow_curse"});
    const std::vector<Participant> foes{{98, "dummy", "Foe", Side::opposition, {2, 2}},
                                        {99, "dummy", "Second Foe", Side::opposition, {3, 3}}};
    {
        auto c = field(*rules, book, foes, 1);
        check(offered(*c, "bestow_curse_strength_5", 98) &&
                  !offered(*c, "bestow_curse_strength_4", 98),
              "Bestow Curse is offered from a level-5 slot, which needs no Concentration");
        unsigned rays = 0;
        while (offered(*c, "scorching_ray_3", 98) && rays < 9)
        {
            check(submit(*c, "scorching_ray_3", 98), "Aim a ray");
            ++rays;
        }
        const auto log = c->snapshot().log();
        check(rays == 4 &&
                  std::count_if(log.begin(), log.end(),
                                [](const auto &line)
                                {
                                    return line.starts_with("Wizard -> Foe");
                                }) == 4 &&
                  slots(*c, 3) == 2,
              "Scorching Ray from a level-3 slot fires four rays");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(submit(*c, "hold_person_3", 98) && submit(*c, "hold_person_3", 99) &&
                  logged(*c, "Foe Wisdom save") && logged(*c, "Second Foe Wisdom save"),
              "Hold Person from a level-3 slot holds two Humanoids");
    }
    std::vector<int> burns, spheres, breaths;
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        auto c = field(*rules, book, foes, seed);
        check(submit(*c, "acid_arrow_4", 98) && submit(*c, "end"),
              "Acid Arrow from a level-4 slot");
        to_turn(*c, 1);
        const auto burned = damage_taken(*c, "from Acid Arrow");
        burns.insert(burns.end(), burned.begin(), burned.end());
        check(submit(*c, "flaming_sphere_4") && submit_cell(*c, "area_move", Cell{3, 2}) &&
                  submit(*c, "area_cast") && submit(*c, "end"),
              "Flaming Sphere from a level-4 slot");
        to_turn(*c, 1);
        const auto sphered = damage_taken(*c, "from the Flaming Sphere");
        spheres.insert(spheres.end(), sphered.begin(), sphered.end());
        check(submit(*c, "dragons_breath_fire_3", 1) && submit(*c, "end"),
              "Dragon's Breath from a level-3 slot");
        to_turn(*c, 1);
        check(offered(*c, "dragons_breath_exhale_fire") &&
                  submit(*c, "dragons_breath_exhale_fire") &&
                  submit_cell(*c, "area_move", Cell{2, 2}) && submit(*c, "area_cast"),
              "The breath keeps the type chosen from the higher slot");
        const auto breathed = damage_taken(*c, "Fire damage.");
        breaths.insert(breaths.end(), breathed.begin(), breathed.end());
    }
    check(grew(burns, 8, 16), "Acid Arrow's burn deals 4d4 from a level-4 slot");
    check(grew(spheres, 12, 24), "Flaming Sphere deals 4d6 from a level-4 slot");
    check(grew(breaths, 18, 24),
          "Dragon's Breath exhales 4d6 from a level-3 slot, not the caster's cantrip dice");

    // One more creature for each slot level above the spell's.
    {
        auto c = field(*rules,
                       with_spells(wizard(9), {"hideous_laughter", "blindness", "invisibility",
                                               "shatter", "mind_spike"}),
                       foes, 1);
        check(offered(*c, "shatter_3") && offered(*c, "mind_spike_3", 98),
              "Shatter and Mind Spike add a die from a higher slot");
        check(submit(*c, "hideous_laughter_2", 98) && submit(*c, "hideous_laughter_2", 99) &&
                  logged(*c, "Foe Wisdom save") && logged(*c, "Second Foe Wisdom save"),
              "Hideous Laughter from a level-2 slot takes two creatures");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(submit(*c, "blindness_3", 98) && submit(*c, "blindness_3", 99) &&
                  logged(*c, "Foe Constitution save") && logged(*c, "Second Foe Constitution save"),
              "Blindness from a level-3 slot takes two creatures");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(offered(*c, "invisibility_3", 1) && submit(*c, "invisibility_3", 1) &&
                  offered(*c, "invisibility_3"),
              "Invisibility from a level-3 slot waits for a second creature");
    }

    // Summoned stat blocks, Animate Objects and the Globe grow with the slot.
    const auto high = field(
        *rules,
        with_spells(wizard(13), {"summon_dragon", "animate_objects", "globe_of_invulnerability"}),
        foes, 1);
    check(offered(*high, "summon_dragon_fire_6") && offered(*high, "animate_objects_6") &&
              offered(*high, "globe_of_invulnerability_7"),
          "Spells that grow with the slot are offered from higher slots");
}

// Dispel Magic also ends Darkness (LEVELS-1, with Daylight cut), as it does
// any spell's area over the square it is aimed at.
void dispel_darkness_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(5), {"darkness", "dispel_magic"}),
                   {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(submit(*c, "darkness") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast") && !c->snapshot().obscured.empty(),
          "Darkness over the foe");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "dispel_magic") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast") && c->snapshot().obscured.empty(),
          "Dispel Magic ends the Darkness");
}
} // namespace

int main()
{
    try
    {
        higher_slot_checks();
        dispel_darkness_checks();
        progression_checks();
        memorize_spell_checks();
        arcane_recovery_checks();
        fireball_checks();
        counterspell_checks();
        haste_checks();
        hypnotic_pattern_checks();
        fear_checks();
        cloud_checks();
        species_save_checks();
        vampiric_touch_checks();
        blink_checks();
        tiny_hut_checks();
        sculpt_checks();
        tentacles_checks();
        blight_and_charm_checks();
        confusion_checks();
        slow_checks();
        dimension_door_checks();
        fire_shield_checks();
        borrowed_attack_checks();
        invisibility_checks();
        ice_storm_checks();
        phantasmal_killer_checks();
        polymorph_checks();
        sphere_checks();
        vitriol_checks();
        wall_of_fire_checks();
        hound_and_elementals_checks();
        level_five_checks();
        cloudkill_checks();
        conjure_elemental_checks();
        summon_dragon_checks();
        animate_objects_checks();
        mislead_checks();
        telekinesis_checks();
        walls_checks();
        arcane_hand_checks();
        level_six_checks();
        eyebite_checks();
        flesh_and_dance_checks();
        globe_checks();
        magic_jar_checks();
        wall_of_ice_checks();
        contingency_checks();
        arcane_sword_checks();
        delayed_blast_checks();
        finger_of_death_checks();
        forcecage_checks();
        prismatic_and_gravity_checks();
        level_eight_checks();
        overchannel_checks();
        simulacrum_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Wizard level checks passed\n";
}
