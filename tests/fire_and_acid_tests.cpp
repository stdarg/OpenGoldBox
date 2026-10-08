// SRD 5.2.1 Fire and Acid gear for every party (docs/audits/cluebook-check.md):
// the Torch is a Simple Melee weapon dealing 1 Fire damage, the original's oil
// and the added Acid and Alchemist's Fire are carried gear, and a Torch blow
// keeps a downed troll from regenerating.
#include "opengold/authored_items.h"
#include "opengold/campaign_party.h"
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

por::Equipment item(std::uint8_t type)
{
    por::Equipment equipment;
    equipment.stored.type = type;
    return equipment;
}

// The original oil and the added gear convert to their SRD items; the Torch is
// a one-handed weapon and the flasks are carried gear.
void conversions()
{
    const auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    check(equipment_conversion(item(original_item::flask_of_oil)) == "oil" &&
          equipment_conversion(item(authored_item::torch)) == "torch" &&
          equipment_conversion(item(authored_item::acid)) == "acid" &&
          equipment_conversion(item(authored_item::alchemists_fire)) == "alchemists_fire",
          "Oil, Torch, Acid and Alchemist's Fire convert to SRD gear");
    const auto torch = module->equipment_info("torch");
    check(torch.slot == EquipmentSlot::weapon && torch.hands == 1, "A Torch is wielded in one hand");
    for (const auto *gear : {"oil", "acid", "alchemists_fire"})
        check(module->equipment_info(gear).slot == EquipmentSlot::carried,
              std::string(gear) + " is carried gear");
    check(item(authored_item::torch).label() == "Torch" &&
          item(authored_item::alchemists_fire).label() == "Alchemist's Fire",
          "The added gear has readable names");
}

CharacterSheet fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "acolyte";
    d.alignment = "neutral_good";
    d.name = "Torchbearer";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {}).sheet();
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log;
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId actor, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && command.actor == actor && (!target || command.target == target))
            return c.submit(command);
    return false;
}

// A Torch hit is Fire damage: a regenerating creature it fells dies on its turn
// while its side fights on.
void torch_stops_regeneration()
{
    const auto module = srd5::parse_content(
                            read(root / "data/rules/srd-5.2.1/combat.rules") +
                            "\ncreature weak-troll 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                            "regeneration weak-troll 15\n"
                            "creature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    const std::array<std::string, 1> torch{"torch"};
    const auto profile = module->character_profile(fighter(), torch);
    bool burned = false;
    for (std::uint64_t seed = 1; seed < 20 && !burned; ++seed)
    {
        auto c = module->create({{10, 4, std::vector<std::uint8_t>(40)},
            {   {1, "campaign-character", "Torchbearer", 0, {2, 1}, profile.data},
                {2, "weak-troll", "Troll", 1, {1, 1}}, {3, "target", "Guard", 1, {9, 3}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            (void)submit(*c, "end", c->snapshot().actor);
        if (c->snapshot().actor != 1 || !submit(*c, "melee", 1, 2))
            continue;
        const auto troll = [&]
        {
            const auto s = c->snapshot();
            return *std::find_if(s.combatants.begin(), s.combatants.end(),
                                 [](const auto & u)
            {
                return u.id == 2;
            });
        };
        if (troll().hit_points != 0)
            continue;
        check(c->snapshot().outcome == Outcome::ongoing,
              "A Torch fells the troll while its guard fights on");
        for (unsigned turns = 0; turns < 3 && !troll().dead; ++turns)
            (void)submit(*c, "end", c->snapshot().actor);
        check(troll().dead && count_logged(*c, "cannot regenerate and dies") == 1,
              "A troll felled by a Torch cannot regenerate and dies");
        burned = true;
    }
    check(burned, "The Torch hits the troll");
}
} // namespace

int main()
{
    try
    {
        conversions();
        torch_stops_regeneration();
        std::cout << "Fire and acid tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
