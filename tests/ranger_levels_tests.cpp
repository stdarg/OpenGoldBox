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

// A Ranger (Hunter) from level 5 to 8 (LEVELS-1, LEVELS-2): Extra Attack,
// Roving, Defensive Tactics, more free Hunter's Marks, level-2 slots and
// spells, and Ensnaring Strike from a higher slot.
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
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds", "ensnaring_strike"}, {}, {}};
    return d;
}

// A Ranger of the level, advanced through the rules' default level-ups, with
// the Defensive Tactics option `tactic` at level 7.
Character ranger(unsigned level, std::string tactic = "escape_the_horde")
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (choice.training.contains(std::string(tactics)))
            choice.training[std::string(tactics)] = {tactic};
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
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

// An AC 1 dummy and a brute that always hits for 1d4.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

std::vector<std::string> log_lines(const CombatSession &c)
{
    return c.snapshot().log();
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = log_lines(c);
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
        {   {1, "campaign-character", "Ranger", Side::party, {1, 1},
                rules.character_profile(hero.sheet(), gear).data},
            {98, std::move(kind), "Foe", Side::opposition, {2, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    return c;
}

bool grants(const Character &hero, std::string_view id, int level)
{
    const auto &all = hero.sheet().grants;
    return std::any_of(all.begin(), all.end(), [&](const auto & g)
    {
        return g.id == id && static_cast<int>(g.level) == level;
    });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(64000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 8,
          "A Ranger stops at 8 until its level-3 spells exist");
    const auto eight = ranger(8);
    check(!rules->advancement_options(eight.sheet()).level, "Level 8 is the Ranger's cap for now");
    check(grants(eight, "feature:extra_attack", 5) && grants(eight, "feature:roving", 6) &&
          grants(eight, "tactics:escape_the_horde", 7), "The level 5-7 features arrive at their levels");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>
{
    {4, 5}, {5, 6}, {7, 7}, {8, 7}
})
    check(rules->spell_access(ranger(level).sheet()).prepared_choices == count,
          "Prepared spells by level: 5 at 4, 6 at 5, 7 at 7 and 8");
    const auto preparable = [&](const Character & hero, std::string_view spell)
    {
        const auto options = rules->spell_choice_options(hero.sheet(), SpellChoiceContext::long_rest);
        return std::any_of(options.preparation.begin(), options.preparation.end(),
                           [&](const auto & o)
        {
            return o.id == spell;
        });
    };
    const auto five = ranger(5);
    check(!preparable(ranger(4), "barkskin"), "No level-2 spells at level 4");
    for (const auto *spell : {"aid", "barkskin", "gust_of_wind", "lesser_restoration",
                              "magic_weapon", "protection_from_poison", "silence", "spike_growth"
                             })
        check(preparable(five, spell), std::string("A level-5 Ranger can prepare ") + spell);
    check(rules->character_profile(ranger(5).sheet(), {}).movement_feet == 30 &&
          rules->character_profile(ranger(6).sheet(), {}).movement_feet == 40,
          "Roving: Speed +10 feet from level 6");
    const auto dummies = dummy_rules();
    auto c = duel(*dummies, five, "dummy", 1);
    check(slots(*c, 1) == 4 && slots(*c, 2) == 2, "Four level-1 and two level-2 slots at level 5");
    check(slots(*duel(*dummies, ranger(7), "dummy", 1), 2) == 3, "Three level-2 slots from level 7");
    check(remaining(*duel(*dummies, ranger(4), "dummy", 1), "favored_enemy") == 2 &&
          remaining(*c, "favored_enemy") == 3, "Favored Enemy: a third free Hunter's Mark at level 5");
    const auto bytes = encode_campaign(party, nullptr, "ranger-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "ranger-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "ranger-levels") == bytes,
          "A level-8 Ranger saves and reloads");
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
        {   {1, "campaign-character", "Ranger", Side::opposition, {1, 1},
                rules->character_profile(hero.sheet(), gear).data},
            {2, "campaign-character", "Barbarian", Side::party, {2, 1},
                rules->character_profile(barbarian.sheet(), gear).data}
        }},
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
    const auto group = std::find_if(offered_rest.begin(), offered_rest.end(), [](const auto & o)
    {
        return o.group.id == tactics;
    });
    check(group != offered_rest.end() && group->selected == std::vector<std::string> {"escape_the_horde"},
          "A Short Rest offers Defensive Tactics, Escape the Horde chosen");
    rules->replace_rest_training(sheet, std::vector<std::string> {"multiattack_defense"});
    check(std::any_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id == "tactics:multiattack_defense";
    }) && std::none_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id == "tactics:escape_the_horde";
    }), "The rest swaps Escape the Horde for Multiattack Defense");
    check(rules->rest_training_options(ranger(6).sheet(), RestKind::short_rest).size() == 1,
          "Before level 7 a rest offers only the Hunter's Prey");
}

// Ensnaring Strike from a level-2 slot: its vines deal 2d6.
void ensnaring_upcast_checks()
{
    const auto rules = dummy_rules();
    auto four = duel(*rules, ranger(4), "dummy", 1);
    check(submit(*four, "melee", 98) && offered(*four, "ensnaring_strike") &&
          !offered(*four, "ensnaring_strike_2"), "No level-2 slot at level 4");
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
            if (line.starts_with("Foe takes ") && line.ends_with(" Piercing damage from the vines."))
                above_six = above_six || std::stoi(line.substr(10)) > 6;
    }
    check(above_six, "The vines deal more than one d6 could");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        extra_attack_checks();
        escape_the_horde_checks();
        multiattack_defense_checks();
        tactics_rest_checks();
        ensnaring_upcast_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Ranger level checks passed\n";
}
