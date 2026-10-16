#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <cstdlib>
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

// A Circle of the Land Druid from level 5 to 10 (LEVELS-1, LEVELS-2): the
// Druid's level-3 to level-5 spells (Call Lightning, Giant Insect, Antilife
// Shell), Wild Resurgence, the later Circle Spells, Natural Recovery,
// Elemental Fury, the Tiger form, Nature's Ward and Land's Aid's 3d6.
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

CharacterDraft draft()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "druid";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Druid";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:druid", {"nature", "medicine"}}, {"class:druid:primal_order", {"warden"}}};
    d.cantrips = {"produce_flame", "shillelagh"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds", "entangle", "faerie_fire",
                                                          "thunderwave"}, {}, {}};
    return d;
}

// An arid Druid of the level through the default level-ups, with Primal
// Strike at 7 and `spell` prepared at the last.
Character druid(unsigned level, const std::string &spell = {}, std::string fury = "primal_strike")
{
    Character hero(*srd5::character_rules(), draft(), {});
    VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (hero.sheet().level + 1 == 3)
            choice.training["subclass:druid:land"] = {"arid"};
        if (hero.sheet().level + 1 == 7)
            choice.training["class:druid:elemental_fury"] = {fury};
        if (!spell.empty() && hero.sheet().level + 1 == static_cast<int>(level) &&
                std::find(choice.spells.begin(), choice.spells.end(), spell) == choice.spells.end())
            choice.spells.back() = spell;
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

bool submit_cell(CombatSession &c, std::string_view verb, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.destination == cell)
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
        check(submit(c, "end"), "End a turn");
    check(c.snapshot().actor == id, "Reach the turn");
}

// The Druid (1) at (1, 2) with a quarterstaff, and a creature at `foe`.
std::unique_ptr<CombatSession> field(const RulesModule &rules, const CharacterSheet &sheet,
                                     Cell foe = {2, 2}, std::string kind = "dummy")
{
    const std::vector<std::string> gear{"quarterstaff"};
    auto c = rules.create({{14, 7, std::vector<Terrain>(98)},
        {   {1, "campaign-character", "Druid", Side::party, {1, 2}, rules.character_profile(sheet, gear).data},
            {98, std::move(kind), "Foe", Side::opposition, foe}
        }},
    1);
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
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft(), {}));
    party.award_experience(165000, "levels-xp");
    while (party.can_advance(id))
        party.advance(id, party.default_advancement(id));
    const auto sheet = party.member(id).character.sheet();
    check(sheet.level == 15, "A Druid reaches 15");
    for (const auto &[level, count] : std::vector<std::pair<unsigned, unsigned>>
{
    {5, 9}, {6, 10}, {7, 11}, {8, 12}, {9, 14}, {10, 15}
})
    check(rules->spell_access(druid(level).sheet()).prepared_choices == count,
          "Prepared spells: 9 at 5 to 15 at 10");
    check(granted(sheet, "feature:wild_resurgence") && granted(sheet, "feature:natural_recovery") &&
          granted(sheet, "feature:natures_ward") &&
          std::any_of(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.id.starts_with("fury:") && g.level == 7;
    }), "Wild Resurgence, Natural Recovery, Elemental Fury and Nature's Ward");
    check(rules->advancement_options(druid(4).sheet()).description.find("Wild Resurgence") !=
          std::string::npos, "Level 5's description names its feature");
    const auto bytes = encode_campaign(party, nullptr, "druid-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "druid-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "druid-levels") == bytes, "A level-10 Druid saves and reloads");
}

void circle_and_recovery_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, druid(6).sheet(), {9, 2});
    check(offered(*c, "fireball") && offered(*c, "fireball_natural"),
          "Fireball is an arid Circle Spell at 5; Natural Recovery casts it without a slot");
    check(submit(*c, "fireball_natural") && submit_cell(*c, "area_move", Cell{9, 2}) &&
          submit(*c, "area_cast") && logged(*c, " Fire damage."), "The free Fireball");
    check(submit(*c, "end"), "End the turn");
    to_turn(*c, 1);
    check(!offered(*c, "fireball_natural"), "Once per Long Rest");
}

void resurgence_and_fury_checks()
{
    const auto rules = dummy_rules();
    // A level-1 slot spent on Thunderwave comes back for a Wild Shape use.
    auto slot = field(*rules, druid(7).sheet());
    check(submit(*slot, "thunderwave") && submit_cell(*slot, "area_move", Cell{2, 2}) &&
          submit(*slot, "area_cast"), "Thunderwave spends a level-1 slot");
    const auto spent = unit(*slot, 1).persistent;
    check(!module()->recovery_info(druid(7).sheet(), spent).choices.empty(),
          "Natural Recovery offers the spent slot after a Short Rest");
    check(submit(*slot, "wild_resurgence_slot") &&
          logged(*slot, "Druid turns Wild Shape into a spell slot.") &&
          !offered(*slot, "wild_resurgence_slot"), "Wild Shape becomes a level-1 slot, once per Long Rest");
    auto c = field(*rules, druid(7).sheet());
    check(submit(*c, "melee", 98) && offered(*c, "primal_strike_thunder", 98) &&
          submit(*c, "primal_strike_thunder", 98) && logged(*c, "Thunder damage from Primal Strike."),
          "Primal Strike follows a hit");
}

void tiger_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*field(*rules, druid(7).sheet()), "wild_shape_tiger"), "No Tiger before 8");
    auto c = field(*rules, druid(8).sheet());
    check(submit(*c, "wild_shape_tiger") && submit(*c, "melee", 98) && logged(*c, "Foe is knocked Prone."),
          "The Tiger's Rend knocks the foe Prone");
}

void spells_checks()
{
    const auto rules = dummy_rules();
    auto lightning = field(*rules, druid(5, "call_lightning").sheet(), {6, 2});
    check(submit(*lightning, "call_lightning") && submit_cell(*lightning, "area_move", Cell{6, 2}) &&
          submit(*lightning, "area_cast") && logged(*lightning, " Lightning damage."), "Call Lightning");
    check(submit(*lightning, "end"), "End the turn");
    to_turn(*lightning, 1);
    check(submit(*lightning, "call_lightning_again") && submit_cell(*lightning, "area_move", Cell{6, 2}) &&
          submit(*lightning, "area_cast"), "Another bolt with a Magic action");
    auto insect = field(*rules, druid(7, "giant_insect").sheet(), {6, 2});
    check(submit(*insect, "giant_insect_spider") && submit_cell(*insect, "area_move", Cell{3, 2}) &&
          submit(*insect, "area_cast") && logged(*insect, "A giant insect answers Druid."), "Giant Insect");
    auto shell = field(*rules, druid(9, "antilife_shell").sheet(), {6, 2}, "brute");
    check(submit(*shell, "antilife_shell", 1) && logged(*shell, "An aura of antilife surrounds Druid."),
          "Antilife Shell");
    check(submit(*shell, "end"), "End the turn");
    to_turn(*shell, 98);
    const auto commands = shell->legal_commands();
    check(std::none_of(commands.begin(), commands.end(), [](const auto & command)
    {
        // Within 10 feet of the Druid at (1, 2).
        return command.verb == "move" && std::abs(command.destination.x - 1) <= 2 &&
               std::abs(command.destination.y - 2) <= 2;
    }), "The brute cannot enter the shell");
}
// Druid levels 11 to 15.
void high_checks()
{
    const auto rules = dummy_rules();
    auto fey = field(*rules, druid(11, "conjure_fey").sheet(), {6, 2});
    check(submit(*fey, "conjure_fey", 98) && fey->snapshot().spiritual_weapons.size() == 1 &&
          !offered(*fey, "conjure_fey_strike", 98), "Conjure Fey's spirit attacks as it appears");
    check(submit(*fey, "end"), "End the turn");
    to_turn(*fey, 1);
    check(submit(*fey, "conjure_fey_strike", 98) && logged(*fey, "Druid's fey spirit strikes."),
          "A Bonus Action strikes again");
    auto thorns = field(*rules, druid(11, "wall_of_thorns").sheet(), {6, 2});
    check(submit(*thorns, "wall_of_thorns_vertical") && submit_cell(*thorns, "area_move", Cell{6, 2}) &&
          submit(*thorns, "area_cast") && logged(*thorns, "A wall of thorns grows.") &&
          logged(*thorns, " Piercing damage."), "Wall of Thorns pierces as it rises");
    auto trees = field(*rules, druid(14).sheet(), {6, 2});
    check(submit(*trees, "natures_sanctuary") && submit_cell(*trees, "area_move", Cell{2, 2}) &&
          submit(*trees, "area_cast") && logged(*trees, "Druid raises Nature's Sanctuary.") &&
          offered(*trees, "move_sanctuary"), "Nature's Sanctuary, then a Bonus Action moves it");
    auto shapes = field(*rules, druid(15, "animal_shapes").sheet(), {6, 2});
    check(submit(*shapes, "animal_shapes_tiger", 1) && submit(*shapes, "spell_cast") &&
          logged(*shapes, "Druid turns into a Tiger.") && unit(*shapes, 1).temporary_hp.amount == 30,
          "Animal Shapes turns the Druid into a Tiger with its Hit Points as Temporary ones");
    check(submit(*shapes, "leave_animal_shape") && logged(*shapes, "Druid returns to its own shape."),
          "A Bonus Action leaves the shape");
}
// Tsunami: a 10-foot-thick wall of water that moves 50 feet away from the
// Druid at the start of its turns, carrying those in it (LEVELS-5).
void tsunami_checks()
{
    const auto rules = dummy_rules();
    auto c = field(*rules, druid(15, "tsunami").sheet(), {5, 2});
    check(submit(*c, "tsunami_vertical") && submit_cell(*c, "area_move", Cell{4, 2}) &&
          submit(*c, "area_cast") && logged(*c, "A wall of water rises.") &&
          logged(*c, "Foe Strength save:"), "The wall rises over the foe in its second row");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the wave");
    to_turn(*c, 98);
    check(submit_cell(*c, "move", Cell{5, 4}) && logged(*c, "Foe Athletics check:"),
          "Moving in the wave takes an Athletics check");
    const bool swam = logged(*c, "(swims).");
    check(swam == (unit(*c, 98).cell == (Cell{5, 4})), "A failed check holds the foe");
    to_turn(*c, 1);
    check(logged(*c, "The tsunami surges forward."), "The wave moves at the Druid's turn");
    check(unit(*c, 98).cell.x == 13, "The wave carries the foe to the field's edge");
    check(logged(*c, "Druid loses Concentration."), "Off the field, the wave is spent");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        circle_and_recovery_checks();
        resurgence_and_fury_checks();
        tiger_checks();
        spells_checks();
        high_checks();
        tsunami_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Druid level checks passed\n";
}
