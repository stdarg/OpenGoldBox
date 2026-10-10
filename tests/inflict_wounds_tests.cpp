#include "opengold/character.h"
#include "opengold/srd5.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
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

// A high-HP target whose Constitution save always fails (-10) or always succeeds (+30).
std::unique_ptr<RulesModule> rules_with_constitution_save(int bonus)
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "saves target 0 0 " + std::to_string(bonus) + " 0 0 0\n");
}

// A created Cleric with Inflict Wounds prepared, advanced to `level`.
CharacterSheet cleric(unsigned level)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "neutral_good";
    d.name = "Inflict tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::vector<std::string> {"sacred_flame"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"cure_wounds", "healing_word", "inflict_wounds"}, {}, {}};
    Character c(*srd5::character_rules(), d, {});
    auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    VitalState state{c.sheet().hit_points, false, {}};
    for (unsigned n = 1; n < level; ++n)
        check(c.advance(*rules, state, rules->default_advancement(c.sheet())), "Advance the Cleric");
    return c.sheet();
}

int hit_points(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a.hit_points;
    throw std::runtime_error("Missing combatant");
}

std::optional<Command> find(const CombatSession &c, std::string_view verb, EntityId target)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.target == target)
            return command;
    return std::nullopt;
}

// A level-one Cleric beside an enemy, for tests/inflict_view_tests.gd.
void write_ui_fixture()
{
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "inflict-fixtures";
    std::filesystem::create_directories(path);
    auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const std::vector<std::string> gear{"quarterstaff"};
    const auto profile = rules->character_profile(cleric(1), gear);
    auto c = rules->create({{12, 9, std::vector<Terrain>(108)},
        {   {1, "campaign-character", "Inflict Cleric", 0, {1, 1}, profile.data},
            {99, "vanguard", "Enemy", 1, {2, 1}}
        }},
    2);
    std::ofstream out(path / "adjacent.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");
}

// Casts `verb` at the adjacent enemy and returns the damage it took.
int damage(const RulesModule &rules, const CharacterSheet &sheet, std::string_view verb,
           int enemy_column = 2)
{
    const auto profile = rules.character_profile(sheet, {});
    auto c = rules.create({{8, 4, std::vector<Terrain>(32)},
        {   {1, "campaign-character", "Cleric", 0, {1, 1}, profile.data},
            {3, "target", "Enemy", 1, {enemy_column, 1}}
        }},
    13);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        const auto end = find(*c, "end", 0);
        check(end && c->submit(*end), "Reach the Cleric's turn");
    }
    const auto cast = find(*c, verb, 3);
    if (!cast)
        return -1;
    const int before = hit_points(*c, 3);
    check(c->submit(*cast), "Inflict Wounds resolves");
    return before - hit_points(*c, 3);
}
} // namespace

int main()
{
    try
    {
        const auto failing = rules_with_constitution_save(-10);
        const auto saving = rules_with_constitution_save(30);
        const auto first = cleric(1);
        const int full = damage(*failing, first, "inflict_wounds");
        check(full >= 2 && full <= 20, "A failed Constitution save takes 2d10 Necrotic damage");
        check(damage(*saving, first, "inflict_wounds") == full / 2,
              "A successful save takes half the same roll, rounded down");
        check(damage(*failing, first, "inflict_wounds", 3) == -1,
              "Inflict Wounds needs a touched, adjacent creature");

        const auto third = cleric(3);
        const int upcast = damage(*failing, third, "inflict_wounds_2");
        check(upcast >= 3 && upcast <= 30, "A level-two slot adds 1d10");
        check(damage(*saving, third, "inflict_wounds_2") == upcast / 2,
              "The upcast damage is also halved on a success");
        write_ui_fixture();
        std::cout << "Inflict Wounds tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
