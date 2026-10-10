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

// A Barbarian of the given level, advanced with the default choices.
Character barbarian(unsigned level = 1)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "barbarian";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Barbarian";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.training = {{"class:barbarian", {"athletics", "perception"}},
        {"class:barbarian:weapon_mastery", {"greataxe", "handaxe"}}
    };
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "barbarian-xp");
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

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

bool raging(const CombatSession &c)
{
    const auto conditions = unit(c, 1).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [](const auto & condition)
    {
        return condition.source == "Raging";
    });
}

unsigned rages_left(const CombatSession &c)
{
    for (const auto &pool : unit(c, 1).resources)
        if (pool.id == "rage")
            return pool.remaining;
    throw std::runtime_error("No Rage pool");
}

// The Barbarian (1) beside an enemy (98), with a greataxe or the given gear.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::vector<std::string> equipment = {"greataxe"},
                                      std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(hero.sheet(), equipment).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Barbarian", 0, {1, 1}, profile},
            {98, "target", "Enemy", 1, {2, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Barbarian's turn");
    check(c->snapshot().actor == 1, "The Barbarian acts");
    return c;
}

// Ends turns until the given creature acts.
void reach(CombatSession &c, EntityId id)
{
    check(submit(c, "end"), "End the turn");
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 4; ++turns)
        check(submit(c, "end"), "End a turn");
}

void rage_checks()
{
    auto module = rules();
    auto c = battle(*module, barbarian());
    check(rages_left(*c) == 2 && submit(*c, "rage") && logged(*c, "Barbarian enters a Rage.") &&
          raging(*c) && rages_left(*c) == 1 && unit(*c, 1).action && !unit(*c, 1).bonus_action,
          "Rage is a Bonus Action that spends one of two uses");
    check(submit(*c, "melee", 98) && logged(*c, "Rage adds 2 damage."),
          "A Strength-based weapon hit adds the Rage Damage");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Rage survives a checkpoint");
    reach(*c, 98);
    check(submit(*c, "melee", 1), "The enemy attacks");
    check(!logged(*c, "Enemy -> Barbarian") || !logged(*c, "hits") ||
          logged(*c, "Barbarian: Bludgeoning damage"),
          "Rage resists the enemy's Bludgeoning, Piercing or Slashing damage");
    reach(*c, 1);
    check(raging(*c) && offered(*c, "extend_rage") && submit(*c, "melee", 98) &&
          !offered(*c, "extend_rage"),
          "An attack roll extends the Rage, and so would a Bonus Action");
    reach(*c, 1);
    check(raging(*c), "The extended Rage lasts through the next turn");
    reach(*c, 1);
    check(!raging(*c), "A turn without attacking or extending lets the Rage end");
}

void heavy_armor_checks()
{
    auto module = rules();
    auto c = battle(*module, barbarian(), {"greataxe", "plate"});
    check(!offered(*c, "rage"), "No Rage in Heavy armor");
}

bool has_grant(const Character &hero, std::string_view id, std::string_view source = {})
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id && (source.empty() || g.source_id == source);
    });
}

void advancement_checks()
{
    const auto second = barbarian(2), third = barbarian(3), fourth = barbarian(4);
    check(second.sheet().level == 2 && has_grant(second, "feature:danger_sense") &&
          has_grant(second, "feature:reckless_attack") && !has_grant(second, "subclass:berserker"),
          "Level two brings Danger Sense and Reckless Attack");
    const auto &skills = third.sheet().grants;
    check(has_grant(third, "subclass:berserker") && has_grant(third, "feature:frenzy") &&
          has_grant(third, "feature:primal_knowledge") &&
          std::count_if(skills.begin(), skills.end(), [](const auto & g)
    {
        return g.source_id == "class:barbarian:primal_knowledge" && g.id.starts_with("skill:");
    }) == 1,
    "Level three brings the Berserker, Frenzy and a Primal Knowledge skill");
    const auto &masteries = fourth.sheet().grants;
    check(fourth.sheet().level == 4 &&
          std::count_if(masteries.begin(), masteries.end(), [](const auto & g)
    {
        return g.id.starts_with("mastery:");
    }) == 3,
    "Level four brings a third Weapon Mastery");
    auto module = rules();
    auto c = battle(*module, third);
    check(rages_left(*c) == 3, "A level-three Barbarian has three Rages");
}

void reckless_checks()
{
    auto module = rules();
    auto c = battle(*module, barbarian(2));
    check(offered(*c, "reckless") && submit(*c, "reckless", 98) &&
          logged(*c, "Barbarian attacks recklessly.") && logged(*c, "(advantage)") &&
          !unit(*c, 1).action,
          "A Reckless attack is the Attack action's attack, with Advantage");
    reach(*c, 98);
    const auto before = c->snapshot().log().size();
    check(submit(*c, "melee", 1), "The enemy attacks");
    const auto log = c->snapshot().log();
    check(std::any_of(log.begin() + std::ptrdiff_t(before), log.end(), [](const auto & line)
    {
        return line.find("Enemy -> Barbarian") != std::string::npos &&
               line.find("(advantage)") != std::string::npos;
    }),
    "Attacks against a reckless Barbarian have Advantage");
}

void frenzy_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 32; ++seed)
    {
        auto c = battle(*module, barbarian(3), {"greataxe"}, seed);
        check(submit(*c, "rage") && submit(*c, "reckless", 98), "Rage, then attack recklessly");
        if (!logged(*c, "Rage adds"))
            continue;
        check(logged(*c, "Frenzy adds"), "A raging, reckless hit adds Frenzy's 2d6");
        return;
    }
    throw std::runtime_error("No seed hits");
}

} // namespace

int main()
{
    try
    {
        rage_checks();
        heavy_armor_checks();
        advancement_checks();
        reckless_checks();
        frenzy_checks();
        std::cout << "Barbarian tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
