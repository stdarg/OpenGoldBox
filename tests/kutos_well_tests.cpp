// Kuto's Well (docs/audits/kutos-well.md): its creature conversions, the
// rules they need and Norris the Gray's portrait.
#include "opengold/npc_portraits.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
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

// The shipped creatures plus a sturdy AC 1 target that never falls.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

bool submit(CombatSession &c, std::string_view verb, EntityId actor, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.actor == actor && (!target || command.target == target))
            return c.submit(command);
    return false;
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Each new conversion fights from combat.rules.
void conversions()
{
    auto module = rules();
    for (const auto *creature : {"lizardfolk", "giant-lizard", "gnoll-warrior"})
    {
        auto c = module->create({{8, 4, std::vector<std::uint8_t>(32)},
            {{1, creature, "Monster", 1, {1, 1}}, {2, "target", "Target", 0, {2, 1}}}},
        3);
        check(c->snapshot().combatants.size() == 2, std::string(creature) + " joins a fight");
    }
}

// Multiattack: the Lizardfolk's melee Attack action makes two attacks.
void multiattack()
{
    auto module = rules();
    auto c = module->create({{8, 4, std::vector<std::uint8_t>(32)},
        {{1, "lizardfolk", "Lizardfolk", 1, {1, 1}}, {2, "target", "Target", 0, {2, 1}}}},
    3);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end", c->snapshot().actor), "Reach the Lizardfolk's turn");
    check(submit(*c, "melee", 1, 2) && count_logged(*c, "Lizardfolk -> Target") == 2,
          "The Lizardfolk attacks twice with one Attack action");
    bool rejected = false;
    try
    {
        (void)srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                                  "\nmultiattack lizardfolk 3\n");
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "A creature has one Multiattack row");
}

// Norris the Gray's portrait fills the view once he has walked up and speaks.
void norris_portrait()
{
    por::TourSnapshot state;
    state.tour_finished = true;
    state.script_id = 29;
    state.sprite_id = 16;
    state.sprite_frame = 2;
    state.dialogue = "YOU ARE SURROUNDED BY THE BANDIT BAND OF THE INFAMOUS NORRIS THE  GRAY.";
    check(speaking_npc_portrait(state).empty(), "Norris's portrait waits until he is near");
    state.sprite_frame = 0;
    check(speaking_npc_portrait(state) == "NPCs/norris-the-gray.png",
          "Norris's portrait shows while he speaks");
    state.dialogue.clear();
    check(speaking_npc_portrait(state).empty(), "Norris's portrait needs his words");
    state.dialogue = "A Slums encounter";
    state.script_id = 20;
    check(speaking_npc_portrait(state).empty(), "Sprite 16 elsewhere is not Norris");
    state.tour_finished = false;
    check(speaking_npc_portrait(state) == "NPCs/rolf.png", "Rolf speaks during his tour");
    check(std::filesystem::exists(root / "art/portraits/NPCs/norris-the-gray.png"),
          "Norris's portrait is in the art folder");
}
} // namespace

int main()
{
    try
    {
        conversions();
        multiattack();
        norris_portrait();
        std::cout << "Kuto's Well tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
