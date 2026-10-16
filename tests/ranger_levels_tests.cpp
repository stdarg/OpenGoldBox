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

// A Ranger (Hunter) from level 5 to 15 (LEVELS-1, LEVELS-2): Extra Attack,
// Roving, Defensive Tactics, more free Hunter's Marks, level-2 to level-4
// slots and spells, Ensnaring Strike from a higher slot, Expertise, Tireless,
// Superior Hunter's Prey, Relentless Hunter, Nature's Veil and Superior
// Hunter's Defense.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
constexpr std::string_view tactics = "subclass:ranger:hunter:defensive_tactics";

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
    d.gender = "male";
    d.character_class = "ranger";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Ranger";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[1] = {{6, 6, 6, 1}, 3};
    d.spells =
        SpellChoices{{}, std::vector<std::string>{"cure_wounds", "ensnaring_strike"}, {}, {}};
    return d;
}

// A Ranger of the level, advanced through the rules' default level-ups, with
// the Defensive Tactics option `tactic` at level 7 and, at level 9, the
// level-3 spells `picks` among its new prepared spells.
Character ranger(unsigned level, std::string tactic = "escape_the_horde",
                 const std::vector<std::string> &picks = {})
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (choice.training.contains(std::string(tactics)))
            choice.training[std::string(tactics)] = {tactic};
        if (hero.sheet().level == 8 && !picks.empty())
        {
            auto spells = hero.sheet().prepared_spells;
            spells.insert(spells.end(), picks.begin(), picks.end());
            for (const auto &spell : choice.spells)
                if (spells.size() < choice.spells.size() &&
                    std::find(spells.begin(), spells.end(), spell) == spells.end())
                    spells.push_back(spell);
            choice.spells = spells;
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
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

// An AC 1 dummy and a brute that always hits for 1d4.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature beast 30 1000 10 30 2 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "type beast beast\n");
}

std::vector<std::string> log_lines(const CombatSession &c)
{
    return c.snapshot().log();
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = log_lines(c);
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

unsigned remaining(const CombatSession &c, std::string_view pool)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == pool)
            return resource.remaining;
    return 0;
}

// The Ranger beside a creature of `kind`, on the Ranger's turn.
std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::string kind, std::uint64_t seed)
{
    const std::vector<std::string> gear{"longsword"};
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
                           {{1,
                             "campaign-character",
                             "Ranger",
                             Side::party,
                             {1, 1},
                             rules.character_profile(hero.sheet(), gear).data},
                            {98, std::move(kind), "Foe", Side::opposition, {2, 1}}}},
                          seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    return c;
}

bool grants(const Character &hero, std::string_view id, int level)
{
    const auto &all = hero.sheet().grants;
    return std::any_of(all.begin(), all.end(),
                       [&](const auto &g)
                       {
                           return g.id == id && static_cast<int>(g.level) == level;
                       });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 15,
          "A Ranger reaches the game's cap of 15: " +
              std::to_string(party.member(id).character.sheet().level));
    const auto eight = ranger(8);
    check(!rules->advancement_options(ranger(15).sheet()).level, "Level 15 is the cap");
    check(grants(eight, "feature:extra_attack", 5) && grants(eight, "feature:roving", 6) &&
              grants(eight, "tactics:escape_the_horde", 7),
          "The level 5-7 features arrive at their levels");
    for (const auto &[level, count] :
         std::vector<std::pair<unsigned, unsigned>>{{4, 5}, {5, 6}, {7, 7}, {8, 7}})
        check(rules->spell_access(ranger(level).sheet()).prepared_choices == count,
              "Prepared spells by level: 5 at 4, 6 at 5, 7 at 7 and 8");
    const auto preparable = [&](const Character &hero, std::string_view spell)
    {
        const auto options =
            rules->spell_choice_options(hero.sheet(), SpellChoiceContext::long_rest);
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto &o)
                           {
                               return o.id == spell;
                           });
    };
    const auto five = ranger(5);
    check(!preparable(ranger(4), "barkskin"), "No level-2 spells at level 4");
    for (const auto *spell : {"aid", "barkskin", "gust_of_wind", "lesser_restoration",
                              "magic_weapon", "protection_from_poison", "silence", "spike_growth"})
        check(preparable(five, spell), std::string("A level-5 Ranger can prepare ") + spell);
    check(rules->character_profile(ranger(5).sheet(), {}).movement_feet == 30 &&
              rules->character_profile(ranger(6).sheet(), {}).movement_feet == 40,
          "Roving: Speed +10 feet from level 6");
    const auto dummies = dummy_rules();
    auto c = duel(*dummies, five, "dummy", 1);
    check(slots(*c, 1) == 4 && slots(*c, 2) == 2, "Four level-1 and two level-2 slots at level 5");
    check(slots(*duel(*dummies, ranger(7), "dummy", 1), 2) == 3,
          "Three level-2 slots from level 7");
    check(remaining(*duel(*dummies, ranger(4), "dummy", 1), "favored_enemy") == 2 &&
              remaining(*c, "favored_enemy") == 3,
          "Favored Enemy: a third free Hunter's Mark at level 5");
    const auto bytes = encode_campaign(party, nullptr, "ranger-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "ranger-levels", nullptr)
                     .party);
    check(encode_campaign(next, nullptr, "ranger-levels") == bytes,
          "A level-15 Ranger saves and reloads");
}

void high_progression_checks()
{
    const auto rules = module();
    const auto training = rules->advancement_options(ranger(8).sheet()).training;
    check(training.size() == 1 && training.front().id == "class:ranger:expertise:9" &&
              training.front().label == "Ranger Expertise" && training.front().count == 2,
          "Level 9 offers Expertise in two skills");
    const auto nine = ranger(9);
    check(std::count_if(nine.sheet().grants.begin(), nine.sheet().grants.end(),
                        [](const auto &g)
                        {
                            return g.source_id == "class:ranger:expertise:9" &&
                                   g.id.starts_with("expertise:");
                        }) == 2,
          "Level 9 gains Expertise in two skills");
    check(grants(ranger(10), "feature:tireless", 10) &&
              grants(ranger(11), "feature:superior_hunters_prey", 11),
          "Tireless at 10 and Superior Hunter's Prey at 11");
    for (const auto &[level, count] :
         std::vector<std::pair<unsigned, unsigned>>{{9, 9}, {10, 9}, {11, 10}, {12, 10}})
        check(rules->spell_access(ranger(level).sheet()).prepared_choices == count,
              "Prepared spells by level: 9 at 9 and 10, 10 at 11 and 12");
    const auto preparable = [&](const Character &hero, std::string_view spell)
    {
        const auto options =
            rules->spell_choice_options(hero.sheet(), SpellChoiceContext::long_rest);
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto &o)
                           {
                               return o.id == spell;
                           });
    };
    check(!preparable(ranger(8), "wind_wall"), "No level-3 spells at level 8");
    for (const auto *spell : {"conjure_animals", "dispel_magic", "plant_growth",
                              "protection_from_energy", "revivify", "wind_wall"})
        check(preparable(nine, spell), std::string("A level-9 Ranger can prepare ") + spell);
    const auto dummies = dummy_rules();
    auto c = duel(*dummies, nine, "dummy", 1);
    check(slots(*c, 3) == 2 && slots(*duel(*dummies, ranger(11), "dummy", 1), 3) == 3,
          "Two level-3 slots at 9, three at 11");
    check(remaining(*c, "favored_enemy") == 4, "Favored Enemy: four free Hunter's Marks at 9");
    const auto thirteen = ranger(13);
    check(grants(thirteen, "feature:relentless_hunter", 13) &&
              grants(ranger(14), "feature:natures_veil", 14) &&
              grants(ranger(15), "feature:superior_hunters_defense", 15),
          "Relentless Hunter at 13, Nature's Veil at 14, Superior Hunter's Defense at 15");
    check(rules->spell_access(thirteen.sheet()).prepared_choices == 11 &&
              rules->spell_access(ranger(15).sheet()).prepared_choices == 12,
          "Prepared spells: 11 at 13, 12 at 15");
    check(!preparable(ranger(12), "stoneskin"), "No level-4 spells at level 12");
    for (const auto *spell :
         {"conjure_woodland_beings", "dominate_beast", "freedom_of_movement", "stoneskin"})
        check(preparable(thirteen, spell), std::string("A level-13 Ranger can prepare ") + spell);
    auto t = duel(*dummies, thirteen, "dummy", 1);
    check(slots(*t, 4) == 1 && slots(*duel(*dummies, ranger(15), "dummy", 1), 4) == 2 &&
              remaining(*t, "favored_enemy") == 5,
          "One level-4 slot at 13, two at 15; five free Hunter's Marks");
}

void tireless_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*duel(*rules, ranger(9), "dummy", 1), "tireless"), "No Tireless before 10");
    auto c = duel(*rules, ranger(10), "dummy", 1);
    const auto uses = remaining(*c, "tireless");
    check(uses >= 1 && offered(*c, "tireless"), "Tireless is offered at level 10");
    check(submit(*c, "tireless", 1) && unit(*c, 1).temporary_hp.amount > 0 &&
              remaining(*c, "tireless") == uses - 1 && !offered(*c, "melee"),
          "Tireless spends the Action and a use for Temporary Hit Points");
    check(logged(*c, "Ranger uses Tireless."), "The log names Tireless");
}

// The Ranger beside a marked dummy, a second dummy 10 feet from it and a
// third 40 feet from it.
std::unique_ptr<CombatSession> hunt(const RulesModule &rules, const Character &hero,
                                    std::uint64_t seed)
{
    const std::vector<std::string> gear{"longsword"};
    auto c = rules.create({{12, 3, std::vector<Terrain>(36)},
                           {{1,
                             "campaign-character",
                             "Ranger",
                             Side::party,
                             {1, 1},
                             rules.character_profile(hero.sheet(), gear).data},
                            {97, "dummy", "Quarry", Side::opposition, {2, 1}},
                            {98, "dummy", "Second", Side::opposition, {4, 1}},
                            {99, "dummy", "Far", Side::opposition, {10, 1}}}},
                          seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 8; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    check(submit(*c, "hunters_mark_free", 97), "Mark the quarry");
    return c;
}

void superior_prey_checks()
{
    const auto rules = dummy_rules();
    bool struck = false;
    for (std::uint64_t seed = 1; seed < 40 && !struck; ++seed)
    {
        auto ten = hunt(*rules, ranger(10), seed);
        check(submit(*ten, "melee", 97) && !offered(*ten, "superior_hunters_prey"),
              "No Superior Hunter's Prey before 11");
        auto c = hunt(*rules, ranger(11), seed);
        check(submit(*c, "melee", 97), "Attack the quarry");
        if (!logged(*c, "Force damage from Hunter's Mark"))
            continue;
        const auto commands = c->legal_commands();
        const auto targets = std::count_if(commands.begin(), commands.end(),
                                           [](const auto &o)
                                           {
                                               return o.verb == "superior_hunters_prey";
                                           });
        check(targets == 1 && std::any_of(commands.begin(), commands.end(),
                                          [](const auto &o)
                                          {
                                              return o.verb == "superior_hunters_prey" &&
                                                     o.target == 98;
                                          }),
              "Only the creature within 30 feet of the quarry is offered");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the chance");
        check(submit(*c, "superior_hunters_prey", 98) && logged(*c, "Second takes ") &&
                  logged(*c, " Force damage from Superior Hunter's Prey.") &&
                  !offered(*c, "superior_hunters_prey"),
              "The mark's damage strikes the second creature, once a turn");
        struck = true;
    }
    check(struck, "A seed where the quarry is hit");
}

void extra_attack_checks()
{
    const auto rules = dummy_rules();
    auto four = duel(*rules, ranger(4), "dummy", 1);
    check(submit(*four, "melee", 98) && !offered(*four, "melee"), "One attack at level 4");
    auto five = duel(*rules, ranger(5), "dummy", 1);
    check(submit(*five, "melee", 98) && offered(*five, "melee"),
          "Extra Attack: a second attack at level 5");
}

// The brute's Opportunity Attack as the Ranger walks away from it.
std::string opportunity_line(const Character &hero)
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, hero, "brute", 1);
    Command away;
    for (const auto &command : c->legal_commands())
        if (command.verb == "move" && command.destination == Cell{0, 1})
            away = command;
    check(away.actor == 1 && c->submit(away) && c->snapshot().reaction_pending,
          "Stepping away from the brute provokes it");
    check(submit(*c, "opportunity", 1), "The brute makes its Opportunity Attack");
    for (const auto &line : log_lines(*c))
        if (line.starts_with("Foe -> Ranger"))
            return line;
    throw std::runtime_error("No Opportunity Attack logged");
}

void escape_the_horde_checks()
{
    check(opportunity_line(ranger(7)).find("(disadvantage)") != std::string::npos,
          "Escape the Horde: the Opportunity Attack has Disadvantage");
    check(opportunity_line(ranger(6)).find("(disadvantage)") == std::string::npos,
          "No Disadvantage before Defensive Tactics");
}

// A level-5 Barbarian attacks the Ranger twice; true when the first attack
// hit and the second had Disadvantage. (A Champion's critical hit would stop
// for its move.)
bool second_attack_disadvantaged(const Character &hero, std::uint64_t seed, bool &first_hit)
{
    const auto rules = module();
    CharacterDraft d = draft();
    d.character_class = "barbarian";
    d.name = "Barbarian";
    d.spells = {};
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    Character barbarian(*srd5::character_rules(), d, {});
    VitalState life{barbarian.sheet().hit_points, false, {}};
    while (barbarian.sheet().level < 5)
        check(barbarian.advance(*rules, life, rules->default_advancement(barbarian.sheet())),
              "The Barbarian levels up");
    const std::vector<std::string> gear{"longsword"};
    auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
                            {{1,
                              "campaign-character",
                              "Ranger",
                              Side::opposition,
                              {1, 1},
                              rules->character_profile(hero.sheet(), gear).data},
                             {2,
                              "campaign-character",
                              "Barbarian",
                              Side::party,
                              {2, 1},
                              rules->character_profile(barbarian.sheet(), gear).data}}},
                           seed);
    for (unsigned turns = 0; c->snapshot().actor != 2 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Barbarian's turn");
    check(submit(*c, "melee", 1) && submit(*c, "melee", 1), "The Barbarian attacks twice");
    std::vector<std::string> attacks;
    for (const auto &line : log_lines(*c))
        if (line.starts_with("Barbarian -> Ranger"))
            attacks.push_back(line);
    check(attacks.size() == 2, "Two attack lines");
    first_hit = attacks[0].find(" misses.") == std::string::npos;
    return attacks[1].find("(disadvantage)") != std::string::npos;
}

void multiattack_defense_checks()
{
    const auto defender = ranger(7, "multiattack_defense");
    bool hit_seen = false, miss_seen = false;
    for (std::uint64_t seed = 1; seed < 60 && !(hit_seen && miss_seen); ++seed)
    {
        bool first_hit = false;
        const bool disadvantaged = second_attack_disadvantaged(defender, seed, first_hit);
        check(disadvantaged == first_hit,
              "Multiattack Defense: Disadvantage on the second attack exactly when the first hit");
        (first_hit ? hit_seen : miss_seen) = true;
    }
    check(hit_seen && miss_seen, "Seeds with a first hit and a first miss");
    bool first_hit = false;
    for (std::uint64_t seed = 1; seed < 60; ++seed)
        check(!second_attack_disadvantaged(ranger(7), seed, first_hit),
              "Escape the Horde gives no Disadvantage to ordinary attacks");
}

void tactics_rest_checks()
{
    const auto rules = module();
    auto sheet = ranger(7).sheet();
    const auto offered_rest = rules->rest_training_options(sheet, RestKind::short_rest);
    const auto group = std::find_if(offered_rest.begin(), offered_rest.end(),
                                    [](const auto &o)
                                    {
                                        return o.group.id == tactics;
                                    });
    check(group != offered_rest.end() &&
              group->selected == std::vector<std::string>{"escape_the_horde"},
          "A Short Rest offers Defensive Tactics, Escape the Horde chosen");
    rules->replace_rest_training(sheet, std::vector<std::string>{"multiattack_defense"});
    check(std::any_of(sheet.grants.begin(), sheet.grants.end(),
                      [](const auto &g)
                      {
                          return g.id == "tactics:multiattack_defense";
                      }) &&
              std::none_of(sheet.grants.begin(), sheet.grants.end(),
                           [](const auto &g)
                           {
                               return g.id == "tactics:escape_the_horde";
                           }),
          "The rest swaps Escape the Horde for Multiattack Defense");
    check(rules->rest_training_options(ranger(6).sheet(), RestKind::short_rest).size() == 1,
          "Before level 7 a rest offers only the Hunter's Prey");
}

// Ensnaring Strike from a level-2 slot: its vines deal 2d6.
void ensnaring_upcast_checks()
{
    const auto rules = dummy_rules();
    auto four = duel(*rules, ranger(4), "dummy", 1);
    check(submit(*four, "melee", 98) && offered(*four, "ensnaring_strike") &&
              !offered(*four, "ensnaring_strike_2"),
          "No level-2 slot at level 4");
    bool above_six = false;
    for (std::uint64_t seed = 1; seed < 80 && !above_six; ++seed)
    {
        auto c = duel(*rules, ranger(5), "dummy", seed);
        check(submit(*c, "melee", 98) && offered(*c, "ensnaring_strike_2"),
              "Ensnaring Strike is offered from a level-2 slot");
        check(submit(*c, "ensnaring_strike_2", 98) && slots(*c, 2) == 1 && slots(*c, 1) == 4,
              "Ensnaring Strike spends the level-2 slot");
        if (!logged(*c, "Foe is Restrained."))
            continue;
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the vines' dice");
        check(submit(*c, "end"), "End the Ranger's turn");
        for (const auto &line : log_lines(*c))
            if (line.starts_with("Foe takes ") &&
                line.ends_with(" Piercing damage from the vines."))
                above_six = above_six || std::stoi(line.substr(10)) > 6;
    }
    check(above_six, "The vines deal more than one d6 could");
}

// A level-9 Ranger with the level-3 spells to test, 40 feet from a dummy, on
// its turn.
std::unique_ptr<CombatSession> field(const RulesModule &rules, std::uint64_t seed,
                                     std::string weapon = "longsword",
                                     std::vector<bool> plants = {})
{
    const auto hero = ranger(9, "escape_the_horde", {"conjure_animals", "wind_wall"});
    auto sheet = hero.sheet();
    sheet.prepared_spells[0] = "protection_from_energy";
    sheet.prepared_spells[1] = "plant_growth";
    const std::vector<std::string> gear{std::move(weapon)};
    auto c = rules.create({{12, 5, std::vector<Terrain>(60), std::move(plants)},
                           {{1,
                             "campaign-character",
                             "Ranger",
                             Side::party,
                             {1, 2},
                             rules.character_profile(sheet, gear).data},
                            {98, "dummy", "Foe", Side::opposition, {9, 2}}}},
                          seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    return c;
}

bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.destination == cell)
            return c.submit(command);
    return false;
}

// Aims an area spell at a square and casts it.
bool cast_at(CombatSession &c, std::string_view verb, Cell cell)
{
    return submit(c, verb) && submit_cell(c, "area_move", cell) && submit(c, "area_cast");
}

void protection_from_energy_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, 1);
    for (const auto *type : {"acid", "cold", "fire", "lightning", "thunder"})
        check(offered(*c, std::string("protection_from_energy_") + type),
              std::string("Protection from Energy offers ") + type);
    check(submit(*c, "protection_from_energy_fire", 1) &&
              logged(*c, "Ranger gains Resistance to Fire damage.") && slots(*c, 3) == 1,
          "Protection from Energy spends a level-3 slot for Fire Resistance");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the ward");
}

void conjure_animals_checks()
{
    const auto rules = dummy_rules();
    bool struck = false;
    for (std::uint64_t seed = 1; seed < 40 && !struck; ++seed)
    {
        auto c = field(*rules, seed);
        check(cast_at(*c, "conjure_animals", Cell{4, 2}) && slots(*c, 3) == 1 &&
                  c->snapshot().spirits.size() == 4 &&
                  logged(*c, "A pack of spirit animals appears."),
              "Conjure Animals sets a pack of two squares by two");
        check(!offered(*c, "move_spirit_pack"), "The pack moves only once the Ranger has moved");
        Command step;
        for (const auto &command : c->legal_commands())
            if (command.verb == "move" && command.destination == Cell{2, 2})
                step = command;
        check(step.actor == 1 && c->submit(step) && offered(*c, "move_spirit_pack"),
              "After moving, the Ranger may move the pack");
        const auto before = c->snapshot().spirits;
        check(submit(*c, "move_spirit_pack", 98) && c->snapshot().spirits != before &&
                  !offered(*c, "move_spirit_pack"),
              "The pack moves beside the dummy, once a turn");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the pack");
        struck = logged(*c, "Foe takes ") && logged(*c, " Slashing damage from the spirit pack.");
    }
    check(struck, "The pack's spirits strike a creature that fails its save");
}

void wind_wall_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, 1, "longbow");
    check(offered(*c, "wind_wall_vertical") && offered(*c, "wind_wall_horizontal") &&
              offered(*c, "wind_wall_rising") && offered(*c, "wind_wall_falling"),
          "Wind Wall offers four directions");
    check(submit(*c, "wind_wall_vertical") && c->snapshot().area_targeting->cells.size() == 5,
          "A vertical wall fills the column");
    check(submit_cell(*c, "area_move", Cell{5, 2}) && submit(*c, "area_cast") &&
              c->snapshot().walls.size() == 5 && slots(*c, 3) == 1,
          "Wind Wall rises");
    check(submit(*c, "end"), "End the Ranger's turn");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Back to the Ranger");
    for (unsigned shots = 0; shots < 2 && offered(*c, "ranged"); ++shots)
        check(submit(*c, "ranged", 98), "Shoot across the wall");
    check(logged(*c, "The wind deflects the shot.") && !logged(*c, "Foe takes "),
          "The wind turns every arrow shot across it");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wall");
}

void plant_growth_checks()
{
    const auto rules = dummy_rules();
    auto bare = field(*rules, 1);
    check(cast_at(*bare, "plant_growth", Cell{6, 2}) && logged(*bare, "No plants grow there."),
          "Without plants, Overgrowth does nothing");
    std::vector<bool> plants(60);
    for (int y = 0; y < 5; ++y)
        for (int x = 6; x < 12; ++x)
            plants[static_cast<std::size_t>(y * 12 + x)] = true;
    auto c = field(*rules, 1, "longsword", plants);
    check(cast_at(*c, "plant_growth", Cell{6, 2}) &&
              logged(*c, "The plants grow thick and overgrown.") && slots(*c, 3) == 1,
          "Plant Growth's Overgrowth");
    const auto board = c->snapshot().battlefield;
    check(board.at({8, 2}) == Terrain::overgrown && board.at({5, 2}) == Terrain::open,
          "Only squares with plants are overgrown");
    check(c->movement_reach(98).size() <
              field(*rules, 1, "longsword", plants)->movement_reach(98).size(),
          "The overgrowth slows the dummy");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the overgrowth");
}

// A Ranger of the level with `spell` prepared, beside a brute that always
// hits and a beast 10 feet from it, on the Ranger's turn.
std::unique_ptr<CombatSession> wilds(const RulesModule &rules, unsigned level, std::string spell,
                                     std::uint64_t seed)
{
    auto sheet = ranger(level).sheet();
    auto &prepared = sheet.prepared_spells;
    if (std::find(prepared.begin(), prepared.end(), spell) == prepared.end())
        prepared[0] = std::move(spell);
    const std::vector<std::string> gear{"longsword"};
    auto c = rules.create({{12, 5, std::vector<Terrain>(60)},
                           {{1,
                             "campaign-character",
                             "Ranger",
                             Side::party,
                             {1, 2},
                             rules.character_profile(sheet, gear).data},
                            {98, "brute", "Foe", Side::opposition, {2, 2}},
                            {99, "beast", "Wolf", Side::opposition, {3, 3}}}},
                          seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    return c;
}

// The brute's next attack on the Ranger, from the Ranger's turn.
void brute_attacks(CombatSession &c)
{
    for (unsigned turns = 0; c.snapshot().actor != 98 && turns < 6; ++turns)
        check(submit(c, "end"), "Reach the brute's turn");
    check(submit(c, "melee", 1), "The brute attacks the Ranger");
}

void relentless_hunter_checks()
{
    const auto rules = dummy_rules();
    for (const unsigned level : {12u, 13u})
    {
        auto c = wilds(*rules, level, "cure_wounds", 1);
        check(submit(*c, "hunters_mark_free", 98), "Mark the brute");
        brute_attacks(*c);
        const bool saved = logged(*c, "Ranger Constitution save:");
        check(saved == (level == 12),
              level == 12 ? "Damage tests the Ranger's Concentration at 12"
                          : "Relentless Hunter: no Concentration save for Hunter's Mark at 13");
    }
}

void natures_veil_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*wilds(*rules, 13, "stoneskin", 1), "natures_veil"), "No Nature's Veil at 13");
    auto c = wilds(*rules, 14, "stoneskin", 1);
    const auto uses = remaining(*c, "natures_veil");
    check(uses >= 1 && submit(*c, "natures_veil", 1) && remaining(*c, "natures_veil") == uses - 1 &&
              !offered(*c, "natures_veil"),
          "Nature's Veil spends the Bonus Action and a use");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the veil");
    brute_attacks(*c);
    bool disadvantaged = false;
    for (const auto &line : log_lines(*c))
        if (line.starts_with("Foe -> Ranger"))
            disadvantaged = line.find("(disadvantage)") != std::string::npos;
    check(disadvantaged, "An attack on the veiled Ranger has Disadvantage");
}

void hunters_defense_checks()
{
    const auto rules = dummy_rules();
    auto fourteen = wilds(*rules, 14, "stoneskin", 1);
    brute_attacks(*fourteen);
    check(!offered(*fourteen, "superior_defense"), "No Superior Hunter's Defense at 14");
    auto c = wilds(*rules, 15, "stoneskin", 1);
    brute_attacks(*c);
    check(c->snapshot().reaction_pending && offered(*c, "superior_defense"),
          "A hit asks the Ranger about Superior Hunter's Defense");
    check(submit(*c, "superior_defense", 0) &&
              logged(*c, "Ranger uses Superior Hunter's Defense: Resistance to ") &&
              !unit(*c, 1).reaction,
          "The Reaction gives Resistance to the hit's type");
}

void stoneskin_checks()
{
    const auto rules = dummy_rules();
    auto c = wilds(*rules, 13, "stoneskin", 1);
    check(submit(*c, "stoneskin", 1) && logged(*c, "Ranger gains Stoneskin.") &&
              slots(*c, 4) == 0 && !offered(*c, "stoneskin"),
          "Stoneskin spends the level-4 slot");
}

void woodland_beings_checks()
{
    const auto rules = dummy_rules();
    bool struck = false;
    for (std::uint64_t seed = 1; seed < 40 && !struck; ++seed)
    {
        auto sheet = ranger(13).sheet();
        auto &prepared = sheet.prepared_spells;
        if (std::find(prepared.begin(), prepared.end(), "conjure_woodland_beings") ==
            prepared.end())
            prepared[0] = "conjure_woodland_beings";
        const std::vector<std::string> gear{"longsword"};
        auto c = rules->create({{12, 5, std::vector<Terrain>(60)},
                                {{1,
                                  "campaign-character",
                                  "Ranger",
                                  Side::party,
                                  {1, 2},
                                  rules->character_profile(sheet, gear).data},
                                 {98, "dummy", "Foe", Side::opposition, {7, 2}}}},
                               seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
            check(submit(*c, "end"), "Reach the Ranger's turn");
        check(submit(*c, "conjure_woodland_beings", 1) && slots(*c, 4) == 0 &&
                  logged(*c, "Woodland spirits flit around Ranger.") &&
                  !c->snapshot().spirits.empty(),
              "Conjure Woodland Beings surrounds the Ranger with spirits");
        check(offered(*c, "woodland_disengage"), "Disengage as a Bonus Action while it lasts");
        Command step;
        for (const auto &command : c->legal_commands())
            if (command.verb == "move" && command.destination == Cell{5, 2})
                step = command;
        check(step.actor == 1 && c->submit(step),
              "The Ranger carries its spirits toward the dummy");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the spirits");
        check(logged(*c, "Foe Wisdom save:"), "The spirits reaching the dummy force a save");
        struck = logged(*c, " Force damage from the woodland spirits.");
    }
    check(struck, "The woodland spirits deal Force damage");
}

void dominate_beast_checks()
{
    const auto rules = dummy_rules();
    bool dominated = false;
    for (std::uint64_t seed = 1; seed < 60 && !dominated; ++seed)
    {
        auto c = wilds(*rules, 13, "dominate_beast", seed);
        const auto commands = c->legal_commands();
        check(!std::any_of(commands.begin(), commands.end(),
                           [](const auto &o)
                           {
                               return o.verb == "dominate_beast" && o.target == 98;
                           }),
              "Dominate Beast is offered only against a Beast");
        check(submit(*c, "dominate_beast", 99), "Cast Dominate Beast on the wolf");
        if (!logged(*c, "Wolf is dominated by Ranger."))
            continue;
        dominated = true;
        check(unit(*c, 99).side == Side::party, "The dominated wolf fights for the party");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the domination");
        const auto order = c->snapshot().combatants;
        const auto place = [&](EntityId id)
        {
            return std::find_if(order.begin(), order.end(),
                                [&](const auto &a)
                                {
                                    return a.id == id;
                                }) -
                   order.begin();
        };
        check(place(99) == place(1) + 1, "The wolf takes its turns right after the Ranger");
        for (unsigned turns = 0; c->snapshot().actor != 99 && turns < 6; ++turns)
            check(submit(*c, "end"), "Reach the wolf's turn");
        check(c->snapshot().actor == 99 && offered(*c, "melee"), "The player commands the wolf");
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
            check(submit(*c, "end"), "Back to the Ranger");
        check(submit(*c, "hunters_mark_free", 98) && logged(*c, "Wolf is no longer dominated.") &&
                  unit(*c, 99).side == Side::opposition,
              "Another Concentration spell ends the domination");
    }
    check(dominated, "A seed where the wolf fails its save");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        high_progression_checks();
        tireless_checks();
        superior_prey_checks();
        extra_attack_checks();
        escape_the_horde_checks();
        multiattack_defense_checks();
        tactics_rest_checks();
        ensnaring_upcast_checks();
        protection_from_energy_checks();
        conjure_animals_checks();
        wind_wall_checks();
        plant_growth_checks();
        relentless_hunter_checks();
        natures_veil_checks();
        hunters_defense_checks();
        stoneskin_checks();
        woodland_beings_checks();
        dominate_beast_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Ranger level checks passed\n";
}
