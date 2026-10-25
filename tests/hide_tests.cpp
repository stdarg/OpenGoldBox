#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/character_pool.h"
#include "opengold/combat_demo.h"
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

// Hide (SRD 5.2.1, HIDE-1): who may hide, what hiding does, how it ends and
// how enemies find a hidden creature.
namespace
{
namespace fx = opengold::srd5::detail;
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// The game's rules with three test creatures that do not fight back: one
// that sees everything (Passive Perception 30), one that sees little (14,
// under any successful Stealth total) and one with 1000 Hit Points.
std::unique_ptr<RulesModule> module()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    std::string content{std::istreambuf_iterator<char>(in), {}};
    content += "\ncreature keen 10 50 0 30 4 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
               "perception keen 20\n"
               "creature dull 10 50 0 30 4 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
               "perception dull 4\n"
               "creature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    return srd5::parse_content(content);
}

// A Character Pool character of the class at the level.
Character pool_character(std::string_view klass, unsigned level)
{
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.set_original_head(1, por::PortraitPart{"fixture", head});
    art.set_original_body(1, por::PortraitPart{"fixture", body});
    for (const auto &preset : character_pool(*srd5::character_rules(), art))
    {
        if (preset.creation_data().character_class == klass)
        {
            CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
            const auto id =
                party.add_pc(Character(*srd5::character_rules(), preset.creation_data(), {}));
            party.award_experience(level == 2 ? 300 : 0, "hide-xp");
            if (level == 2)
            {
                party.advance(id, party.default_advancement(id));
            }
            return party.member(id).character;
        }
    }
    throw std::runtime_error("No pool character of the class");
}

// The Rogue at level 2, with Cunning Action.
Character rogue()
{
    return pool_character("rogue", 2);
}

// A 12 by 6 field with a wall at x = 4 from y = 0 to 2: the Rogue at (3, 1)
// is out of sight of anyone east of the wall.
std::vector<Terrain> walled()
{
    std::vector<Terrain> terrain(72);
    for (int y = 0; y <= 2; ++y)
    {
        terrain[y * 12 + 4] = Terrain::obstacle;
    }
    return terrain;
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
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
    return false;
}

bool move_to(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == "move" && command.destination == cell)
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

std::unique_ptr<CombatSession> fight(const RulesModule &rules, const Character &hero,
                                     std::string_view foe, Cell foe_cell, std::uint64_t seed)
{
    auto c = rules.create(
        {{12, 6, walled()},
         {{1,
           "campaign-character",
           "Rogue",
           Side::party,
           {3, 1},
           rules.character_profile(hero.sheet(), std::vector<std::string>{"dagger"}).data},
          {98, std::string(foe), "Foe", Side::opposition, foe_cell}}},
        seed);
    // The Rogue's Alert offers an initiative swap first; it keeps its place.
    for (unsigned turns = 0;
         turns < 6 && (c->snapshot().actor != 1 || offered(*c, "initiative_keep")); ++turns)
    {
        check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Rogue's turn");
    }
    return c;
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

// A seed whose first Stealth check hides the Rogue with the Cunning Action.
std::unique_ptr<CombatSession> hidden_rogue(const RulesModule &rules, const Character &hero,
                                            std::string_view foe, Cell foe_cell)
{
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        auto c = fight(rules, hero, foe, foe_cell, seed);
        check(submit(*c, "cunning_hide"), "The Rogue hides as a Bonus Action");
        if (unit(*c, 1).hidden)
        {
            return c;
        }
    }
    throw std::runtime_error("No seed hides the Rogue");
}

// Hiding needs every enemy out of sight; a Rogue hides as a Bonus Action and
// keeps its Action.
void hiding_checks()
{
    const auto rules = module();
    const auto hero = rogue();
    auto open = fight(*rules, hero, "target", {3, 4}, 1);
    check(!offered(*open, "hide") && !offered(*open, "cunning_hide"),
          "No hiding in an enemy's sight");
    auto c = fight(*rules, hero, "target", {8, 1}, 1);
    check(offered(*c, "hide") && offered(*c, "cunning_hide"),
          "Behind the wall the Rogue may hide with its Action or a Bonus Action");
    c = hidden_rogue(*rules, hero, "target", {8, 1});
    check(logged(*c, "Rogue Stealth check: d20 ") && logged(*c, "Rogue is hidden (Stealth ") &&
              unit(*c, 1).action && !unit(*c, 1).bonus_action,
          "The Stealth check hides the Rogue; its Action is left");
    const auto conditions = unit(*c, 1).conditions;
    check(std::any_of(conditions.begin(), conditions.end(),
                      [](const auto &m)
                      {
                          return m.source == "Hidden";
                      }),
          "The Rogue shows as Hidden");
    check(!offered(*c, "hide") && !offered(*c, "cunning_hide"), "A hidden Rogue cannot hide again");
    const auto saved = c->save();
    const auto restored = rules->restore(saved);
    check(restored->save() == saved && unit(*restored, 1).hidden,
          "A checkpoint keeps the Rogue hidden");
    check(!unit(*c, 1).persistent.resources.empty() &&
              unit(*c, 1).persistent.resources.find(" 71 ") == std::string::npos,
          "Hiding does not outlast the fight");
}

// A hidden Rogue stepping into a keen enemy's view is found at once; a dull
// one does not notice, and attacking ends hiding after the roll's Advantage.
void finding_checks()
{
    const auto rules = module();
    const auto hero = rogue();
    auto c = hidden_rogue(*rules, hero, "keen", {8, 1});
    check(move_to(*c, {3, 2}) && move_to(*c, {3, 3}) && move_to(*c, {3, 4}),
          "The Rogue steps out past the wall");
    check(logged(*c, "Foe finds Rogue.") && !unit(*c, 1).hidden,
          "The keen enemy's Passive Perception finds the Rogue in view");

    c = hidden_rogue(*rules, hero, "dull", {6, 1});
    check(move_to(*c, {3, 2}) && move_to(*c, {3, 3}) && move_to(*c, {3, 4}) && unit(*c, 1).hidden &&
              !logged(*c, "finds Rogue"),
          "The dull enemy does not notice the Rogue in view");
    check(move_to(*c, {4, 3}) && move_to(*c, {5, 2}) && unit(*c, 1).hidden,
          "The Rogue walks up unseen");
    check(submit(*c, "melee", 98) && logged(*c, "(advantage)") &&
              logged(*c, "Rogue is no longer hidden.") && !unit(*c, 1).hidden,
          "The attack has Advantage, and the roll ends hiding");
}

// A monster that sees no enemy does not attack the hidden one beside it: it
// searches, and finds the Rogue when its check reaches the Stealth total.
void search_checks()
{
    const auto rules = module();
    const auto hero = rogue();
    bool found = false, missed = false;
    for (std::uint64_t seed = 1; seed < 60 && !(found && missed); ++seed)
    {
        auto c = fight(*rules, hero, "dull", {8, 1}, seed);
        if (!submit(*c, "cunning_hide") || !unit(*c, 1).hidden)
        {
            continue;
        }
        check(move_to(*c, {3, 2}) && move_to(*c, {3, 3}) && move_to(*c, {3, 4}) &&
                  submit(*c, "end"),
              "The Rogue steps into view and ends its turn");
        check(c->snapshot().actor == 98, "The monster's turn");
        const auto choice = choose_demo_command(*c);
        check(choice.verb == "search", "With no enemy in sight the monster searches");
        check(c->submit(choice) && logged(*c, "Foe searches: Perception d20 "),
              "The search is logged");
        if (unit(*c, 1).hidden)
        {
            missed = true;
        }
        else
        {
            found = logged(*c, "Foe finds Rogue.");
        }
    }
    check(found && missed, "A search finds the Rogue on a high enough roll only");
}

// Casting any spell ends hiding (HIDE-1), even one with no attack roll. The
// combatant is named Rogue whatever its class.
void spell_checks()
{
    const auto rules = module();
    const auto cleric = pool_character("cleric", 1);
    for (std::uint64_t seed = 1; seed < 200; ++seed)
    {
        auto c = fight(*rules, cleric, "dull", {6, 1}, seed);
        if (!submit(*c, "hide") || !unit(*c, 1).hidden || !submit(*c, "end"))
        {
            continue;
        }
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(c->submit(choose_demo_command(*c)), "The monster acts");
        }
        if (!unit(*c, 1).hidden)
        {
            continue;
        }
        check(move_to(*c, {3, 2}) && move_to(*c, {3, 3}) && move_to(*c, {3, 4}) &&
                  unit(*c, 1).hidden,
              "The Cleric steps into view, still hidden");
        check(submit(*c, "resistance_fire", 1) && logged(*c, "Rogue is no longer hidden.") &&
                  !unit(*c, 1).hidden,
              "Resistance, with no attack roll, ends hiding");
        return;
    }
    throw std::runtime_error("No seed keeps the Cleric hidden");
}

// Every combatant in a saved fight, or vital record, with `kind`: Poisoned, or
// Heroes' Feast's immunity to it.
std::string with_effect(std::string saved, fx::EffectKind kind)
{
    for (auto at = saved.find("FX9 "); at != std::string::npos; at = saved.find("FX9 ", at + 1))
    {
        std::istringstream in(saved.substr(at));
        auto effects = fx::read_effects(in);
        const auto length =
            in.tellg() < 0 ? std::string::npos : static_cast<std::size_t>(in.tellg());
        if (kind == fx::EffectKind::poisoned)
        {
            fx::apply_poisoned(effects, {.scope = 9, .actor = 99}, "Venom", 6000);
        }
        else
        {
            fx::apply_spell_benefit(effects, {.scope = 9, .actor = 99}, "Feast", kind, 0);
        }
        std::ostringstream out;
        fx::write_effects(out, effects);
        saved.replace(at, length, out.str());
    }
    return saved;
}

// The d20 of the Rogue's Stealth check as a Bonus Action.
int stealth_die(const RulesModule &rules, const std::string &saved)
{
    const auto c = rules.restore(saved);
    check(submit(*c, "cunning_hide"), "The Rogue tries to hide");
    for (const auto &line : c->snapshot().log())
    {
        if (const auto at = line.find("Rogue Stealth check: d20 "); at != std::string::npos)
        {
            return std::stoi(line.substr(at + 25));
        }
    }
    throw std::runtime_error("No Stealth check logged");
}

// Poisoned gives ability checks Disadvantage, as it does attack rolls: the
// same fight rolls a Poisoned Rogue's Stealth no higher, and sometimes lower.
// Heroes' Feast's immunity lifts it. Outside combat, a Poisoned member forces
// a door with Disadvantage too.
void poisoned_checks()
{
    const auto rules = module();
    const auto hero = rogue();
    bool lower = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        const auto saved = fight(*rules, hero, "dull", {8, 1}, seed)->save();
        const auto poisoned = with_effect(saved, fx::EffectKind::poisoned);
        const int plain = stealth_die(*rules, saved), hindered = stealth_die(*rules, poisoned);
        check(hindered <= plain, "A Poisoned Rogue's Stealth die is the lower of two");
        lower = lower || hindered < plain;
        check(stealth_die(*rules, with_effect(poisoned, fx::EffectKind::feasted)) == plain,
              "Heroes' Feast makes the Rogue immune to Poisoned");
    }
    check(lower, "Poisoned lowers some Stealth checks");
    const auto poisoned_hide = rules->restore(
        with_effect(fight(*rules, hero, "dull", {8, 1}, 1)->save(), fx::EffectKind::poisoned));
    check(submit(*poisoned_hide, "cunning_hide") && logged(*poisoned_hide, "(disadvantage) + "),
          "The Stealth check's line shows the Disadvantage");

    VitalState vitals{hero.sheet().hit_points, false, {}};
    rules->recover(vitals, hero.sheet());
    auto poisoned = vitals;
    poisoned.resources = with_effect(vitals.resources, fx::EffectKind::poisoned);
    lower = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        RandomState plain{seed}, hindered{seed};
        const auto gear = std::vector<std::string>{"dagger"};
        const auto force = [&](const VitalState &state, RandomState &random)
        {
            return rules->roll_ability_check(hero.sheet(), state, gear, Ability::strength,
                                             "athletics", random);
        };
        const auto a = force(vitals, plain), b = force(poisoned, hindered);
        check(b.die <= a.die && b.total - b.die == a.total - a.die,
              "A Poisoned member forces a door with the lower of two dice");
        lower = lower || b.die < a.die;
    }
    check(lower, "Poisoned lowers some door checks");
}
} // namespace

int main()
{
    try
    {
        hiding_checks();
        finding_checks();
        search_checks();
        spell_checks();
        poisoned_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Hide tests passed\n";
}
