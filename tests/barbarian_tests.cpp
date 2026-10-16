#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include "status_effects.h"
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
namespace fx = opengold::srd5::detail;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
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
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.training = {{"class:barbarian", {"nature", "perception"}},
                  {"class:barbarian:weapon_mastery", {"greataxe", "handaxe"}}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "barbarian-xp");
    for (unsigned n = 1; n < level; ++n)
    {
        party.advance(id, party.default_advancement(id));
    }
    return party.member(id).character;
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

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb;
                       });
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

bool raging(const CombatSession &c)
{
    const auto conditions = unit(c, 1).conditions;
    return std::any_of(conditions.begin(), conditions.end(),
                       [](const auto &condition)
                       {
                           return condition.source == "Raging";
                       });
}

unsigned rages_left(const CombatSession &c)
{
    for (const auto &pool : unit(c, 1).resources)
    {
        if (pool.id == "rage")
        {
            return pool.remaining;
        }
    }
    throw std::runtime_error("No Rage pool");
}

// The Barbarian (1) beside an enemy (98), with a greataxe or the given gear.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::vector<std::string> equipment = {"greataxe"},
                                      std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(hero.sheet(), equipment).data;
    auto c = module.create({{12, 6, std::vector<Terrain>(72)},
                            {{1, "campaign-character", "Barbarian", Side::party, {1, 1}, profile},
                             {98, "target", "Enemy", Side::opposition, {2, 1}}}},
                           seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the Barbarian's turn");
    }
    check(c->snapshot().actor == 1, "The Barbarian acts");
    return c;
}

// Ends turns until the given creature acts.
void reach(CombatSession &c, EntityId id)
{
    check(submit(c, "end"), "End the turn");
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 4; ++turns)
    {
        check(submit(c, "end"), "End a turn");
    }
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

// The Barbarian (1), Restrained by Entangle, beside an enemy (98), carrying Acid.
std::unique_ptr<CombatSession> entangled(const RulesModule &module)
{
    const auto hero = barbarian();
    Participant barbarian_entry{
        1,
        "campaign-character",
        "Barbarian",
        Side::party,
        {1, 1},
        module.character_profile(hero.sheet(), std::vector<std::string>{"greataxe"}).data,
        VitalState{hero.sheet().hit_points, false,
                   "SRD13 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 2 "
                   "0 FX8 2 1 1 19 77 99 \"Druid\" 15 60000 0 0"}};
    barbarian_entry.inventory = {{0, "acid", 3, -1}};
    auto c = module.create({{12, 6, std::vector<Terrain>(72)},
                            {barbarian_entry, {98, "target", "Enemy", Side::opposition, {2, 1}}}},
                           5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the Barbarian's turn");
    }
    return c;
}

// Rage gives Strength checks Advantage, including the escape from Restrained.
void rage_escape_checks()
{
    auto module = rules();
    auto plain = entangled(*module);
    check(submit(*plain, "escape") && logged(*plain, "Barbarian Athletics check: d20") &&
              !logged(*plain, "(advantage)"),
          "Without Rage the escape check is a plain roll");
    auto raging_hero = entangled(*module);
    check(submit(*raging_hero, "rage") && submit(*raging_hero, "escape") &&
              logged(*raging_hero, "(advantage) + "),
          "A raging Barbarian's escape check has Advantage");
}

// Forcing an enemy's saving throw extends a Rage, as an attack roll does.
void thrown_flask_rage_checks()
{
    auto module = rules();
    auto c = entangled(*module);
    check(submit(*c, "rage"), "Enter a Rage");
    reach(*c, 1);
    check(raging(*c) && submit(*c, "throw_acid", 98) && logged(*c, "Enemy Dexterity save"),
          "Thrown Acid forces the enemy's Dexterity save");
    reach(*c, 1);
    check(raging(*c), "The save extends the Rage through the next turn");
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
    return std::any_of(grants.begin(), grants.end(),
                       [&](const auto &g)
                       {
                           return g.id == id && (source.empty() || g.source_id == source);
                       });
}

void advancement_checks()
{
    const auto second = barbarian(2), third = barbarian(3), fourth = barbarian(4);
    check(second.sheet().level == 2 && has_grant(second, "feature:danger_sense") &&
              has_grant(second, "feature:reckless_attack") &&
              !has_grant(second, "subclass:berserker"),
          "Level two brings Danger Sense and Reckless Attack");
    const auto &skills = third.sheet().grants;
    check(has_grant(third, "subclass:berserker") && has_grant(third, "feature:frenzy") &&
              has_grant(third, "feature:primal_knowledge") &&
              std::count_if(skills.begin(), skills.end(),
                            [](const auto &g)
                            {
                                return g.source_id == "class:barbarian:primal_knowledge" &&
                                       g.id.starts_with("skill:");
                            }) == 1,
          "Level three brings the Berserker, Frenzy and a Primal Knowledge skill");
    const auto &masteries = fourth.sheet().grants;
    check(fourth.sheet().level == 4 && std::count_if(masteries.begin(), masteries.end(),
                                                     [](const auto &g)
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
    check(std::any_of(log.begin() + std::ptrdiff_t(before), log.end(),
                      [](const auto &line)
                      {
                          return line.find("Enemy -> Barbarian") != std::string::npos &&
                                 line.find("(advantage)") != std::string::npos;
                      }),
          "Attacks against a reckless Barbarian have Advantage");
}

// Reckless lasts until the start of the Barbarian's next turn and survives a
// checkpoint.
void reckless_duration_checks()
{
    auto module = rules();
    auto c = battle(*module, barbarian(2));
    const auto reckless = [&]
    {
        const auto conditions = unit(*c, 1).conditions;
        return std::any_of(conditions.begin(), conditions.end(),
                           [](const auto &condition)
                           {
                               return condition.source == "Reckless";
                           });
    };
    check(submit(*c, "reckless", 98) && reckless(),
          "Attacking recklessly makes the Barbarian Reckless");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Reckless survives a checkpoint");
    reach(*c, 98);
    check(reckless(), "Reckless lasts through the enemy's turn");
    reach(*c, 1);
    check(!reckless(), "Reckless ends at the start of the Barbarian's next turn");
}

// An enemy Cleric casts Sacred Flame at the Barbarian, who rolls a Dexterity
// save; returns its natural roll. `incapacitated` laughs it helplessly first.
int danger_save(const RulesModule &module, const Character &hero, std::uint64_t seed,
                bool incapacitated)
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Foe";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.cantrips = {"sacred_flame"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds"}, {}, {}};
    const Character foe(*srd5::character_rules(), d, {});
    Encounter e{
        {12, 6, std::vector<Terrain>(72)},
        {{1,
          "campaign-character",
          "Barbarian",
          Side::party,
          {1, 1},
          module.character_profile(hero.sheet(), std::vector<std::string>{"greataxe"}).data},
         {98,
          "campaign-character",
          "Foe",
          Side::opposition,
          {6, 1},
          module.character_profile(foe.sheet(), {}).data}}};
    if (incapacitated)
    {
        auto vitals = unit(*module.create(e, seed), 1).persistent;
        const auto at = vitals.resources.find("FX8");
        std::istringstream in(vitals.resources.substr(at));
        auto effects = fx::read_effects(in);
        fx::apply_repeating_condition(effects, fx::EffectKind::laughing, {.scope = 7, .actor = 98},
                                      "Foe", {.dc = 38, .first_save_ms = 6000});
        std::ostringstream out;
        fx::write_effects(out, effects);
        vitals.resources.replace(at, std::string::npos, out.str());
        e.participants[0].state = vitals;
    }
    auto c = module.create(e, seed);
    for (unsigned turns = 0; c->snapshot().actor != 98 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the Cleric's turn");
    }
    check(submit(*c, "sacred_flame", 1), "The Cleric casts Sacred Flame at the Barbarian");
    for (const auto &line : c->snapshot().log())
    {
        if (const auto at = line.find("Barbarian Dexterity save: d20 "); at != std::string::npos)
        {
            return std::stoi(line.substr(at + 30));
        }
    }
    throw std::runtime_error("No Dexterity save in the log");
}

// Danger Sense: Advantage on Dexterity saves, unless Incapacitated. With the
// same rolls, Advantage keeps the first die or a higher second one.
void danger_sense_checks()
{
    auto module = rules();
    const auto first = barbarian(1), second = barbarian(2);
    bool higher = false;
    for (std::uint64_t seed = 1; seed <= 24; ++seed)
    {
        const int plain = danger_save(*module, first, seed, false),
                  sensed = danger_save(*module, second, seed, false);
        check(sensed >= plain, "Danger Sense never lowers a Dexterity save");
        higher |= sensed > plain;
        check(danger_save(*module, second, seed, true) == danger_save(*module, first, seed, true),
              "An Incapacitated Barbarian has no Danger Sense");
    }
    check(higher, "Danger Sense gives Advantage on Dexterity saves");
}

// Primal Knowledge is one of its six skills the Barbarian does not already have;
// a refused choice changes nothing.
void primal_knowledge_checks()
{
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(barbarian(2));
    party.award_experience(900, "primal:level");
    const auto before = party.member(id).character.sheet();
    for (const auto *skill : {"athletics", "arcana", "unknown"})
    {
        auto choice = party.default_advancement(id);
        choice.training["class:barbarian:primal_knowledge"] = {skill};
        bool refused = false;
        try
        {
            party.advance(id, choice);
        }
        catch (const std::exception &)
        {
            refused = true;
        }
        check(refused && party.member(id).character.sheet().level == 2 &&
                  party.member(id).character.sheet().grants == before.grants,
              "A held, foreign or unknown Primal Knowledge skill is refused");
    }
    auto choice = party.default_advancement(id);
    choice.training["class:barbarian:primal_knowledge"] = {"survival"};
    party.advance(id, choice);
    check(
        has_grant(party.member(id).character, "skill:survival", "class:barbarian:primal_knowledge"),
        "Survival is learned through Primal Knowledge");
}

void frenzy_checks()
{
    auto module = rules();
    for (std::uint64_t seed = 1; seed < 32; ++seed)
    {
        auto c = battle(*module, barbarian(3), {"greataxe"}, seed);
        check(submit(*c, "rage") && submit(*c, "reckless", 98), "Rage, then attack recklessly");
        if (!logged(*c, "Rage adds"))
        {
            continue;
        }
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
        rage_escape_checks();
        thrown_flask_rage_checks();
        heavy_armor_checks();
        advancement_checks();
        reckless_checks();
        reckless_duration_checks();
        danger_sense_checks();
        primal_knowledge_checks();
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
