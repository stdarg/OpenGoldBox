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

// A high-HP, AC 1 target of the given creature type and Constitution save.
std::unique_ptr<RulesModule> rules_for(std::string type, int constitution_save = 0)
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "saves target 0 0 " + std::to_string(constitution_save) +
                               " 0 0 0\ntype target " + type + "\n");
}

// A level-two Paladin who prepared Cure Wounds and Searing Smite.
Character paladin()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "paladin";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Smiter";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds", "searing_smite"}, {}, {}};
    Character c(*srd5::character_rules(), d, {});
    auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    VitalState state{c.sheet().hit_points, false, {}};
    check(c.advance(*rules, state, rules->default_advancement(c.sheet())), "Reach level two");
    return c;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

std::vector<std::string> smites(const CombatSession &c)
{
    std::vector<std::string> result;
    for (const auto &command : c.legal_commands())
        if (command.verb.find("smite") != std::string::npos)
            result.push_back(command.verb);
    std::sort(result.begin(), result.end());
    return result;
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto offered = c.legal_commands();
    for (const auto &command : offered)
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

// Level-one slots are the third field of the saved vital state.
unsigned slots(const CombatSession &c)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    unsigned winds{}, level_one{};
    in >> magic >> winds >> level_one;
    return level_one;
}

unsigned resource(const CombatSession &c, std::string_view id)
{
    for (const auto &pool : unit(c, 1).resources)
        if (pool.id == id)
            return pool.remaining;
    return 0;
}

// The Paladin beside the target with a longsword (and a shortbow, for the
// ranged case), on the Paladin's turn.
std::unique_ptr<CombatSession> battle(const RulesModule &rules, const Character &hero,
                                      std::vector<std::string> gear = {"longsword"})
{
    const auto profile = rules.character_profile(hero.sheet(), gear).data;
    auto c = rules.create({{8, 4, std::vector<Terrain>(32)},
        {   {1, "campaign-character", "Smiter", 0, {1, 1}, profile},
            {99, "target", "Target", 1, {2, 1}}
        }},
    3);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Paladin's turn");
    check(c->snapshot().actor == 1, "The Paladin acts");
    return c;
}

// Attacks in melee until a hit leaves the smite window open.
void hit(CombatSession &c)
{
    check(smites(c).empty(), "No smite is offered before a hit");
    check(submit(c, "melee", 99), "The Paladin attacks");
    check(!smites(c).empty(), "A melee hit on the AC 1 target opens the smite window");
}

void window_checks()
{
    auto rules = rules_for("humanoid");
    const auto hero = paladin();
    auto c = battle(*rules, hero);
    hit(*c);
    check(smites(*c) == std::vector<std::string> {"divine_smite", "divine_smite_free", "searing_smite"},
          "Divine Smite, Paladin's Smite and a prepared Searing Smite follow the hit");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved && smites(*rules->restore(saved)) == smites(*c),
          "The open window survives a combat checkpoint");
    check(submit(*c, "end"), "End the turn");
    check(smites(*c).empty(), "Any other command ends the window");
}

int smite_damage(const RulesModule &rules, std::string_view verb)
{
    auto c = battle(rules, paladin());
    hit(*c);
    const int before = unit(*c, 99).hit_points;
    const unsigned slots_before = slots(*c), free = resource(*c, "paladins_smite");
    check(submit(*c, verb, 99), "The smite resolves");
    check(!unit(*c, 1).bonus_action && smites(*c).empty(), "A smite spends the Bonus Action");
    if (verb == "divine_smite_free")
        check(resource(*c, "paladins_smite") == free - 1 && slots(*c) == slots_before,
              "Paladin's Smite spends its free use, not a slot");
    else
        check(slots(*c) == slots_before - 1 && resource(*c, "paladins_smite") == free,
              "A smite cast with a slot spends one slot");
    return before - unit(*c, 99).hit_points;
}

void divine_smite_checks()
{
    const auto humanoid = rules_for("humanoid"), fiend = rules_for("fiend");
    const int free = smite_damage(*humanoid, "divine_smite_free");
    check(free >= 2 && free <= 16, "Divine Smite deals 2d8 Radiant damage");
    check(smite_damage(*humanoid, "divine_smite") == free,
          "The free and slot casts roll the same dice");
    check(smite_damage(*fiend, "divine_smite_free") > free,
          "A Fiend takes an extra 1d8 from the same rolls");
}

void searing_smite_checks()
{
    for (const int constitution :
            {
                -10, 30
            })
    {
        auto rules = rules_for("humanoid", constitution);
        auto c = battle(*rules, paladin());
        hit(*c);
        const int before = unit(*c, 99).hit_points;
        check(submit(*c, "searing_smite", 99), "Searing Smite resolves");
        const int first = before - unit(*c, 99).hit_points;
        check(first >= 1 && first <= 6, "Searing Smite adds 1d6 Fire damage to the hit");
        check(submit(*c, "end"), "End the Paladin's turn");
        // The target's turn began: it burned, then saved.
        const auto log = c->snapshot().log();
        check(std::any_of(log.begin(), log.end(), [](const auto & line)
        {
            return line.starts_with("Target burns for ");
        }),
        "The target burns at the start of its turn");
        check(rules->restore(c->save())->save() == c->save(),
              "The burning effect survives a checkpoint");
        while (c->snapshot().actor != 1)
            check(submit(*c, "end"), "Return to the Paladin");
        const auto so_far = c->snapshot().log();
        const auto burned_again = std::count_if(so_far.begin(), so_far.end(), [](const auto & line)
        {
            return line.starts_with("Target burns for ");
        });
        check(burned_again == 1, "One burn per target turn so far");
        while (c->snapshot().actor != 99)
            check(submit(*c, "end"), "Reach the target's next turn");
        const auto later = c->snapshot().log();
        const auto burns = std::count_if(later.begin(), later.end(), [](const auto & line)
        {
            return line.starts_with("Target burns for ");
        });
        check(burns == (constitution > 0 ? 1 : 2),
              "A successful Constitution save ends the burning; a failed one continues it");
    }
}

// A Paladin who just hit a vanguard, for tests/smite_view_tests.gd. The game
// loads it with the standard rules content, so no custom target is used.
void write_ui_fixture()
{
    auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = rules->character_profile(paladin().sheet(), std::vector<std::string> {"longsword"}).data;
    for (std::uint64_t seed = 1; seed <= 64; ++seed)
    {
        auto c = rules->create({{8, 4, std::vector<Terrain>(32)},
            {   {1, "campaign-character", "Smiter", 0, {1, 1}, profile},
                {99, "vanguard", "Target", 1, {2, 1}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "end"), "Reach the Paladin's turn");
        if (c->snapshot().actor != 1 || !submit(*c, "melee", 99) || smites(*c).empty())
            continue;
        const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "smite-fixtures";
        std::filesystem::create_directories(path);
        std::ofstream out(path / "after-hit.save", std::ios::binary);
        out << c->save();
        check(bool(out), "Write the UI fixture");
        return;
    }
    throw std::runtime_error("No seed lands a melee hit");
}

void campaign_checks()
{
    auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(paladin());
    const auto free_smite = [&]
    {
        for (const auto &pool :
                rules->recovery_info(party.member(id).character.sheet(), party.member(id).vitals)
                .resources)
            if (pool.id == "paladins_smite")
                return pool;
        throw std::runtime_error("Missing Paladin's Smite");
    };
    check(free_smite().capacity == 1 && free_smite().remaining == 1 &&
          !free_smite().short_rest_recovery,
          "Paladin's Smite has one free use, restored only by a Long Rest");
}
} // namespace

int main()
{
    try
    {
        window_checks();
        divine_smite_checks();
        searing_smite_checks();
        campaign_checks();
        write_ui_fixture();
        std::cout << "Smite tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
