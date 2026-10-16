#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
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

// A Draconic Sorcerer from level 5 to 15 (LEVELS-1, LEVELS-2, LEVELS-3): the
// Sorcerer's level-3 to level-8 spells, Draconic Spells, Sorcerous
// Restoration, Elemental Affinity, Sorcery Incarnate, more Metamagic, Font of
// Magic's higher slots and Dragon Wings.
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

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// An AC 1 dummy.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

CharacterDraft draft()
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
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:sorcerer", {"deception", "insight"}}};
    d.cantrips = {"sorcerous_burst", "fire_bolt", "ray_of_frost", "shocking_grasp"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"magic_missile", "burning_hands"}, {}, {}};
    return d;
}

// A Sorcerer of the level, advanced through the rules' default level-ups,
// with `spell` prepared from the level that allows it.
Character sorcerer(unsigned level, const std::string &spell = {})
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (!spell.empty() && hero.sheet().level + 1 == static_cast<int>(level) &&
            std::find(choice.spells.begin(), choice.spells.end(), spell) == choice.spells.end())
        {
            choice.spells.back() = spell;
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb && (!target || command.target == target);
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

bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && command.destination == cell)
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

// Spell slots of a level left: from the third field of the saved vital state.
unsigned slots(const CombatSession &c, unsigned level, EntityId id = 1)
{
    std::istringstream in(unit(c, id).persistent.resources);
    std::string magic;
    unsigned winds{}, count{};
    in >> magic >> winds;
    for (unsigned n = 1; n <= level; ++n)
    {
        in >> count;
    }
    return count;
}

void to_turn(CombatSession &c, EntityId id)
{
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 12; ++turns)
    {
        check(submit(c, "end"), "End a turn");
    }
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Sorcerer (1) at (1, 2) and a dummy, on the Sorcerer's turn.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet,
                                     Cell foe = {9, 2})
{
    const std::vector<std::string> gear{"quarterstaff"};
    auto c = rules.create({{14, 7, std::vector<Terrain>(98)},
                           {{1,
                             "campaign-character",
                             "Sorcerer",
                             Side::party,
                             {1, 2},
                             rules.character_profile(sheet, gear).data},
                            {98, "dummy", "Foe", Side::opposition, foe}}},
                          1);
    to_turn(*c, 1);
    return c;
}

bool granted(const CharacterSheet &sheet, std::string_view id, unsigned level)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(),
                       [&](const auto &g)
                       {
                           return g.id == id && g.level == level;
                       });
}

void progression_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
    {
        party.advance(id, party.default_advancement(id));
    }
    const auto sheet = party.member(id).character.sheet();
    check(sheet.level == 15, "A Sorcerer reaches 15");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>{{5, 9},
                                                                                 {6, 10},
                                                                                 {7, 11},
                                                                                 {8, 12},
                                                                                 {9, 14},
                                                                                 {10, 15},
                                                                                 {11, 16},
                                                                                 {12, 16},
                                                                                 {13, 17},
                                                                                 {14, 17},
                                                                                 {15, 18}})
    {
        check(rules->spell_access(sorcerer(level).sheet()).prepared_choices == count,
              "Prepared spells: 9 at 5 to 18 at 15");
    }
    check(rules->spell_access(sorcerer(10).sheet()).cantrip_choices == 6, "A sixth cantrip at 10");
    check(granted(sheet, "feature:sorcerous_restoration", 5) &&
              granted(sheet, "feature:sorcery_incarnate", 7) &&
              granted(sheet, "feature:dragon_wings", 14),
          "Sorcerous Restoration at 5, Sorcery Incarnate at 7, Dragon Wings at 14");
    check(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                        [](const auto &g)
                        {
                            return g.id.starts_with("affinity:") && g.level == 6;
                        }) == 1,
          "Elemental Affinity's type at 6");
    check(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                        [](const auto &g)
                        {
                            return g.id.starts_with("metamagic:");
                        }) == 4,
          "Two more Metamagic options at 10");
    const auto dummies = dummy_rules();
    for (const auto &[level, slot] : std::vector<std::pair<unsigned, unsigned>>{
             {5, 3}, {7, 4}, {9, 5}, {11, 6}, {13, 7}, {15, 8}})
    {
        auto c = field(*dummies, sorcerer(level).sheet());
        check(slots(*c, slot) >= 1 && slots(*c, slot + 1) == 0,
              "A new slot level every two levels");
    }
    check(
        rules->advancement_options(sorcerer(4).sheet()).description.find("Sorcerous Restoration") !=
            std::string::npos,
        "The level's description names its feature");
    const auto bytes = encode_campaign(party, nullptr, "sorcerer-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "sorcerer-levels", nullptr)
                     .party);
    check(encode_campaign(next, nullptr, "sorcerer-levels") == bytes,
          "A level-15 Sorcerer saves and reloads");
}

void draconic_spells_checks()
{
    const auto rules = dummy_rules();
    auto c5 = field(*rules, sorcerer(5).sheet());
    check(offered(*c5, "fear"), "Fear is always prepared at 5");
    auto c7 = field(*rules, sorcerer(7).sheet(), {4, 2});
    check(offered(*c7, "charm_monster", 98), "Charm Monster is always prepared at 7");
    auto c9 = field(*rules, sorcerer(9).sheet());
    check(offered(*c9, "summon_dragon_acid"), "Summon Dragon is always prepared at 9");
    auto fireball = field(*rules, sorcerer(5, "fireball").sheet());
    check(submit(*fireball, "fireball") && submit_cell(*fireball, "area_move", Cell{9, 2}) &&
              submit(*fireball, "area_cast") && logged(*fireball, " Fire damage."),
          "A level-5 Sorcerer casts Fireball");
}

void font_of_magic_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, sorcerer(9).sheet());
    check(offered(*c, "create_slot_5") && !offered(*c, "create_slot_6") &&
              submit(*c, "create_slot_5") && logged(*c, "creates a level-5 spell slot.") &&
              slots(*c, 5) == 2,
          "Font of Magic creates a level-5 slot at 9 for 7 Sorcery Points");
    check(submit(*c, "convert_slot_5") &&
              logged(*c, "turns a level-5 spell slot into Sorcery Points."),
          "A level-5 slot becomes Sorcery Points");
}

void restoration_checks()
{
    const auto rules = module();
    const auto sheet = sorcerer(6).sheet();
    VitalState state{sheet.hit_points, false, {}};
    const auto dummies = dummy_rules();
    auto c = field(*dummies, sheet);
    check(submit(*c, "create_slot_2"), "Spend Sorcery Points");
    state = unit(*c, 1).persistent;
    const auto info = rules->recovery_info(sheet, state);
    check(std::any_of(info.choices.begin(), info.choices.end(),
                      [](const auto &choice)
                      {
                          return choice.id == "sorcerous_restoration";
                      }),
          "Sorcerous Restoration is a Short Rest choice");
    (void)rules->recover_rest_choice(state, sheet, "sorcerous_restoration");
    const auto after = rules->recovery_info(sheet, state);
    check(std::none_of(after.choices.begin(), after.choices.end(),
                       [](const auto &choice)
                       {
                           return choice.id == "sorcerous_restoration";
                       }),
          "Once per Long Rest");
}

void incarnate_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, sorcerer(7, "fireball").sheet());
    // Two Innate Sorcery uses spent over two turns, then 2 Sorcery Points.
    check(submit(*c, "innate_sorcery"), "Innate Sorcery");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(!offered(*c, "innate_sorcery_points"), "A use is left");
    check(submit(*c, "metamagic_careful") && submit(*c, "metamagic_distant"),
          "Sorcery Incarnate readies two options while Innate Sorcery is active");
    check(submit(*c, "fireball") && submit_cell(*c, "area_move", Cell{9, 2}) &&
              submit(*c, "area_cast") && logged(*c, "Sorcerer uses Careful Spell.") &&
              logged(*c, "Sorcerer uses Distant Spell."),
          "Both options change one spell");
}

void wings_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, sorcerer(14).sheet());
    check(submit(*c, "dragon_wings") && logged(*c, "Sorcerer spreads dragon wings.") &&
              unit(*c, 1).status.find("Dragon Wings") != std::string::npos,
          "Dragon Wings");
    check(!offered(*c, "dragon_wings") && !offered(*c, "dragon_wings_points"),
          "The wings last their hour");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        draconic_spells_checks();
        font_of_magic_checks();
        restoration_checks();
        incarnate_checks();
        wings_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Sorcerer level checks passed\n";
}
