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

// A Monk with Strength 9 and Dexterity 18, advanced to the given level.
Character monk(unsigned level = 1)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "monk";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Monk";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{3, 3, 3, 1}, 3};
    d.rolls[1] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:monk", {"acrobatics", "stealth"}}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "monk-xp");
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

std::vector<std::string> attack_lines(const CombatSession &c)
{
    std::vector<std::string> lines;
    for (const auto &line : c.snapshot().log)
        if (line.starts_with("Monk -> Enemy: d20 "))
            lines.push_back(line);
    return lines;
}

// The number after `before` in a line.
int number_after(const std::string &line, std::string_view before)
{
    return std::stoi(line.substr(line.find(before) + before.size()));
}

// The Monk (1) beside an enemy (98).
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::vector<std::string> equipment = {},
                                      std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(hero.sheet(), equipment).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Monk", 0, {1, 1}, profile},
            {98, "target", "Enemy", 1, {2, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Monk's turn");
    check(c->snapshot().actor == 1, "The Monk acts");
    return c;
}

void martial_arts_checks()
{
    auto module = rules();
    auto c = battle(*module, monk());
    check(submit(*c, "melee", 98) && submit(*c, "martial_arts", 98), "Strike, then strike again");
    const auto lines = attack_lines(*c);
    check(lines.size() == 2 && number_after(lines[0], " + ") == 6 &&
          number_after(lines[1], " + ") == 6,
          "Unarmed Strikes use Dexterity: +2 Proficiency and +4");
    for (const auto &line : lines)
        if (line.find(" hits for ") != std::string::npos)
        {
            const int damage = number_after(line, " hits for ");
            check(damage >= 5 && damage <= 10, "An Unarmed Strike deals 1d6 + 4");
        }
    check(!unit(*c, 1).bonus_action, "The second Unarmed Strike spent the Bonus Action");

    auto dagger = battle(*module, monk(), {"dagger"});
    check(submit(*dagger, "melee", 98) && number_after(attack_lines(*dagger)[0], " + ") == 6 &&
          offered(*dagger, "martial_arts"),
          "A Monk weapon also uses Dexterity, and the Bonus Unarmed Strike stays");

    auto armored = battle(*module, monk(), {"leather"});
    check(!offered(*armored, "martial_arts"), "Armor ends Martial Arts");
}

} // namespace

int main()
{
    try
    {
        martial_arts_checks();
        std::cout << "Monk tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
