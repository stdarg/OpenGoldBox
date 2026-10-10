// Kuto's Well (docs/audits/kutos-well.md): its creature conversions, the
// rules they need, its arrow traps and Norris the Gray's portrait.
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
    const auto log = c.snapshot().log();
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// Each new conversion fights from combat.rules.
void conversions()
{
    auto module = rules();
    for (const auto *creature : {"lizardfolk", "giant-lizard", "gnoll-warrior", "norris-the-gray"})
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

// A created level-1 Fighter.
CharacterSheet fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "acolyte";
    d.alignment = "neutral_good";
    d.name = "Arrow target";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {}).sheet();
}

// The DAMAGE arrows: an attack roll against AC, doubled dice on a 20, and death
// saves rolled at once for a member an arrow drops.
void arrow_traps()
{
    const auto module = rules();
    const auto sheet = fighter();
    const HazardAttack arrow{3, 1, 6, 0, "piercing"};
    const int armor_class = module->character_profile(sheet, {}).armor_class;
    bool missed = false, hit = false, critical = false;
    for (std::uint64_t seed = 1; seed < 400; ++seed)
    {
        VitalState state{sheet.hit_points, false, {}};
        RandomState random{seed};
        const auto r = module->hazard_attack(state, sheet, arrow, random);
        check(r.total == r.natural + 3 && r.armor_class == armor_class,
              "An arrow rolls d20 + 3 against the member's AC");
        check(r.hit == (r.natural == 20 || (r.natural != 1 && r.total >= r.armor_class)) &&
              r.critical == (r.natural == 20), "An arrow hits by the SRD attack roll rules");
        check(r.damage >= (r.hit ? (r.critical ? 2 : 1) : 0) &&
              r.damage <= (r.hit ? (r.critical ? 12 : 6) : 0) &&
              state.hit_points == sheet.hit_points - r.damage,
              "A hit deals 1d6, or 2d6 on a 20");
        missed |= !r.hit;
        hit |= r.hit;
        critical |= r.critical;
    }
    check(missed && hit && critical, "Arrows miss, hit and critically hit");

    VitalState frail{1, false, {}};
    RandomState random{1};
    auto dropped = module->hazard_attack(frail, sheet, {100, 1, 1, 0, "piercing"}, random);
    while (!dropped.hit)
    {
        frail = {1, false, {}};
        dropped = module->hazard_attack(frail, sheet, {100, 1, 1, 0, "piercing"}, random);
    }
    check(!dropped.death_saves.empty() &&
          (frail.dead || frail.hit_points == 1 || dropped.death_saves.back() >= 10),
          "A dropped member's death saves are rolled until stable, revived or dead");
    bool rejected = false;
    try
    {
        VitalState down{0, false, frail.resources};
        (void)module->hazard_attack(down, sheet, arrow, random);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "Arrows only target conscious members");
    // The arrows' dice come from the original script, so bad data must be
    // refused rather than divide by zero inside the dice roller.
    rejected = false;
    try
    {
        for (std::uint64_t seed = 1; seed < 40; ++seed)
        {
            VitalState target{sheet.hit_points, false, {}};
            random = RandomState{seed};
            (void)module->hazard_attack(target, sheet, {100, 1, 0, 0, "piercing"}, random);
        }
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    check(rejected, "An arrow whose die has no sides is refused");
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
        arrow_traps();
        std::cout << "Kuto's Well tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
