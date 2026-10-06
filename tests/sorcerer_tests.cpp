#include "opengold/campaign_party.h"
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

std::unique_ptr<RulesModule> module_rules()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A Sorcerer with Dexterity 15 and Charisma 15, advanced to the given level.
Character sorcerer(unsigned level = 1)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "sorcerer";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Sorcerer";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.training = {{"class:sorcerer", {"arcana", "insight"}}};
    d.cantrips = {"sorcerous_burst", "fire_bolt", "ray_of_frost", "shocking_grasp"};
    d.spells = SpellChoices{{}, std::vector<std::string> {"magic_missile", "burning_hands"}, {}, {}};
    CampaignParty party(module_rules());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "sorcerer-xp");
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
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

unsigned pool(const CombatSession &c, std::string_view id)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == id)
            return resource.remaining;
    throw std::runtime_error("Missing resource pool");
}

// Level-one slots, read from "SRD11 winds slots slots2 ...".
int first_level_slots(const CombatSession &c)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    int winds{}, slots{};
    in >> magic >> winds >> slots;
    return slots;
}

bool has_grant(const Character &hero, std::string_view id)
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

// The Sorcerer (1) and an enemy (98) 15 feet away.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero)
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Sorcerer", 0, {1, 1}, profile},
            {98, "target", "Enemy", 1, {4, 1}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Sorcerer's turn");
    check(c->snapshot().actor == 1, "The Sorcerer acts");
    return c;
}

void spellcasting_checks()
{
    const auto hero = sorcerer();
    check(hero.sheet().prepared_spells.size() == 2, "A level-one Sorcerer prepares two spells");
    auto module = rules();
    auto c = battle(*module, hero);
    check(offered(*c, "magic_missile") && offered(*c, "burning_hands") &&
          offered(*c, "sorcerous_burst_fire") && offered(*c, "sorcerous_burst_psychic") &&
          offered(*c, "fire_bolt"),
          "Prepared spells and cantrips, Sorcerous Burst once per damage type");
    check(submit(*c, "sorcerous_burst_psychic", 98) && logged(*c, "Sorcerer -> Enemy"),
          "Sorcerous Burst is a spell attack");
    auto rules_module = module_rules();
    const auto options = rules_module->spell_choice_options(hero.sheet(), SpellChoiceContext::long_rest);
    check(!options.may_prepare, "A Long Rest does not change a Sorcerer's prepared spells");
}

void innate_sorcery_checks()
{
    auto module = rules();
    auto c = battle(*module, sorcerer());
    check(pool(*c, "innate_sorcery") == 2 && submit(*c, "innate_sorcery") &&
          pool(*c, "innate_sorcery") == 1 && unit(*c, 1).action && !offered(*c, "innate_sorcery"),
          "Innate Sorcery is a Bonus Action with two uses");
    check(submit(*c, "fire_bolt", 98) && logged(*c, "(advantage)"),
          "Innate Sorcery gives Advantage on spell attack rolls");
}

void draconic_checks()
{
    const auto third = sorcerer(3), fourth = sorcerer(4);
    check(has_grant(third, "subclass:draconic") && has_grant(third, "feature:draconic_resilience") &&
          has_grant(third, "feature:draconic_spells"),
          "Level three brings Draconic Sorcery");
    // d6 + Constitution at level one and 4 + Constitution per later level, plus 3
    // from Draconic Resilience.
    const int con = third.sheet().modifiers[2];
    check(third.sheet().hit_points == (6 + con) + 2 * (4 + con) + 3,
          "Draconic Resilience adds 3 Hit Points");
    check(fourth.sheet().hit_points == third.sheet().hit_points + 4 + con + 1,
          "And one more at each later level");
    check(fourth.sheet().prepared_spells.size() == 7, "A level-four Sorcerer prepares seven spells");
    auto module = rules();
    auto c = battle(*module, third);
    check(unit(*c, 1).armor_class == 10 + 2 + 2, "Unarmored AC is 10 + Dexterity + Charisma");
    check(offered(*c, "chromatic_orb_fire") && offered(*c, "command_halt") &&
          offered(*c, "scorching_ray") && offered(*c, "dragons_breath_fire"),
          "Draconic Spells are always prepared, with level-two spells");
}

void font_of_magic_checks()
{
    const auto second = sorcerer(2);
    check(has_grant(second, "feature:font_of_magic"), "Level two brings Font of Magic");
    auto module = rules();
    auto c = battle(*module, second);
    check(pool(*c, "sorcery_points") == 2 && first_level_slots(*c) == 3 &&
          !offered(*c, "convert_slot_1") && !offered(*c, "create_slot_2"),
          "Two Sorcery Points; a full pool takes no more, and level-2 slots wait for level three");
    check(submit(*c, "create_slot_1") && pool(*c, "sorcery_points") == 0 &&
          first_level_slots(*c) == 4 && !unit(*c, 1).bonus_action,
          "A Bonus Action turns 2 Sorcery Points into a level-1 slot");
    check(submit(*c, "convert_slot_1") && pool(*c, "sorcery_points") == 1 &&
          first_level_slots(*c) == 3 && unit(*c, 1).action,
          "A slot becomes Sorcery Points without an action");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "Sorcery Points survive a checkpoint");
}

} // namespace

int main()
{
    try
    {
        spellcasting_checks();
        innate_sorcery_checks();
        draconic_checks();
        font_of_magic_checks();
        std::cout << "Sorcerer tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
