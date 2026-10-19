#include "combat_fixture.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// The original's wands, potions and other charged items (#263, MAGIC-3).
namespace
{
using opengold::test::check;

const auto source = std::filesystem::path(OPENGOLD_SOURCE_DIR);

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Read the rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

std::unique_ptr<RulesModule> module()
{
    return srd5::parse_content(read(source / "data/rules/srd-5.2.1/combat.rules"));
}

Character fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Fighter";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

// An original record: its type, charges (byte 60), spell or power (61) and
// stack size.
por::Equipment original(std::uint8_t type, std::uint8_t charges, std::uint8_t power,
                        std::uint8_t stack = 1)
{
    por::Equipment item;
    item.stored.type = type;
    item.stored.magic_bonus = 1;
    item.stored.effect_codes = {charges, power, 0};
    item.stored.stack_size = stack;
    return item;
}

void conversion()
{
    const auto converts = [](const por::Equipment &item, std::string_view key, unsigned quantity)
    {
        return equipment_conversion(item) == key && carried_quantity(item) == quantity;
    };
    check(converts(original(79, 23, 88), "wand:magic_missiles", 23) &&
              converts(original(78, 3, 51), "wand:lightning_bolts", 3) &&
              converts(original(78, 67, 51), "wand:lightning_bolts", 67) &&
              converts(original(78, 23, 84), "wand:paralysis", 23),
          "Wands convert with their charges");
    check(converts(original(71, 1, 85), "potion_of_healing", 1) &&
              converts(original(70, 1, 85, 2), "potion_of_healing", 2) &&
              converts(original(70, 3, 3), "potion_of_greater_healing", 3) &&
              converts(original(84, 1, 82), "potion_of_giant_strength", 1) &&
              converts(original(71, 1, 80), "potion_of_speed", 1),
          "Potions convert, Extra Healing's three doses as three Potions of Greater Healing");
    check(converts(original(70, 1, 86), "dust_of_disappearance", 1) &&
              converts(original(70, 6, 87), "necklace_of_fireballs", 6) &&
              converts(original(21, 1, 83), "javelin:lightning", 1) &&
              converts(original(70, 1, 90), "manual_of_bodily_health", 1),
          "The other charged items convert");
    check(equipment_conversion(original(62, 3, 3)).starts_with("por:unsupported:"),
          "A scroll of Cure Light Wounds is not a potion");
    auto worn = original(36, 0, 82);
    worn.stored.effect_codes = {0, 82, 0x84};
    check(equipment_conversion(worn) != "potion_of_giant_strength",
          "An effect applied when readied is not a use");

    auto necklace = original(70, 6, 87);
    necklace.decoded_name = "Necklace of Missiles";
    check(necklace.label() == "Necklace of Fireballs",
          "The Necklace of Missiles is named the Necklace of Fireballs");
    check(srd5::charge_noun("wand:magic_missiles") == "charges" &&
              srd5::charge_noun("necklace_of_fireballs") == "beads" &&
              srd5::charge_noun("potion_of_healing").empty(),
          "A wand counts charges and the necklace beads; potions count themselves");
}

// Loot keeps its charges in the party and in a save.
void campaign()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    check(party.award_loot({}, {original(79, 23, 88), original(70, 3, 3)}, "test:charged"),
          "Collect a wand and Potion Extra Healing");
    const auto items = party.member(id).character.inventory().items();
    check(items[items.size() - 2].definition_id == "wand:magic_missiles" &&
              items[items.size() - 2].quantity == 23 &&
              items.back().definition_id == "potion_of_greater_healing" &&
              items.back().quantity == 3,
          "The wand has 23 charges and there are three potions");
    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "charged"),
                                     *srd5::character_rules(), *rules, "charged", nullptr)
                         .party);
    const auto kept = restored.member(id).character.inventory().items();
    check(std::equal(kept.begin(), kept.end(), items.begin(), items.end()),
          "A save keeps the charges");
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

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb;
                       });
}

unsigned uses_left(const CombatSession &c, std::uint64_t item)
{
    for (const auto &combatant : c.snapshot().combatants)
    {
        for (const auto &[id, left] : combatant.item_uses_left)
        {
            if (combatant.id == 1 && id == item)
            {
                return left;
            }
        }
    }
    throw std::runtime_error("The item is not reported");
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

// A fight at the fighter's first turn: the fighter carries `carried` (each
// with inventory ID 7, 8, ...) and holds `equipped`, with `hp` Hit Points.
std::unique_ptr<CombatSession> fighter_turn(const RulesModule &rules,
                                            std::vector<std::pair<std::string, unsigned>> carried,
                                            std::vector<std::string> equipped, int hp)
{
    const auto hero = fighter();
    Participant p{1, "campaign-character", "Fighter", Side::party, {1, 1},
                  rules.character_profile(hero.sheet(), equipped).data};
    p.state = VitalState{hp, false, {}};
    std::uint64_t id = 7;
    for (const auto &[definition, quantity] : carried)
    {
        p.inventory.push_back({id++, definition, quantity});
    }
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
                           {p, {2, "vanguard", "Foe", Side::opposition, {6, 1}}}},
                          5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "End the foe's turn");
    }
    check(c->snapshot().actor == 1, "The fighter acts");
    return c;
}

int hit_points(const CombatSession &c, EntityId id)
{
    for (const auto &combatant : c.snapshot().combatants)
    {
        if (combatant.id == id)
        {
            return combatant.hit_points;
        }
    }
    throw std::runtime_error("No such combatant");
}

// A potion is drunk as a Bonus Action and spends one of the stack.
void potions()
{
    const auto rules = module();
    auto full = fighter_turn(*rules, {{"potion_of_healing", 2}}, {}, 12);
    check(!offered(*full, "potion_of_healing_item"), "Unhurt, there is nothing to drink for");

    auto c = fighter_turn(*rules, {{"potion_of_healing", 2}}, {}, 2);
    check(submit(*c, "potion_of_healing_item", 1), "Drink a Potion of Healing");
    const int healed = hit_points(*c, 1) - 2;
    check(healed >= 4 && healed <= 10 && logged(*c, "uses the Potion of Healing"),
          "It restores 2d4 + 2 Hit Points");
    check(uses_left(*c, 7) == 1, "One potion is left");
    check(!offered(*c, "potion_of_healing_item") && offered(*c, "dash"),
          "It took the Bonus Action, not the Action");
    check(rules->restore(c->save())->save() == c->save(), "A checkpoint keeps the potions left");

    auto greater = fighter_turn(*rules, {{"potion_of_greater_healing", 3}}, {}, 1);
    check(submit(*greater, "potion_of_greater_healing_item", 1), "Drink a Potion of Greater Healing");
    const int more = hit_points(*greater, 1) - 1;
    check(more >= 8 && uses_left(*greater, 7) == 2, "It restores 4d4 + 4 Hit Points");
}

// A wand is used while held; each use spends a charge, with the wand's DC 15.
void wands()
{
    const auto rules = module();
    auto stowed = fighter_turn(*rules, {{"wand:magic_missiles", 3}}, {}, 12);
    check(!offered(*stowed, "magic_missile_item"), "A wand in the pack does nothing");

    auto c = fighter_turn(*rules, {{"wand:magic_missiles", 3}}, {"wand:magic_missiles"}, 12);
    // Each of the three darts is aimed in turn.
    for (unsigned dart = 0; dart < 3; ++dart)
    {
        check(submit(*c, "magic_missile_item", 2), "Aim a dart");
    }
    check(logged(*c, "uses the Wand of Magic Missiles"), "Cast Magic Missile from the wand");
    check(hit_points(*c, 2) < 28 && uses_left(*c, 7) == 2, "The darts strike and spend a charge");
    check(!offered(*c, "magic_missile_item"), "It took the Action");
}
// Aims an area item's effect at `center` and sets it off.
bool aim_and_cast(CombatSession &c, std::string_view verb, Cell center)
{
    if (!submit(c, verb))
    {
        return false;
    }
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == "area_move" && command.destination == center)
        {
            (void)c.submit(command);
            break;
        }
    }
    return submit(c, "area_cast");
}

// A bead or a lightning wand's bolt fills an area, with the item's DC 15.
void areas()
{
    const auto rules = module();
    auto necklace = fighter_turn(*rules, {{"necklace_of_fireballs", 6}}, {}, 12);
    check(aim_and_cast(*necklace, "fireball_item", {6, 1}) &&
              logged(*necklace, "uses the Necklace of Fireballs") &&
              uses_left(*necklace, 7) == 5,
          "A bead of the necklace explodes as a Fireball");
    check(logged(*necklace, "DC 15"), "Its save is DC 15");

    auto wand = fighter_turn(*rules, {{"wand:lightning_bolts", 3}}, {"wand:lightning_bolts"}, 12);
    check(aim_and_cast(*wand, "lightning_bolt_item", {6, 1}) &&
              logged(*wand, "uses the Wand of Lightning Bolts") && uses_left(*wand, 7) == 2,
          "The wand casts Lightning Bolt and spends a charge");
}
// After a fight the party carries what is left; the last potion is gone.
void after_combat()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    check(party.award_loot({}, {original(71, 1, 85)}, "test:potion"), "Collect a potion");
    const auto potion = party.member(id).character.inventory().items().back().id;
    auto participants = party.participants();
    participants.front().cell = {1, 1};
    participants.front().state = VitalState{2, false, {}};
    participants.push_back({2, "vanguard", "Foe", Side::opposition, {6, 1}});
    party.begin_combat();
    auto c = rules->create({{12, 6, std::vector<Terrain>(72)},
                            participants,
                            party.state().next_combat_scope,
                            100,
                            coins(party.state().wealth, Coin::gold)},
                           5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != id && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "Reach the fighter's turn");
    }
    check(submit(*c, "potion_of_healing_item", id), "Drink the potion");
    party.apply_combat(c->snapshot());
    party.end_combat();
    check(!party.member(id).character.inventory().find(potion) &&
              !party.member(id).item_sources.contains(potion),
          "The empty potion is gone");
}
} // namespace

int main()
{
    try
    {
        conversion();
        campaign();
        potions();
        wands();
        areas();
        after_combat();
        std::cout << "Charged item tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
