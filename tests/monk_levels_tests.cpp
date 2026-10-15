#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

// A Monk (Open Hand) from level 5 to 9 (LEVELS-1): Extra Attack, the d8
// Martial Arts die, Unarmored Movement, Stunning Strike, Wholeness of Body and
// Evasion.
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

CharacterDraft draft(std::string klass)
{
    CharacterDraft d;
    d.race = "dwarf";
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Veteran";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    const auto rules = srd5::character_rules();
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : rules->training_options(d))
            if (!d.training.contains(group.id))
            {
                for (unsigned i = 0; i < group.count; ++i)
                    d.training[group.id].push_back(group.options.at(i).id);
                filled = true;
                break;
            }
    }
    return d;
}



// A Monk of the level, advanced through the rules' ordinary level-ups.
Character monk(unsigned level)
{
    Character hero(*srd5::character_rules(), draft("monk"), {});
    VitalState life;
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
        check(hero.advance(*rules, life), "An ordinary level-up");
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

std::unique_ptr<RulesModule> dummy_rules(std::string extra = {})
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 10 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               extra);
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

std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::uint64_t seed, std::optional<VitalState> state = {},
                                    std::string kind = "dummy")
{
    const auto profile = rules.character_profile(hero.sheet(), {}).data;
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
        {   {1, "campaign-character", "Monk", Side::party, {1, 1}, profile, state},
            {98, std::move(kind), "Dummy", Side::opposition, {2, 1}}
        }},
    seed);
    // Uncanny Metabolism is offered as Initiative is rolled; it is kept for later.
    for (unsigned turns = 0; (c->snapshot().actor != 1 || offered(*c, "initiative_keep")) &&
            turns < 6; ++turns)
        check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Monk's turn");
    return c;
}

void progression_checks()
{
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft("monk"), {}));
    party.award_experience(64000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 9, "In the campaign a Monk stops at 9");
    const auto rules = module();
    check(rules->character_profile(monk(5).sheet(), {}).movement_feet == 40 &&
          rules->character_profile(monk(6).sheet(), {}).movement_feet == 45 &&
          rules->character_profile(monk(10).sheet(), {}).movement_feet == 50 &&
          rules->character_profile(monk(14).sheet(), {}).movement_feet == 55,
          "Unarmored Movement +10 feet, then +15 from 6, +20 from 10 and +25 from 14");
    const auto fifteen = monk(15);
    check(fifteen.sheet().level == 15 && !rules->advancement_options(fifteen.sheet()).level,
          "Level 15 is the Monk's cap");
    const auto holds = [&](std::string_view grant, int level)
    {
        return std::any_of(fifteen.sheet().grants.begin(), fifteen.sheet().grants.end(),
                           [&](const auto & g)
        {
            return g.id == grant && g.level == level;
        });
    };
    check(holds("feature:heightened_focus", 10) && holds("feature:self_restoration", 10) &&
          holds("feature:fleet_step", 11) && holds("feature:deflect_energy", 13) &&
          holds("feature:disciplined_survivor", 14) && holds("feature:perfect_focus", 15),
          "The level 10-15 features arrive at their levels");
    const auto proficient = [](const Character &hero)
    {
        unsigned count = 0;
        for (const auto ability : all_abilities)
            count += hero.sheet().save_proficiencies[ability] ? 1 : 0;
        return count;
    };
    check(proficient(monk(13)) == 2 && proficient(monk(14)) == 6,
          "Disciplined Survivor: proficiency in every save from level 14");
    const auto bytes = encode_campaign(party, nullptr, "monk-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "monk-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "monk-levels") == bytes, "A level-9 Monk saves and reloads");
}

// Extra Attack with the d8 Martial Arts die, and Stunning Strike.
void stunning_checks()
{
    const auto rules = dummy_rules();
    bool stunned = false, staggered = false;
    for (std::uint64_t seed = 1; seed < 80 && !(stunned && staggered); ++seed)
    {
        auto c = duel(*rules, monk(5), seed);
        check(offered(*c, "melee_stun"), "Stunning Strike is offered");
        check(submit(*c, "melee_stun", 98), "A Stunning Strike attack");
        if (!logged(*c, "Monk attempts a Stunning Strike."))
            continue;
        check(!offered(*c, "melee_stun") && offered(*c, "melee"),
              "Once a turn; Extra Attack's second attack is left");
        stunned |= logged(*c, "Dummy is Stunned.");
        staggered |= logged(*c, "Dummy is staggered");
        const auto saved = c->save();
        check(rules->restore(saved)->save() == saved, "A checkpoint keeps the condition");
    }
    check(stunned && staggered, "Both outcomes of the Constitution save");
}

// Flurry of Blows may try a Stunning Strike for one more Focus Point.
void flurry_stun_checks()
{
    const auto rules = dummy_rules();
    bool tried = false;
    for (std::uint64_t seed = 1; seed < 40 && !tried; ++seed)
    {
        auto c = duel(*rules, monk(5), seed);
        check(offered(*c, "flurry_stun"), "Flurry of Blows offers Stunning Strike");
        check(submit(*c, "flurry_stun", 98), "A Flurry with Stunning Strike");
        tried = logged(*c, "Monk attempts a Stunning Strike.");
        if (tried)
            check(!offered(*c, "melee_stun"), "Still once a turn");
    }
    check(tried, "A Flurry hit tries the Stunning Strike");
}

// Empowered Strikes: from level 6 Unarmed Strikes deal Force damage.
void empowered_checks()
{
    const auto rules = dummy_rules("affinity dummy hide resistance bludgeoning\n"
                                   "affinity dummy ward resistance force\n");
    const auto type_logged = [&](unsigned level, std::string_view text)
    {
        for (std::uint64_t seed = 1; seed < 40; ++seed)
        {
            auto c = duel(*rules, monk(level), seed);
            check(submit(*c, "flurry_of_blows", 98), "A Flurry of Blows");
            if (logged(*c, text))
                return true;
        }
        return false;
    };
    check(type_logged(5, "Dummy: Bludgeoning damage"), "A level-5 Unarmed Strike is Bludgeoning");
    check(type_logged(6, "Dummy: Force damage"), "A level-6 Unarmed Strike is Force");
}

// Heightened Focus: three Flurry strikes and Patient Defense's Temporary Hit
// Points at level 10.
void heightened_focus_checks()
{
    const auto rules = dummy_rules();
    for (const unsigned level : {9u, 10u})
    {
        auto c = duel(*rules, monk(level), 1);
        check(submit(*c, "flurry_of_blows", 98), "A Flurry of Blows");
        const auto log = c->snapshot().log();
        const auto strikes = std::count_if(log.begin(), log.end(), [](const auto & line)
        {
            return line.starts_with("Monk -> Dummy: d20");
        });
        check(strikes == (level == 10 ? 3 : 2), "Three strikes from level 10");
    }
    auto c = duel(*rules, monk(10), 1);
    check(submit(*c, "patient_defense_focus") && unit(*c, 1).temporary_hp.amount >= 2,
          "Patient Defense with Focus gives Temporary Hit Points");
}

// Fleet Step: Step of the Wind right after another Bonus Action, from level 11.
void fleet_step_checks()
{
    const auto rules = dummy_rules();
    auto early = duel(*rules, monk(10), 1);
    check(submit(*early, "patient_defense") && !offered(*early, "step_of_the_wind"),
          "Not before level 11");
    auto c = duel(*rules, monk(11), 1);
    check(submit(*c, "patient_defense") && offered(*c, "step_of_the_wind"),
          "Step of the Wind follows another Bonus Action");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps Fleet Step");
    check(submit(*c, "step_of_the_wind") && !offered(*c, "step_of_the_wind"),
          "Once, right after it");
}

// Deflect Energy: Deflect Attacks against Fire from level 13.
void deflect_energy_checks()
{
    const auto rules = dummy_rules("creature brute 10 1000 -10 30 30 1 4 10 0 0 0 0 0 0 0 0 0 1 0\n"
                                   "damage_types brute fire fire\n");
    for (const unsigned level : {12u, 13u})
    {
        auto c = duel(*rules, monk(level), 1, {}, "brute");
        check(submit(*c, "end"), "The Monk waits");
        for (unsigned steps = 0; steps < 6 && c->snapshot().actor == 98 &&
                !c->snapshot().reaction_pending; ++steps)
            check(c->submit(choose_demo_command(*c)), "The brute acts");
        check(offered(*c, "deflect") == (level == 13), "Deflect Attacks against Fire from 13");
    }
}

// Perfect Focus: a level-15 Monk starts a fight with at least 4 Focus Points.
void perfect_focus_checks()
{
    const auto rules = dummy_rules();
    const auto focus = [](const CombatSession &c)
    {
        for (const auto &pool : unit(c, 1).resources)
            if (pool.id == "focus")
                return pool.remaining;
        return 0u;
    };
    // Patient Defense with Focus each turn spends the Focus down to 2.
    auto first = duel(*rules, monk(15), 1);
    for (unsigned steps = 0; steps < 200 && focus(*first) > 2; ++steps)
        if (first->snapshot().reaction_pending)
            check(submit(*first, "decline"), "Decline");
        else if (first->snapshot().actor == 1)
            check(submit(*first, "patient_defense_focus") || submit(*first, "end"), "Spend Focus");
        else
            check(first->submit(choose_demo_command(*first)), "The dummy acts");
    check(focus(*first) == 2, "Two Focus Points left");
    auto c = duel(*rules, monk(15), 1, unit(*first, 1).persistent);
    check(logged(*c, "Monk has 4 Focus Points (Perfect Focus).") && focus(*c) == 4,
          "Perfect Focus restores Focus to 4");
}

// Wholeness of Body heals with a Bonus Action.
void wholeness_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, monk(6), 1, VitalState{5, false, {}, {}});

    check(submit(*c, "wholeness_of_body") && logged(*c, "Monk uses Wholeness of Body.") &&
          unit(*c, 1).hit_points > 5 && !unit(*c, 1).bonus_action,
          "Wholeness of Body heals for a Bonus Action");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        stunning_checks();
        flurry_stun_checks();
        empowered_checks();
        wholeness_checks();
        heightened_focus_checks();
        fleet_step_checks();
        deflect_energy_checks();
        perfect_focus_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Monk level tests passed\n";
}
