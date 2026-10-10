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

// A sturdy AC 1 target that hits back.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A Monk with Strength 9 and Dexterity 18, advanced to the given level.
Character monk(unsigned level = 1)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "monk";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Monk";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{3, 3, 3, 1}, 3};
    d.rolls[1] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:monk", {"acrobatics", "stealth"}}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "monk-xp");
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

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

std::vector<std::string> attack_lines(const CombatSession &c)
{
    std::vector<std::string> lines;
    for (const auto &line : c.snapshot().log())
        if (line.starts_with("Monk -> Enemy: d20 "))
            lines.push_back(line);
    return lines;
}

// The number after `before` in a line.
int number_after(const std::string &line, std::string_view before)
{
    return std::stoi(line.substr(line.find(before) + before.size()));
}

// The Monk (1) beside an enemy (98).
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::vector<std::string> equipment = {},
                                      std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(hero.sheet(), equipment).data;
    auto c = module.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Monk", 0, {1, 1}, profile},
            {98, "target", "Enemy", 1, {2, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Monk's turn");
    check(c->snapshot().actor == 1, "The Monk acts");
    return c;
}

void martial_arts_checks()
{
    auto module = rules();
    auto c = battle(*module, monk());
    check(submit(*c, "melee", 98) && submit(*c, "martial_arts", 98), "Strike, then strike again");
    const auto lines = attack_lines(*c);
    check(lines.size() == 2 && number_after(lines[0], " + ") == 6 &&
          number_after(lines[1], " + ") == 6,
          "Unarmed Strikes use Dexterity: +2 Proficiency and +4");
    for (const auto &line : lines)
        if (line.find(" hits for ") != std::string::npos)
        {
            const int damage = number_after(line, " hits for ");
            check(damage >= 5 && damage <= 10, "An Unarmed Strike deals 1d6 + 4");
        }
    check(!unit(*c, 1).bonus_action, "The second Unarmed Strike spent the Bonus Action");

    auto dagger = battle(*module, monk(), {"dagger"});
    check(submit(*dagger, "melee", 98) && number_after(attack_lines(*dagger)[0], " + ") == 6 &&
          offered(*dagger, "martial_arts"),
          "A Monk weapon also uses Dexterity, and the Bonus Unarmed Strike stays");

    auto armored = battle(*module, monk(), {"leather"});
    check(!offered(*armored, "martial_arts"), "Armor ends Martial Arts");
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

unsigned focus_left(const CombatSession &c)
{
    for (const auto &pool : unit(c, 1).resources)
        if (pool.id == "focus")
            return pool.remaining;
    throw std::runtime_error("No Focus pool");
}

bool has_grant(const Character &hero, std::string_view id)
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

void advancement_checks()
{
    const auto second = monk(2), third = monk(3), fourth = monk(4);
    check(has_grant(second, "feature:monks_focus") && has_grant(second, "feature:unarmored_movement") &&
          has_grant(second, "feature:uncanny_metabolism") &&
          !has_grant(second, "feature:deflect_attacks"),
          "Level two brings Monk's Focus, Unarmored Movement and Uncanny Metabolism");
    check(has_grant(third, "feature:deflect_attacks") && has_grant(third, "subclass:open_hand") &&
          has_grant(third, "feature:open_hand_technique") && fourth.sheet().level == 4,
          "Level three brings Deflect Attacks and the Open Hand; level four is reached");
    auto module = rules();
    auto c = battle(*module, fourth);
    check(focus_left(*c) == 4 && unit(*c, 1).movement_feet == 40,
          "Four Focus Points and 10 feet more Speed at level four");
}

void focus_checks()
{
    auto module = rules();
    {
        auto c = battle(*module, monk(2));
        check(submit(*c, "flurry_of_blows", 98) && logged(*c, "Monk uses Flurry of Blows.") &&
              attack_lines(*c).size() == 2 && focus_left(*c) == 1 && unit(*c, 1).action,
              "Flurry of Blows: two Unarmed Strikes for a Focus Point and the Bonus Action");
    }
    {
        auto c = battle(*module, monk(2));
        const int feet = unit(*c, 1).movement_feet;
        check(submit(*c, "step_of_the_wind") && unit(*c, 1).movement_feet == 2 * feet &&
              focus_left(*c) == 2,
              "Step of the Wind Dashes for free");
    }
    {
        auto c = battle(*module, monk(2));
        check(submit(*c, "patient_defense_focus") && focus_left(*c) == 1 &&
              logged(*c, "Monk uses Patient Defense."),
              "Patient Defense with a Focus Point Disengages and Dodges");
        check(submit(*c, "end"), "End the Monk's turn");
        check(submit(*c, "melee", 1) && logged(*c, "(disadvantage)"),
              "The Dodge gives attackers Disadvantage");
    }
}

// The Monk (1), hurt, beside an enemy (98), before Initiative decisions.
std::unique_ptr<CombatSession> hurt_battle(const RulesModule &module, const Character &hero)
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    Participant monk{1, "campaign-character", "Monk", 0, {1, 1}, profile};
    monk.state = VitalState{hero.sheet().hit_points - 5};
    return module.create({{12, 6, std::vector<Terrain>(72)},
        {monk, {98, "target", "Enemy", 1, {2, 1}}}},
    5);
}

void metabolism_checks()
{
    auto module = rules();
    auto c = hurt_battle(*module, monk(2));
    check(c->snapshot().initiative_choices == std::vector<EntityId> {1} &&
          offered(*c, "uncanny_metabolism") && offered(*c, "initiative_keep"),
          "A hurt Monk is offered Uncanny Metabolism at Initiative");
    const int hp = unit(*c, 1).hit_points;
    check(submit(*c, "uncanny_metabolism") && logged(*c, "Monk uses Uncanny Metabolism.") &&
          unit(*c, 1).hit_points > hp && c->snapshot().initiative_choices.empty(),
          "Uncanny Metabolism restores Hit Points and starts the fight");
}

// The enemy attacks the Monk until a hit asks about Deflect Attacks.
std::unique_ptr<CombatSession> deflect_question(const RulesModule &module, std::uint64_t &seed)
{
    for (; seed < 64; ++seed)
    {
        auto c = battle(module, monk(3), {}, seed);
        check(submit(*c, "end"), "End the Monk's turn");
        check(submit(*c, "melee", 1), "The enemy attacks");
        if (!c->snapshot().reaction_pending)
            continue;
        check(c->snapshot().actor == 1 && offered(*c, "deflect") && offered(*c, "decline") &&
              !offered(*c, "shield"),
              "A hit Monk is asked about Deflect Attacks");
        return c;
    }
    throw std::runtime_error("No seed hits the Monk");
}

void deflect_checks()
{
    auto module = rules();
    std::uint64_t seed = 1;
    auto c = deflect_question(*module, seed);
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The question survives a checkpoint");
    const int hp = unit(*c, 1).hit_points;
    check(submit(*c, "deflect") && !unit(*c, 1).reaction, "Deflect Attacks spends the Reaction");
    // The target creature's blows are weak, so the deflection stops them all.
    check(c->snapshot().reaction_pending && offered(*c, "redirect") &&
          !logged(*c, "Enemy -> Monk"),
          "Fully deflected, the Monk is asked about redirecting it");
    const auto at_redirect = c->save();
    check(module->restore(at_redirect)->save() == at_redirect,
          "The redirect question survives a checkpoint");
    check(submit(*c, "redirect") && logged(*c, "Monk deflects") &&
          logged(*c, "Monk redirects the attack at Enemy.") && logged(*c, "Enemy Dexterity save") &&
          unit(*c, 1).hit_points == hp && focus_left(*c) == 2,
          "Redirecting spends a Focus Point and forces a Dexterity save");

    auto declined = deflect_question(*module, ++seed);
    check(submit(*declined, "decline") && logged(*declined, "Enemy -> Monk") &&
          !logged(*declined, "deflects"),
          "Declining lets the hit land");
}

void open_hand_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, monk(3), {}, seed);
        check(offered(*c, "flurry_addle") && offered(*c, "flurry_push") &&
              submit(*c, "flurry_topple", 98),
              "Open Hand Technique is chosen with the Flurry");
        if (!logged(*c, "Enemy is knocked Prone."))
            continue;
        check(logged(*c, "Enemy Dexterity save"), "Topple calls for a Dexterity save");
        return;
    }
    throw std::runtime_error("No seed topples the enemy");
}

} // namespace

int main()
{
    try
    {
        martial_arts_checks();
        advancement_checks();
        focus_checks();
        metabolism_checks();
        deflect_checks();
        open_hand_checks();
        std::cout << "Monk tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
