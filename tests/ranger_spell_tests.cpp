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

// A sturdy AC 1 target that hits back and a one-Hit-Point weakling.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "creature weakling 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A level-one Ranger who prepared the named spells; Hunter's Mark is always prepared.
Character ranger(std::vector<std::string> prepared)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "male";
    d.character_class = "ranger";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Ranger";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    return Character(*srd5::character_rules(), d, {});
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
    const auto offered = c.legal_commands();
    for (const auto &command : offered)
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Level-one slots, read from the vital record "SRD11 winds slots ...".
int slots(const CombatSession &c)
{
    std::istringstream in(unit(c, 1).persistent.resources);
    std::string magic;
    int winds{}, level_one{};
    in >> magic >> winds >> level_one;
    return level_one;
}

unsigned remaining(const CombatSession &c, std::string_view pool)
{
    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == pool)
            return resource.remaining;
    return 0;
}

// The Ranger (1) with a weakling (98) and a sturdy target (99) beside it.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero)
{
    const auto profile =
        module.character_profile(hero.sheet(), std::vector<std::string> {"longsword"}).data;
    auto c = module.create({{8, 4, std::vector<std::uint8_t>(32)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, profile},
            {98, "weakling", "Weakling", 1, {2, 1}},
            {99, "target", "Target", 1, {2, 2}}
        }},
    5);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    check(c->snapshot().actor == 1, "The Ranger acts");
    return c;
}

// Ends the Ranger's turn and returns to it.
void next_turn(CombatSession &c)
{
    check(submit(c, "end"), "End the Ranger's turn");
    for (unsigned turns = 0; c.snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(c, "end"), "Back to the Ranger");
}

void favored_enemy_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds"}));
    check(remaining(*c, "favored_enemy") == 2 && slots(*c) == 2,
          "Favored Enemy: two free casts beside two slots");
    check(submit(*c, "hunters_mark_free", 99) && !unit(*c, 1).bonus_action &&
          unit(*c, 1).action && remaining(*c, "favored_enemy") == 1 &&
          slots(*c) == 2,
          "A free Hunter's Mark is a Bonus Action that spends no slot");
    const auto saved = c->save();
    check(module->restore(saved)->save() == saved, "The mark survives a checkpoint");
    next_turn(*c);
    check(!submit(*c, "hunters_mark_free", 98), "No free cast while the quarry still stands");
    check(submit(*c, "melee", 99) && logged(*c, "Force damage from Hunter's Mark"),
          "A hit on the quarry adds Hunter's Mark's 1d6 Force damage");
}

void move_mark_checks()
{
    auto module = rules();
    auto moved = battle(*module, ranger({"cure_wounds"}));
    check(submit(*moved, "hunters_mark_free", 98) && submit(*moved, "melee", 98), "Drop the quarry");
    next_turn(*moved);
    check(submit(*moved, "hunters_mark_move", 99) && logged(*moved, "Ranger moves Hunter's Mark to Target.") &&
          remaining(*moved, "favored_enemy") == 1,
          "A dropped quarry's mark moves for a Bonus Action without a new cast");
    check(submit(*moved, "melee", 99) && logged(*moved, "Target takes"),
          "The new quarry is marked");
}

void slot_cast_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds"}));
    check(submit(*c, "hunters_mark", 99) && slots(*c) == 1 &&
          remaining(*c, "favored_enemy") == 2,
          "Hunter's Mark from a slot keeps the free casts");
}

void longstrider_checks()
{
    auto module = rules();
    auto c = battle(*module, ranger({"cure_wounds", "longstrider"}));
    const int before = unit(*c, 1).movement_feet;
    check(submit(*c, "longstrider", 1) && !unit(*c, 1).action &&
          unit(*c, 1).movement_feet == before + 10,
          "Longstrider adds 10 feet of Speed at once");
    next_turn(*c);
    check(unit(*c, 1).movement_feet == before + 10, "Longstrider lasts into later turns");
}
// A Ranger with a vanguard 25 feet away on its turn, for
// tests/hunters_mark_view_tests.gd. The game loads it with the standard rules content.
void write_ui_fixture()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto profile = module->character_profile(ranger({"cure_wounds", "longstrider"}).sheet(),
                         std::vector<std::string> {"longsword"}).data;
    auto c = module->create({{12, 9, std::vector<std::uint8_t>(108)},
        {   {1, "campaign-character", "Ranger", 0, {1, 1}, profile},
            {99, "vanguard", "Enemy", 1, {6, 1}}
        }},
    2);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Ranger's turn");
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "ranger-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream out(path / "ranger.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");
}
} // namespace

int main()
{
    try
    {
        favored_enemy_checks();
        move_mark_checks();
        slot_cast_checks();
        longstrider_checks();
        write_ui_fixture();
        std::cout << "Ranger spell tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
