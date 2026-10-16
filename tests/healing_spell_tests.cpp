#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Cure Wounds and Healing Word through character level 4 (SRD 5.2.1 pp. 121,
// 139; issues #167 and #169): every class route heals with its own casting
// ability, a level-2 slot adds two dice, Cure Wounds is an Action at Touch and
// Healing Word a Bonus Action at 60 feet, and a refused cast changes nothing.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// The SRD rules plus a test creature with 1000 Hit Points, so no heal is capped.
std::unique_ptr<RulesModule> rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(content +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A Character Pool draft of the class, as a player adds one.
CharacterDraft pool_draft(std::string_view klass)
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
            return preset.creation_data();
        }
    }
    throw std::runtime_error("No pool character of the class");
}

// The class's caster at `level`, with `spell` prepared and the casting ability
// rolled from four dice whose best three are `best`.
Character caster(std::string_view klass, const std::string &spell, Ability ability, int best,
                 unsigned level = 1)
{
    auto d = pool_draft(klass);
    check(d.spells.has_value(), std::string(klass) + " chooses spells");
    auto &prepared = d.spells->prepared;
    check(prepared && !prepared->empty(), std::string(klass) + " prepares level-one spells");
    if (std::find(prepared->begin(), prepared->end(), spell) == prepared->end())
    {
        prepared->back() = spell;
    }
    d.rolls[static_cast<std::size_t>(ability)] = {{best, best, best, 1}, 3};
    CampaignParty party(rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "healing:levels");
    for (unsigned n = 1; n < level; ++n)
    {
        party.advance(id, party.default_advancement(id));
    }
    return party.member(id).character;
}

int modifier(const Character &c, Ability ability)
{
    return c.sheet().scores[ability] / 2 - 5;
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

bool offers(const CombatSession &c, std::string_view verb, EntityId target)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && command.target == target;
                       });
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

// The caster at (1,1), a badly wounded ally beside it at (1,2), another at
// (1,5), 20 feet away, and an enemy far off.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero)
{
    const auto profile = module.character_profile(hero.sheet(), {}).data;
    auto c = module.create(
        {{14, 8, std::vector<Terrain>(112)},
         {{1, "campaign-character", "Healer", Side::party, {1, 1}, profile},
          {2, "target", "Near ally", Side::party, {1, 2}, {}, VitalState{1, false, {}, {}}},
          {3, "target", "Far ally", Side::party, {1, 5}, {}, VitalState{1, false, {}, {}}},
          {99, "target", "Enemy", Side::opposition, {12, 7}}}},
        11);
    // Alert first offers to swap initiative; keep it.
    (void)submit(*c, "initiative_keep");
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
    {
        check(submit(*c, "end"), "Reach the healer's turn");
    }
    check(c->snapshot().actor == 1, "The healer acts");
    return c;
}

// HP the target regains from casting `verb` at it.
int healed(const RulesModule &module, const Character &hero, std::string_view verb, EntityId target)
{
    auto c = battle(module, hero);
    const int before = unit(*c, target).hit_points;
    check(submit(*c, verb, target), std::string(verb) + " is cast");
    return unit(*c, target).hit_points - before;
}

struct Route
{
    std::string klass, spell;
    Ability ability;
    bool second_slot; // The class has level-2 slots by level 3.
};

void routes()
{
    const auto module = rules();
    const std::vector<Route> all{
        {"cleric", "cure_wounds", Ability::wisdom, true},
        {"bard", "cure_wounds", Ability::charisma, true},
        {"druid", "cure_wounds", Ability::wisdom, true},
        {"paladin", "cure_wounds", Ability::charisma, false},
        {"ranger", "cure_wounds", Ability::wisdom, false},
        {"cleric", "healing_word", Ability::wisdom, true},
        {"bard", "healing_word", Ability::charisma, true},
        {"druid", "healing_word", Ability::wisdom, true},
    };
    for (const auto &route : all)
    {
        const auto label = route.klass + " " + route.spell;
        const auto strong = caster(route.klass, route.spell, route.ability, 6),
                   weak = caster(route.klass, route.spell, route.ability, 4);
        const int gap = modifier(strong, route.ability) - modifier(weak, route.ability);
        check(gap > 0, label + ": the two casters differ in their casting ability");
        check(healed(*module, strong, route.spell, 2) ==
                  healed(*module, weak, route.spell, 2) + gap,
              label + ": heals with the class's casting ability");

        // Cure Wounds touches; Healing Word reaches the ally 20 feet away.
        auto c = battle(*module, strong);
        const bool touch = route.spell == "cure_wounds";
        check(offers(*c, route.spell, 3) == !touch,
              label + (touch ? ": Touch range only" : ": reaches 60 feet"));
        check(submit(*c, route.spell, 2) && unit(*c, 1).action == !touch &&
                  unit(*c, 1).bonus_action == touch,
              label + (touch ? ": takes the Action" : ": takes the Bonus Action"));

        if (!route.second_slot)
        {
            continue;
        }
        // A level-2 slot rolls two more dice after the same first two.
        const auto third = caster(route.klass, route.spell, route.ability, 6, 3);
        const int die = touch ? 8 : 4;
        const int one = healed(*module, third, route.spell, 2),
                  two = healed(*module, third, route.spell + "_2", 2);
        check(two >= one + 2 && two <= one + 2 * die, label + ": a level-2 slot adds two dice");
    }
}

// A refused cast leaves the fight exactly as it was.
void refusals()
{
    const auto module = rules();
    const auto hero = caster("cleric", "cure_wounds", Ability::wisdom, 6);
    auto c = battle(*module, hero);
    const auto saved = c->save();
    Command far;
    far.verb = "cure_wounds";
    far.target = 3;
    check(!c->submit(far) && c->save() == saved, "Cure Wounds out of reach is refused atomically");
    check(submit(*c, "cure_wounds", 2), "Cure Wounds is cast");
    const auto spent = c->save();
    check(!submit(*c, "cure_wounds", 2) && c->save() == spent,
          "A second Action spell in the turn is refused atomically");
}

// Disciple of Life adds 2 + the slot's level to Healing Word too.
void disciple_of_life()
{
    const auto module = rules();
    const auto before = caster("cleric", "healing_word", Ability::wisdom, 6, 2),
               life = caster("cleric", "healing_word", Ability::wisdom, 6, 3);
    check(healed(*module, life, "healing_word", 2) ==
              healed(*module, before, "healing_word", 2) + 3,
          "Disciple of Life adds 2 + the slot's level to Healing Word");
}
} // namespace

int main()
{
    try
    {
        routes();
        refusals();
        disciple_of_life();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Healing spell tests passed\n";
}
