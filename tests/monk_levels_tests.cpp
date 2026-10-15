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

std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 10 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
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
                                    std::uint64_t seed, std::optional<VitalState> state = {})
{
    const auto profile = rules.character_profile(hero.sheet(), {}).data;
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)},
        {   {1, "campaign-character", "Monk", Side::party, {1, 1}, profile, state},
            {98, "dummy", "Dummy", Side::opposition, {2, 1}}
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
    check(party.member(id).character.sheet().level == 9, "A Monk advances to level 9");
    const auto rules = module();
    check(rules->character_profile(monk(5).sheet(), {}).movement_feet == 40 &&
          rules->character_profile(monk(6).sheet(), {}).movement_feet == 45,
          "Unarmored Movement +10 feet, then +15 from level 6");
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
        wholeness_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Monk level tests passed\n";
}
