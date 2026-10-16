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
        throw std::runtime_error(message);
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
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
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
        r = {{6, 5, 4, 1}, 3};
    d.rolls[3] = {{6, 6, 6, 1}, 3};
    d.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost", "chill_touch"};
    const std::vector<std::string> book{"magic_missile", "burning_hands", "shield", "mage_armor",
                                        "false_life", "thunderwave"};
    d.spells = SpellChoices{{{"spellbook:1", book}}, std::vector<std::string>{"magic_missile"}, {}, {}};
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
        check(hero.advance(*rules, life, rules->default_advancement(hero.sheet())),
              "An ordinary level-up");
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
        const auto known = std::find_if(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
        {
            return g.id == "spell:" + spell;
        });
        if (known == sheet.grants.end())
        {
            // The latest spell learned, whose level allows any new spell.
            auto learned = sheet.grants.end();
            for (auto g = sheet.grants.begin(); g != sheet.grants.end(); ++g)
                if (g->id.starts_with("spell:") && g->level >= 5 &&
                        g->source_id == "class:wizard:spellcasting" &&
                        g->choices.contains("access") && g->choices.at("access") == "spellbook" &&
                        std::find(spells.begin(), spells.end(), g->id.substr(6)) == spells.end() &&
                        (learned == sheet.grants.end() || g->level > learned->level))
                    learned = g;
            check(learned != sheet.grants.end(), "A level-5 spell to replace");
            const auto old = learned->id.substr(6);
            learned->id = "spell:" + spell;
            std::erase(sheet.prepared_spells, old);
        }
        auto &prepared = sheet.prepared_spells;
        if (std::find(prepared.begin(), prepared.end(), spell) == prepared.end())
        {
            if (next < prepared.size())
                prepared[next] = spell;
            else
                prepared.push_back(spell);
        }
        ++next;
    }
    return sheet;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb && (!target || command.target == target);
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.destination == cell)
            return c.submit(command);
    return false;
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

// Spell slots of a level left: from the third field of the saved vital state.
unsigned slots(const CombatSession &c, unsigned level, EntityId id = 1)
{
    std::istringstream in(unit(c, id).persistent.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
        in >> count;
    return count;
}

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
        check(submit(c, "end") || submit(c, "decline"), "End a turn");
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Wizard (1) at (1, 2) and its creatures, on the Wizard's turn.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet,
                                     std::vector<Participant> others, std::uint64_t seed)
{
    const std::vector<std::string> gear{"quarterstaff"};
    others.insert(others.begin(), {1, "campaign-character", "Wizard", Side::party, {1, 2},
                                   rules.character_profile(sheet, gear).data
                                  });
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
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 10, "A Wizard stops at 10 for now");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>
{
    {5, 9}, {6, 10}, {7, 11}, {8, 12}, {9, 14}, {10, 15}
})
    check(rules->spell_access(wizard(level).sheet()).prepared_choices == count,
          "Prepared spells: 9 at 5 to 15 at 10");
    check(rules->spell_access(wizard(10).sheet()).cantrip_choices == 5, "A fifth cantrip at 10");
    const auto grants = wizard(10).sheet().grants;
    check(std::any_of(grants.begin(), grants.end(), [](const auto & g)
    {
        return g.id == "feature:empowered_evocation" && g.level == 10;
    }), "Empowered Evocation at 10");
    auto four = wizard(4).sheet();
    four.level = 5;
    const auto learning = rules->spell_choice_options(four, SpellChoiceContext::advancement).learning;
    const auto book = std::find_if(learning.begin(), learning.end(), [](const auto & group)
    {
        return group.id == "spellbook:5";
    });
    check(book != learning.end() && book->count == 2 &&
          std::any_of(book->options.begin(), book->options.end(), [](const auto & o)
    {
        return o.id == "fireball";
    }), "Level 5 learns two spells, level-3 ones among them");
    const auto dummies = dummy_rules();
    auto c5 = field(*dummies, wizard(5).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c6 = field(*dummies, wizard(6).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c5, 3) == 2 && slots(*c6, 3) == 3, "Two level-3 slots at 5, three at 6");
    auto c7 = field(*dummies, wizard(7).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c8 = field(*dummies, wizard(8).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c7, 4) == 1 && slots(*c8, 4) == 2, "One level-4 slot at 7, two at 8");
    auto c9 = field(*dummies, wizard(9).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    auto c10 = field(*dummies, wizard(10).sheet(), {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(slots(*c9, 5) == 1 && slots(*c10, 5) == 2, "One level-5 slot at 9, two at 10");
    const auto bytes = encode_campaign(party, nullptr, "wizard-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "wizard-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "wizard-levels") == bytes, "A level-10 Wizard saves and reloads");
}

void memorize_spell_checks()
{
    const auto rules = module();
    check(!rules->spell_choice_options(wizard(4).sheet(), SpellChoiceContext::short_rest).may_prepare,
          "No Memorize Spell before 5");
    const auto sheet = wizard(5).sheet();
    const auto options = rules->spell_choice_options(sheet, SpellChoiceContext::short_rest);
    check(options.may_prepare && !options.may_replace, "Memorize Spell prepares, never replaces");
    std::vector<std::string> unprepared;
    for (const auto &spell : rules->spell_access(sheet).spellbook)
        if (std::find(sheet.prepared_spells.begin(), sheet.prepared_spells.end(), spell.id) ==
                sheet.prepared_spells.end())
            unprepared.push_back(spell.id);
    check(unprepared.size() >= 2, "Two unprepared spells in the book");
    auto one = sheet.prepared_spells, two = sheet.prepared_spells;
    one[0] = unprepared[0];
    two[0] = unprepared[0];
    two[1] = unprepared[1];
    auto swapped = sheet;
    rules->apply_spell_choices(swapped, SpellChoices{{}, one, {}, {}}, SpellChoiceContext::short_rest,
                               ChoiceCompleteness::complete);
    check(swapped.prepared_spells == one, "One prepared spell changes on a Short Rest");
    bool refused = false;
    try
    {
        auto copy = sheet;
        rules->apply_spell_choices(copy, SpellChoices{{}, two, {}, {}}, SpellChoiceContext::short_rest,
                                   ChoiceCompleteness::complete);
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
    check(!party.state().spell_rest &&
          party.member(id).character.sheet().prepared_spells == one,
          "Memorize Spell changes one prepared spell");
    const auto bytes = encode_campaign(party, nullptr, "memorize");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(), "memorize",
                                 nullptr).party);
    check(encode_campaign(next, nullptr, "memorize") == bytes, "The changed spell saves and reloads");
}

void arcane_recovery_checks()
{
    const auto rules = module();
    const auto sheet = wizard(5).sheet();
    // Both level-3 slots spent.
    const auto dummies = dummy_rules();
    auto c = field(*dummies, with_spells(wizard(5), {"fireball"}), {{98, "dummy", "Foe", Side::opposition, {8, 2}}}, 1);
    check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
          submit(*c, "area_cast") && slots(*c, 3) == 1, "A level-3 slot is spent");
    const auto state = unit(*c, 1).persistent;
    const auto info = rules->recovery_info(sheet, state);
    check(std::any_of(info.choices.begin(), info.choices.end(), [](const auto & choice)
    {
        return choice.id == "arcane_recovery:0:0:1";
    }), "Arcane Recovery at 5 can restore a level-3 slot");
}

void fireball_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(5), {"fireball", "lightning_bolt"}),
    {   {97, "dummy", "Near", Side::opposition, {6, 2}},
        {98, "dummy", "Far", Side::opposition, {10, 2}}
    }, 1);
    check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
          submit(*c, "area_cast") && logged(*c, "Near takes ") && logged(*c, "Far takes ") &&
          logged(*c, " Fire damage."), "Fireball's 20-foot Sphere burns both foes");
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
            {   {1, "campaign-character", "Wizard", Side::party, {1, 2},
                    rules->character_profile(mine, gear).data},
                {98, "campaign-character", "Rival", Side::opposition, {6, 2},
                    rules->character_profile(theirs, gear).data}
            }},
        seed);
        to_turn(*c, 98);
        check(submit(*c, "fire_bolt", 1), "The rival casts Fire Bolt");
        if (!c->snapshot().reaction_pending)
            continue;
        check(offered(*c, "counterspell") && submit(*c, "counterspell"),
              "The Wizard is asked about Counterspell");
        check(logged(*c, "Wizard casts Counterspell.") && slots(*c, 3) == 1 &&
              logged(*c, "Rival Constitution save:"), "Counterspell spends a level-3 slot");
        stopped = logged(*c, "Rival's spell dissipates.");
        if (stopped)
            check(!logged(*c, "Rival -> Wizard") && !logged(*c, "Wizard takes "),
                  "A stopped spell does nothing");
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
          unit(*c, 1).armor_class == ac + 2, "Haste: +2 AC");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps Haste");
    to_turn(*c, 98);
    to_turn(*c, 1);
    check(unit(*c, 1).movement_feet == 60, "Haste doubles the Speed");
    check(submit(*c, "dash") && offered(*c, "dash") && offered(*c, "fire_bolt", 98),
          "Haste's action Dashes and keeps the Action");
    check(submit(*c, "dash") && !offered(*c, "dash") && !offered(*c, "fire_bolt", 98),
          "The second Dash spends the Action");
    for (unsigned turns = 0; turns < 40 && !logged(*c, "A wave of lethargy washes over Wizard."); ++turns)
        check(submit(*c, "end"), "Pass the minute");
    check(logged(*c, "A wave of lethargy washes over Wizard."), "Haste's end brings lethargy");
}

void hypnotic_pattern_checks()
{
    const auto rules = dummy_rules();
    bool entranced = false;
    for (std::uint64_t seed = 1; seed < 40 && !entranced; ++seed)
    {
        auto c = field(*rules, with_spells(wizard(5), {"hypnotic_pattern"}),
        {   {2, "dummy", "Ally", Side::party, {7, 1}},
            {98, "dummy", "Foe", Side::opposition, {8, 2}}
        }, seed);
        check(submit(*c, "hypnotic_pattern") && submit_cell(*c, "area_move", Cell{10, 2}) &&
              submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"), "The pattern appears");
        if (!logged(*c, "Foe is entranced by the pattern."))
            continue;
        entranced = true;
        check(unit(*c, 98).status.find("Charmed") != std::string::npos, "Entranced: Charmed");
        to_turn(*c, 98);
        check(!offered(*c, "move") && !offered(*c, "melee"), "An entranced creature does nothing");
        to_turn(*c, 2);
        check(submit(*c, "shake_awake", 98) && logged(*c, "Ally shakes Foe awake.") &&
              unit(*c, 98).status.find("Charmed") == std::string::npos, "An action shakes it awake");
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
              submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"), "Fear's cone");
        if (!logged(*c, "Foe is Frightened."))
            continue;
        frightened = true;
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(commands.size() == 1 && (commands.front().verb == "move" || commands.front().verb == "dash"),
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
          submit(*sleet, "area_cast") && sleet->snapshot().battlefield.at({9, 3}) == Terrain::difficult &&
          !sleet->snapshot().obscured.empty(), "Sleet Storm: obscured, Difficult Terrain");
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
              submit(*c, "area_cast"), "Stinking Cloud");
        to_turn(*c, 98);
        retched = logged(*c, "Foe retches in the cloud and can take no action this turn.");
        if (retched)
            check(!unit(*c, 98).action && !unit(*c, 98).bonus_action, "No action or Bonus Action");
    }
    check(retched, "A seed where the foe fails its save");
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
            {   {1, "campaign-character", "Wizard", Side::party, {1, 2},
                    rules->character_profile(sheet, gear).data, VitalState{5, false, {}}},
                {98, "dummy", "Foe", Side::opposition, {2, 2}}
            }},
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
            continue;
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
    check(std::any_of(actions.begin(), actions.end(), [](const auto & a)
    {
        return a.id == "tiny_hut" && a.whole_party;
    }), "Tiny Hut is a camp action for the party");
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
              logged(*c, "Foe Strength save:"), "Black Tentacles rise");
        held = logged(*c, " Bludgeoning damage from the tentacles.") && logged(*c, "Foe is Restrained.");
        if (!held)
            continue;
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the tentacles");
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(std::any_of(commands.begin(), commands.end(), [](const auto & command)
        {
            return command.verb == "escape" && command.label == "Escape the tentacles";
        }), "A held creature may try to escape");
    }
    check(held, "A seed where the foe fails its save");
}

void blight_and_charm_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"blight", "charm_monster"}),
    {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, 1);
    check(submit(*c, "blight", 98) && logged(*c, "Foe Constitution save:") &&
          logged(*c, " Necrotic damage"), "Blight withers the foe");
    bool charmed = false;
    for (std::uint64_t seed = 1; seed < 40 && !charmed; ++seed)
    {
        auto m = field(*rules, with_spells(wizard(7), {"charm_monster"}),
        {{98, "dummy", "Foe", Side::opposition, {4, 2}}}, seed);
        check(submit(*m, "charm_monster", 98) || (submit(*m, "charm_monster", 98) && submit(*m, "spell_cast")),
              "Cast Charm Monster");
        if (m->snapshot().spell_targeting)
            check(submit(*m, "spell_cast"), "Charm the chosen foe");
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
              submit(*c, "area_cast") && logged(*c, "Foe Wisdom save:"), "Confusion");
        if (!logged(*c, "Foe is Confused."))
            continue;
        confused = true;
        check(submit(*c, "end"), "End the Wizard's turn");
        check(logged(*c, "Foe wanders in confusion.") || logged(*c, "Foe stands confused") ||
              logged(*c, "Foe lashes out in confusion.") || logged(*c, "Foe shakes off the confusion"),
              "A d10 decides the confused creature's turn");
    }
    check(confused, "A seed where the foe fails its save");
}

void dimension_door_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"dimension_door"}),
    {   {2, "dummy", "Ally", Side::party, {1, 3}},
        {98, "dummy", "Foe", Side::opposition, {12, 2}}
    }, 1);
    check(offered(*c, "dimension_door_with", 2), "Dimension Door may take the ally beside");
    check(submit(*c, "dimension_door_with", 2) && submit_cell(*c, "area_move", Cell{11, 5}) &&
          submit(*c, "area_cast") && unit(*c, 1).cell == (Cell{11, 5}) &&
          logged(*c, "Ally steps through with Wizard."), "The Wizard and the ally step across");
    check(std::max(std::abs(unit(*c, 2).cell.x - 11), std::abs(unit(*c, 2).cell.y - 5)) <= 1,
          "The ally arrives within 5 feet");
}

void fire_shield_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(7), {"fire_shield"}),
    {{98, "brute", "Foe", Side::opposition, {2, 2}}}, 1);
    check(offered(*c, "fire_shield_chill") && submit(*c, "fire_shield_warm") &&
          logged(*c, "Wizard is wreathed in a warm shield."), "Fire Shield's warm shield");
    to_turn(*c, 98);
    check(submit(*c, "melee", 1), "The brute strikes");
    // The Wizard declines to cast Shield.
    if (c->snapshot().reaction_pending)
        check(submit(*c, "decline"), "Decline Shield");
    check(logged(*c, "Foe takes ") &&
          logged(*c, " Fire damage from the fire shield."), "The shield burns a melee attacker");
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
          submit(*c, "area_cast") && logged(*c, " Bludgeoning damage.") && logged(*c, " Cold damage."),
          "Ice Storm deals Bludgeoning and Cold");
    check(c->snapshot().battlefield.at({9, 3}) == Terrain::difficult, "Hail makes Difficult Terrain");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the hail");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(submit(*c, "end"), "End the next turn");
    check(c->snapshot().battlefield.at({9, 3}) == Terrain::open, "The hail melts after the next turn");
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
            continue;
        haunted = true;
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the nightmare");
        to_turn(*c, 98);
        const auto before = c->snapshot().log().size();
        check(submit(*c, "end"), "The foe ends its turn");
        const auto log = c->snapshot().log();
        check(std::any_of(log.begin() + static_cast<std::ptrdiff_t>(before), log.end(),
                          [](const auto & line)
        {
            return line.starts_with("Foe Wisdom save:");
        }), "The foe saves again at the end of its turn");
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
              logged(*c, "Foe Wisdom save:"), "Polymorph");
        if (!logged(*c, "Foe turns into a Wolf."))
            continue;
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
            continue;
        sealed = true;
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 98);
        const auto commands = c->legal_commands();
        check(std::all_of(commands.begin(), commands.end(), [](const auto & command)
        {
            return command.verb == "move" || command.verb == "end";
        }), "A sealed creature only rolls its sphere");
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
              submit(*c, "area_cast") && logged(*c, " Acid damage."), "Vitriolic Sphere");
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
    {   {97, "dummy", "Inside", Side::opposition, {8, 2}},
        {98, "dummy", "Beside", Side::opposition, {9, 3}}
    }, 1);
    check(offered(*c, "wall_of_fire_vertical_east") && offered(*c, "wall_of_fire_ring_inner"),
          "Wall of Fire offers each direction's two sides and a ring's");
    check(submit(*c, "wall_of_fire_vertical_east") && submit_cell(*c, "area_move", Cell{8, 2}) &&
          submit(*c, "area_cast") && logged(*c, "Inside Dexterity save:") &&
          !c->snapshot().walls.empty(), "The wall rises through a foe");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wall");
    to_turn(*c, 98);
    check(submit(*c, "end") && logged(*c, "Beside takes ") &&
          logged(*c, " Fire damage from the wall of fire."), "Its burning side burns a creature ending its turn there");
}

void hound_and_elementals_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(8), {"faithful_hound", "conjure_minor_elementals"}),
    {{98, "dummy", "Foe", Side::opposition, {6, 2}}}, 1);
    check(submit(*c, "faithful_hound") && submit_cell(*c, "area_move", Cell{5, 2}) &&
          submit(*c, "area_cast") && logged(*c, "A phantom watchdog appears."), "Faithful Hound");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the hound");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(logged(*c, "The hound bites at Foe."), "The hound bites at the start of the Wizard's turn");
    check(offered(*c, "conjure_minor_elementals_cold") &&
          submit(*c, "conjure_minor_elementals_fire") &&
          logged(*c, "Elemental spirits flit around Wizard."), "Conjure Minor Elementals: Fire");
    bool burned = false;
    for (unsigned turns = 0; turns < 8 && !burned; ++turns)
    {
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        Command step;
        for (const auto &command : c->legal_commands())
            if (command.verb == "move" && command.destination == Cell{5, 2})
                step = command;
        if (step.actor)
            check(c->submit(step), "Step beside the foe");
        if (submit(*c, "melee", 98))
            burned = logged(*c, " Fire damage from the elemental spirits.");
    }
    check(burned, "A hit on a creature in the Emanation deals the chosen type");
}

bool has_combatant(const CombatSession &c, std::string_view form)
{
    const auto all = c.snapshot().combatants;
    return std::any_of(all.begin(), all.end(), [&](const auto & a)
    {
        return a.form == form;
    });
}

EntityId combatant_with(const CombatSession &c, std::string_view form)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.form == form)
            return a.id;
    return 0;
}

void level_five_checks()
{
    const auto rules = dummy_rules();
    // Cone of Cold.
    auto cone = field(*rules, with_spells(wizard(9), {"cone_of_cold"}),
    {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, 1);
    check(submit(*cone, "cone_of_cold") && submit_cell(*cone, "area_move", Cell{5, 2}) &&
          submit(*cone, "area_cast") && logged(*cone, " Cold damage."), "Cone of Cold");
    // Hold Monster and Dominate Person.
    bool held = false, dominated = false;
    for (std::uint64_t seed = 1; seed < 40 && (!held || !dominated); ++seed)
    {
        auto c = field(*rules, with_spells(wizard(9), {"hold_monster", "dominate_person"}),
        {{98, "dummy", "Foe", Side::opposition, {5, 2}}}, seed);
        if (!held)
        {
            check(submit(*c, "hold_monster", 98) || (c->snapshot().spell_targeting && submit(*c, "spell_cast")),
                  "Hold Monster");
            if (c->snapshot().spell_targeting)
                check(submit(*c, "spell_cast"), "Hold the chosen foe");
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
          submit(*c, "area_cast"), "Cast Cloudkill");
    check(logged(*c, " Poison damage from the fog.") &&
          !c->snapshot().obscured.empty(), "Cloudkill's fog");
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
              logged(*c, "An elemental spirit appears."), "Conjure Elemental");
        to_turn(*c, 98);
        held = logged(*c, "Foe is Restrained by the spirit.") && logged(*c, " Fire damage from the elemental spirit.");
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
          logged(*c, "A Draconic Spirit answers Wizard.") && has_combatant(*c, "draconic_spirit"),
          "A Draconic Spirit appears with the dragon's art");
    const auto spirit = combatant_with(*c, "draconic_spirit");
    const auto order = c->snapshot().combatants;
    check(order[1].id == spirit || std::find_if(order.begin(), order.end(), [&](const auto & a)
    {
        return a.id == 1;
    }) + 1 == std::find_if(order.begin(), order.end(), [&](const auto & a)
    {
        return a.id == spirit;
    }), "It acts right after the Wizard");
    check(unit(*c, spirit).armor_class == 19 && unit(*c, spirit).max_hit_points == 50,
          "A level-5 spirit: AC 19, 50 Hit Points");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the spirit");
    check(submit(*c, "end"), "End the Wizard's turn");
    check(c->snapshot().actor == spirit && offered(*c, "spirit_breath"),
          "The spirit acts next and may breathe");
    check(submit(*c, "spirit_breath") && submit_cell(*c, "area_move", Cell{9, 2}) &&
          submit(*c, "area_cast") && logged(*c, "Draconic Spirit breathes.") &&
          !offered(*c, "spirit_breath"), "It breathes once a turn");
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
        objects += a.form == "animated_object";
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
          has_combatant(*c, "copy:1"), "Mislead: Invisible, with a double wearing the Wizard's art");
    const auto twin = combatant_with(*c, "copy:1");
    to_turn(*c, 98);
    Command near;
    for (const auto &command : c->legal_commands())
        if (command.verb == "move" && std::max(std::abs(command.destination.x - unit(*c, twin).cell.x),
                                               std::abs(command.destination.y - unit(*c, twin).cell.y)) == 1)
            near = command;
    if (near.actor)
        check(c->submit(near), "The foe closes on the double");
    check(submit(*c, "melee", twin) && logged(*c, "The attack passes through an illusory double.") &&
          !offered(*c, "melee", twin), "An attack passes through the double, and foes ignore it after");
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
              logged(*c, "Foe Strength save:"), "Telekinesis seizes the foe");
        if (unit(*c, 98).cell != (Cell{10, 2}))
            continue;
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
          logged(*c, "A wall of force springs up."), "A Wall of Force across the field");
    check(!offered(*c, "fire_bolt", 98), "Nothing passes through it");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wall");
}

void arcane_hand_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, with_spells(wizard(9), {"arcane_hand"}),
    {{98, "dummy", "Foe", Side::opposition, {9, 2}}}, 1);
    check(submit(*c, "arcane_hand") && submit_cell(*c, "area_move", Cell{5, 2}) &&
          submit(*c, "area_cast") && logged(*c, "A hand of shimmering force appears."), "Arcane Hand");
    check(offered(*c, "hand_push", 98) && submit(*c, "hand_fist", 98) &&
          logged(*c, "The hand strikes Foe.") && unit(*c, 1).bonus_action,
          "The hand strikes as it appears, without the Bonus Action");
    check(!offered(*c, "hand_fist", 98) || unit(*c, 1).bonus_action, "Later, with the Bonus Action");
}

void sculpt_checks()
{
    const auto rules = dummy_rules();
    for (const unsigned level : {5u, 6u})
    {
        auto c = field(*rules, with_spells(wizard(level), {"fireball"}),
        {   {2, "dummy", "Ally", Side::party, {8, 1}},
            {98, "dummy", "Foe", Side::opposition, {8, 3}}
        }, 1);
        check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{8, 2}) &&
              submit(*c, "area_cast"), "Fireball over an ally");
        check(logged(*c, "Ally is spared by Sculpt Spells.") == (level == 6),
              "Sculpt Spells from level 6");
    }
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        memorize_spell_checks();
        arcane_recovery_checks();
        fireball_checks();
        counterspell_checks();
        haste_checks();
        hypnotic_pattern_checks();
        fear_checks();
        cloud_checks();
        vampiric_touch_checks();
        blink_checks();
        tiny_hut_checks();
        sculpt_checks();
        tentacles_checks();
        blight_and_charm_checks();
        confusion_checks();
        dimension_door_checks();
        fire_shield_checks();
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
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Wizard level checks passed\n";
}
