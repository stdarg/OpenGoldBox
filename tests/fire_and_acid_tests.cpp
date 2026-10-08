// SRD 5.2.1 Fire and Acid gear for every party (GEAR-1 in docs/SRD-DECISIONS.md):
// the Torch is a Simple Melee weapon dealing 1 Fire damage that any carrier can
// draw to strike; the original's Oil and the added Acid and Alchemist's Fire are
// thrown and used up; and Fire keeps a downed troll from regenerating.
#include "opengold/authored_items.h"
#include "opengold/campaign_party.h"
#include "opengold/combat_demo.h"
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
// Shipped rules plus a weak troll, a sturdy guard and a target that always
// fails Dexterity saves.
std::string arena_rules()
{
    return read(root / "data/rules/srd-5.2.1/combat.rules") +
           "\ncreature weak-troll 1 1 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
           "regeneration weak-troll 15\n"
           "creature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
           "creature clumsy 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
           "saves clumsy 0 -10 0 0 0 0\n";
}

Character torchbearer()
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
    return Character(*srd5::character_rules(), d, {});
}

// A fighter wielding a longsword and carrying `gear`, against `enemies` placed
// at `cells` (the fighter stands at (2,2)).
struct Arena
{
    std::shared_ptr<CampaignParty> party;
    MemberId fighter{};
    std::unique_ptr<CombatDemo> demo;
};

Arena arena(const std::vector<std::uint8_t> &gear,
            const std::vector<std::pair<std::string, rules::Cell>> &enemies, std::uint64_t seed,
            bool shield = false)
{
    Arena result;
    result.party = std::make_shared<CampaignParty>(srd5::parse_content(arena_rules()));
    result.fighter = result.party->add_pc(torchbearer());
    const auto wield = [&](std::uint8_t type)
    {
        result.party->purchase(result.fighter, item(type));
        result.party->equip(
            result.fighter,
            result.party->member(result.fighter).character.inventory().items().back().id);
    };
    wield(36);
    if (shield)
        wield(59);
    for (const auto type : gear)
        result.party->purchase(result.fighter, item(type));
    CampaignEncounter encounter;
    encounter.field.geometry = {12, 6, std::vector<std::uint8_t>(72)};
    encounter.field.tiles.resize(72, 7);
    encounter.positions.push_back({2, 2});
    EntityId id = 1000;
    for (const auto &[definition, cell] : enemies)
    {
        encounter.enemies.push_back({id++, definition, definition, 1, {}});
        encounter.positions.push_back(cell);
    }
    result.demo = std::make_unique<CombatDemo>(srd5::parse_content(arena_rules()));
    result.demo->campaign_party(result.party);
    result.demo->encounter(std::move(encounter), seed);
    return result;
}

bool offered(const CombatSession &c, std::string_view verb, EntityId actor, EntityId target)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb && command.actor == actor && command.target == target;
    });
}

bool use(CombatDemo &demo, std::string_view verb, EntityId actor, EntityId target = 0)
{
    for (const auto &command : demo.combat().legal_commands())
        if (command.verb == verb && command.actor == actor && (!target || command.target == target))
            return demo.submit(command);
    return false;
}

// Ends turns until `id` acts.
void turn_of(CombatDemo &demo, EntityId id)
{
    for (unsigned turns = 0; demo.combat().snapshot().actor != id; ++turns)
    {
        check(turns < 8, "Reach the wanted turn");
        check(use(demo, "end", demo.combat().snapshot().actor), "End the turn");
    }
}

CombatantView unit(const CombatDemo &demo, EntityId id)
{
    const auto s = demo.combat().snapshot();
    return *std::find_if(s.combatants.begin(), s.combatants.end(), [&](const auto & u)
    {
        return u.id == id;
    });
}

std::size_t logged(const CombatDemo &demo, std::string_view text)
{
    const auto log = demo.combat().snapshot().log;
    return std::count_if(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

// A sword-wielder draws a carried Torch to strike, and its Fire keeps a felled
// troll down.
void torch_drawn_against_troll()
{
    for (std::uint64_t seed = 1; seed < 30; ++seed)
    {
        auto a = arena({authored_item::torch}, {{"weak-troll", {3, 2}}, {"target", {10, 4}}}, seed);
        const auto fighter = EntityId(a.fighter);
        turn_of(*a.demo, fighter);
        check(offered(a.demo->combat(), "torch", fighter, 1000) &&
              offered(a.demo->combat(), "melee", fighter, 1000),
              "A sword-wielder carrying a Torch can strike with either");
        check(use(*a.demo, "torch", fighter, 1000), "The fighter strikes with the Torch");
        if (unit(*a.demo, 1000).hit_points != 0)
            continue;
        for (unsigned turns = 0; turns < 3 && !unit(*a.demo, 1000).dead; ++turns)
            (void)use(*a.demo, "end", a.demo->combat().snapshot().actor);
        check(unit(*a.demo, 1000).dead && logged(*a.demo, "cannot regenerate and dies") == 1,
              "A troll felled by a Torch cannot regenerate and dies");
        return;
    }
    check(false, "The Torch hits the troll");
}

// A vial of Acid is thrown within 20 feet, used up, and leaves the inventory.
void acid_used_up()
{
    auto a = arena({authored_item::acid}, {{"clumsy", {5, 2}}, {"target", {10, 4}}}, 3);
    const auto fighter = EntityId(a.fighter);
    turn_of(*a.demo, fighter);
    check(offered(a.demo->combat(), "throw_acid", fighter, 1000) &&
          !offered(a.demo->combat(), "throw_acid", fighter, 1001),
          "Acid can be thrown at a creature within 20 feet, not beyond");
    check(use(*a.demo, "throw_acid", fighter, 1000) && logged(*a.demo, "throws Acid") == 1 &&
          unit(*a.demo, 1000).hit_points < 1000,
          "A failed save against Acid takes Acid damage");
    check(!offered(a.demo->combat(), "throw_acid", fighter, 1000),
          "The only vial is used up");
    const auto &items = a.party->member(a.fighter).character.inventory().items();
    check(std::none_of(items.begin(), items.end(), [](const auto & i)
    {
        return i.definition_id == "acid";
    }),
    "The thrown vial leaves the campaign inventory");
}

// Alchemist's Fire sets a creature burning: it burns at the start of its turn
// and can roll on the ground to put the fire out.
void alchemists_fire_burns()
{
    auto a = arena({authored_item::alchemists_fire}, {{"clumsy", {5, 2}}}, 5);
    const auto fighter = EntityId(a.fighter);
    turn_of(*a.demo, fighter);
    check(use(*a.demo, "throw_alchemists_fire", fighter, 1000) &&
          logged(*a.demo, "starts burning") == 1,
          "Alchemist's Fire sets the creature burning");
    turn_of(*a.demo, 1000);
    check(logged(*a.demo, "burns for") == 1 &&
          offered(a.demo->combat(), "extinguish", 1000, 0),
          "A burning creature takes Fire damage on its turn and can put the fire out");
    check(use(*a.demo, "extinguish", 1000) && unit(*a.demo, 1000).prone,
          "Putting the fire out leaves the creature Prone");
}

// Oil makes the next Fire damage deal 5 more.
void oil_then_torch()
{
    for (std::uint64_t seed = 1; seed < 30; ++seed)
    {
        auto a = arena({original_item::flask_of_oil, authored_item::torch}, {{"clumsy", {3, 2}}},
                       seed);
        const auto fighter = EntityId(a.fighter);
        turn_of(*a.demo, fighter);
        check(use(*a.demo, "throw_oil", fighter, 1000) && logged(*a.demo, "covered in oil") == 1,
              "Oil covers a creature that fails its save");
        check(use(*a.demo, "end", fighter), "The fighter ends its turn");
        turn_of(*a.demo, fighter);
        const auto before = unit(*a.demo, 1000).hit_points;
        check(use(*a.demo, "torch", fighter, 1000), "The fighter strikes with the Torch");
        if (unit(*a.demo, 1000).hit_points == before)
            continue;
        check(before - unit(*a.demo, 1000).hit_points == 6 &&
              logged(*a.demo, "burns for 5 more damage") == 1,
              "Fire on an oiled creature deals 5 more damage");
        return;
    }
    check(false, "The Torch hits the oiled creature");
}
// A carried longbow and arrows: drawn to shoot an enemy out of melee reach.
// A shield-bearer first takes off its shield (an action), which costs its AC.
void bows_drawn()
{
    constexpr std::uint8_t longbow = 41, arrows = 73;
    auto free_hand = arena({longbow, arrows}, {{"target", {8, 2}}}, 3);
    const auto archer = EntityId(free_hand.fighter);
    turn_of(*free_hand.demo, archer);
    check(offered(free_hand.demo->combat(), "shoot", archer, 1000) &&
          !offered(free_hand.demo->combat(), "melee", archer, 1000),
          "A carried longbow is drawn to shoot an enemy out of reach");
    check(use(*free_hand.demo, "shoot", archer, 1000) && logged(*free_hand.demo, "Torchbearer ->"),
          "The arrow is shot");

    auto shielded = arena({longbow, arrows}, {{"target", {8, 2}}}, 3, true);
    const auto fighter = EntityId(shielded.fighter);
    turn_of(*shielded.demo, fighter);
    const auto armored = unit(*shielded.demo, fighter).armor_class;
    check(!offered(shielded.demo->combat(), "shoot", fighter, 1000) &&
          offered(shielded.demo->combat(), "doff_shield", fighter, 0),
          "A shield-bearer must take off its shield to draw a two-handed bow");
    check(use(*shielded.demo, "doff_shield", fighter) &&
          unit(*shielded.demo, fighter).armor_class == armored - 2 &&
          !offered(shielded.demo->combat(), "doff_shield", fighter, 0),
          "Taking off the shield costs its AC");
    const auto checkpoint = shielded.demo->combat().save();
    check(srd5::parse_content(arena_rules())->restore(checkpoint)->save() == checkpoint,
          "A removed shield survives a checkpoint");
    check(use(*shielded.demo, "end", fighter), "The fighter ends its turn");
    turn_of(*shielded.demo, fighter);
    check(offered(shielded.demo->combat(), "shoot", fighter, 1000) &&
          unit(*shielded.demo, fighter).armor_class == armored - 2,
          "Without its shield the fighter can shoot, and the shield stays off");
}
// A troll set Burning while down cannot regenerate and dies cleanly on its turn,
// and the combat AI throws Alchemist's Fire at a standing troll that is not
// burning yet.
void burning_troll()
{
    for (std::uint64_t seed = 1; seed < 30; ++seed)
    {
        auto a = arena({authored_item::alchemists_fire}, {{"weak-troll", {4, 2}},
            {"target", {10, 4}}
        }, seed);
        const auto fighter = EntityId(a.fighter);
        turn_of(*a.demo, fighter);
        check(choose_demo_command(a.demo->combat()).verb == "throw_alchemists_fire",
              "The AI sets a standing troll burning");
        check(use(*a.demo, "throw_alchemists_fire", fighter, 1000), "Throw Alchemist's Fire");
        if (!unit(*a.demo, 1000).burning)
            continue;
        check(unit(*a.demo, 1000).regenerates, "The snapshot shows a regenerating troll");
        for (unsigned turns = 0; turns < 4 && !unit(*a.demo, 1000).dead; ++turns)
            check(use(*a.demo, "end", a.demo->combat().snapshot().actor), "End the turn");
        check(unit(*a.demo, 1000).dead, "A burning troll at 0 HP cannot regenerate and dies");
        return;
    }
    check(false, "Alchemist's Fire sets the troll burning");
}
// Beside a standing troll, the combat AI strikes with its Torch: the Fire stops
// regeneration worth more than a sword's damage.
void torch_tactic()
{
    auto a = arena({authored_item::torch}, {{"troll", {3, 2}}}, 4);
    const auto fighter = EntityId(a.fighter);
    turn_of(*a.demo, fighter);
    const auto troll = unit(*a.demo, 1000);
    check(troll.regenerates && !troll.regeneration_stopped && troll.hit_points > 0,
          "The snapshot shows a standing troll that can regenerate");
    check(choose_demo_command(a.demo->combat()).verb == "torch",
          "The AI strikes a standing troll with its Torch");
}
} // namespace

int main()
{
    try
    {
        conversions();
        torch_stops_regeneration();
        torch_drawn_against_troll();
        acid_used_up();
        alchemists_fire_burns();
        oil_then_torch();
        bows_drawn();
        burning_troll();
        torch_tactic();
        std::cout << "Fire and acid tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
