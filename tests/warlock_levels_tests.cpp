#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// A Fiend Warlock from level 5 to 15 (LEVELS-1, LEVELS-2): the Warlock's
// level-3 to level-5 spells, the Fiend Spells, more invocations (Eldritch
// Smite, Thirsting Blade, Lifedrinker, Devouring Blade, Witch Sight), Dark
// One's Own Luck, Fiendish Resilience, Mystic Arcanum and Hurl Through Hell.
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

// An AC 1 dummy and a brute that always hits.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 1 1000 20 30 30 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

CharacterDraft draft(std::string first)
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
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:warlock", {"investigation", "deception"}},
        {"class:warlock:invocations", {std::move(first)}}
    };
    d.cantrips = {"eldritch_blast", "chill_touch"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"hex", "hellish_rebuke"}, {}, {}};
    return d;
}

// A Warlock of the level through the default level-ups, taking `picks`'
// invocations at their levels instead, and Circle of Death for its first
// Mystic Arcanum.
Character warlock(unsigned level, std::string first = "pact_of_the_blade",
                  std::map<unsigned, std::vector<std::string>> picks = {})
{
    Character hero(*srd5::character_rules(), draft(std::move(first)), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        const auto next = static_cast<unsigned>(hero.sheet().level) + 1;
        auto choice = rules->default_advancement(hero.sheet());
        if (const auto chosen = picks.find(next); chosen != picks.end())
            choice.training["class:warlock:invocations:" + std::to_string(next)] = chosen->second;
        if (next == 11)
            choice.training["class:warlock:arcanum:11"] = {"circle_of_death"};
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
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

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
        check(submit(c, "end") || submit(c, "decline"), "End a turn");
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Warlock (1) at (1, 2) with a longsword and a creature beside it.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet,
                                     std::string foe = "dummy", std::uint64_t seed = 1)
{
    const std::vector<std::string> gear{"longsword"};
    auto c = rules.create({{14, 7, std::vector<Terrain>(98)},
        {   {1, "campaign-character", "Warlock", Side::party, {1, 2},
                rules.character_profile(sheet, gear).data},
            {98, std::move(foe), "Foe", Side::opposition, {2, 2}}
        }},
    seed);
    to_turn(*c, 1);
    return c;
}

bool granted(const CharacterSheet &sheet, std::string_view id)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft("pact_of_the_blade"), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    const auto sheet = party.member(id).character.sheet();
    check(sheet.level == 15, "A Warlock reaches 15");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>
{
    {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 10}, {11, 11}, {12, 11}, {13, 12}, {14, 12},
    {15, 13}
})
    check(rules->spell_access(warlock(level).sheet()).prepared_choices == count,
          "Prepared spells: 6 at 5 to 13 at 15");
    check(rules->spell_access(warlock(10).sheet()).cantrip_choices == 4, "A fourth cantrip at 10");
    check(std::count_if(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id.starts_with("invocation:");
    }) == 9, "Nine invocations at 15");
    for (const unsigned level : {11u, 13u, 15u})
        check(std::any_of(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
    {
        return g.id.starts_with("arcanum:") && g.level == level;
    }), "Mystic Arcanum at 11, 13 and 15");
    check(granted(sheet, "feature:hurl_through_hell"), "Hurl Through Hell at 14");
    check(granted(sheet, "feature:dark_ones_own_luck") &&
          std::any_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id.starts_with("resilience:") && g.level == 10;
    }), "Dark One's Own Luck at 6, Fiendish Resilience at 10");
    check(rules->advancement_options(warlock(13).sheet()).description.find("Hurl Through Hell") != std::string::npos,
          "The level's description names its feature");
    const auto bytes = encode_campaign(party, nullptr, "warlock-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "warlock-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "warlock-levels") == bytes,
          "A level-15 Warlock saves and reloads");
}

void fiend_spells_checks()
{
    const auto rules = dummy_rules();
    check(offered(*field(*rules, warlock(5).sheet()), "fireball"), "Fireball is always prepared at 5");
    check(offered(*field(*rules, warlock(7).sheet()), "fire_shield_warm"),
          "Fire Shield is always prepared at 7");
    check(offered(*field(*rules, warlock(9).sheet()), "insect_plague"),
          "Insect Plague is always prepared at 9");
}

void blade_checks()
{
    const auto rules = dummy_rules();
    const auto hero = warlock(9, "pact_of_the_blade",
    {{5, {"eldritch_smite", "thirsting_blade"}}, {9, {"lifedrinker"}}});
    auto c = field(*rules, hero.sheet());
    check(submit(*c, "melee", 98) && offered(*c, "eldritch_smite", 98) &&
          offered(*c, "lifedrinker_radiant", 98), "Eldritch Smite and Lifedrinker follow a pact weapon hit");
    check(submit(*c, "eldritch_smite", 98) && logged(*c, "Force damage from Eldritch Smite.") &&
          logged(*c, "Foe is knocked Prone."), "Eldritch Smite spends a slot and knocks the foe Prone");
    check(submit(*c, "lifedrinker_radiant", 98) && logged(*c, "Radiant damage from Lifedrinker."),
          "Lifedrinker adds Radiant damage");
    check(offered(*c, "melee", 98) && !offered(*c, "eldritch_smite", 98),
          "Thirsting Blade's second attack; Eldritch Smite once a turn");
}

void arcanum_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, warlock(11).sheet());
    const auto commands = c->legal_commands();
    const auto arcanum = std::find_if(commands.begin(), commands.end(), [](const auto & command)
    {
        return command.verb.ends_with("_arcanum");
    });
    check(arcanum != commands.end(), "Mystic Arcanum's spell is offered");
    const auto verb = arcanum->verb;
    check(c->submit(*arcanum), "Cast the Mystic Arcanum spell");
    if (c->snapshot().spell_targeting || c->snapshot().area_targeting)
        check(submit(*c, "area_cast") || submit(*c, "spell_cast"), "Finish the casting");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(!offered(*c, verb), "Once per Long Rest");
}

void hurl_checks()
{
    const auto rules = dummy_rules();
    bool hurled = false;
    for (std::uint64_t seed = 1; seed < 40 && !hurled; ++seed)
    {
        auto c = field(*rules, warlock(14).sheet(), "dummy", seed);
        check(submit(*c, "melee", 98) && offered(*c, "hurl_through_hell", 98) &&
              submit(*c, "hurl_through_hell", 98), "Hurl Through Hell follows a hit");
        if (!logged(*c, "Foe is hurled through the Lower Planes."))
            continue;
        hurled = true;
        check(logged(*c, "Psychic damage from Hurl Through Hell.") && unit(*c, 98).banished,
              "The foe is gone and hurt");
        check(submit(*c, "end"), "End the turn");
        to_turn(*c, 1);
        check(submit(*c, "end") && logged(*c, "Foe returns from the Lower Planes."),
              "It returns at the end of the Warlock's next turn");
    }
    check(hurled, "A seed where the foe fails its save");
}

void devouring_checks()
{
    const auto rules = dummy_rules();
    const auto hero = warlock(15, "pact_of_the_blade",
    {{5, {"eldritch_smite", "thirsting_blade"}}, {12, {"devouring_blade"}}, {15, {"witch_sight"}}});
    check(granted(hero.sheet(), "invocation:devouring_blade") &&
          granted(hero.sheet(), "invocation:witch_sight"), "Devouring Blade at 12, Witch Sight at 15");
    auto c = field(*rules, hero.sheet());
    check(submit(*c, "melee", 98) && submit(*c, "melee", 98) && submit(*c, "melee", 98),
          "Devouring Blade: three attacks");
    check(!offered(*c, "melee", 98), "No fourth attack");
}

void luck_checks()
{
    const auto rules = dummy_rules();
    bool lucky = false;
    for (std::uint64_t seed = 1; seed < 80 && !lucky; ++seed)
    {
        auto c = field(*rules, warlock(6).sheet(), "brute", seed);
        check(submit(*c, "hex_3", 98), "Concentrate on Hex from a level-3 slot");
        to_turn(*c, 98);
        // Hellish Rebuke is declined.
        check(submit(*c, "melee", 1) && (!c->snapshot().reaction_pending || submit(*c, "decline")),
              "The brute attacks");
        lucky = logged(*c, "to the save (Dark One's Own Luck).");
    }
    check(lucky, "Dark One's Own Luck adds 1d10 to a failed save it could still pass");
}

void resilience_checks()
{
    const auto rules = module();
    auto sheet = warlock(10).sheet();
    const auto offers = rules->rest_training_options(sheet, RestKind::short_rest);
    check(std::any_of(offers.begin(), offers.end(), [](const auto & offer)
    {
        return offer.group.id == "subclass:warlock:fiendish_resilience";
    }), "Fiendish Resilience changes after a Short Rest");
    const std::vector<std::string> fire{"fire"};
    (void)rules->replace_rest_training(sheet, fire);
    check(granted(sheet, "resilience:fire"), "Fiendish Resilience now resists Fire");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        fiend_spells_checks();
        blade_checks();
        luck_checks();
        resilience_checks();
        arcanum_checks();
        hurl_checks();
        devouring_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Warlock level checks passed\n";
}
