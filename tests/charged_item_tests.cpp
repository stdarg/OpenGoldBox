#include "combat_fixture.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
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
              converts(original(70, 6, 87), "necklace_of_missiles", 6) &&
              converts(original(21, 1, 83), "javelin:lightning", 1) &&
              converts(original(70, 1, 90), "manual_of_bodily_health", 1),
          "The other charged items convert");
    check(equipment_conversion(original(62, 3, 3)).starts_with("por:unsupported:"),
          "A scroll of Cure Light Wounds is not a potion");
    auto worn = original(36, 0, 82);
    worn.stored.effect_codes = {0, 82, 0x84};
    check(equipment_conversion(worn) != "potion_of_giant_strength",
          "An effect applied when readied is not a use");

    check(srd5::charge_noun("wand:magic_missiles") == "charges" &&
              srd5::charge_noun("necklace_of_missiles") == "missiles" &&
              srd5::charge_noun("potion_of_healing").empty(),
          "A wand counts charges and the necklace missiles; potions count themselves");
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
                                            std::vector<std::string> equipped, int hp,
                                            Cell foe = {6, 1})
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
                           {p, {2, "vanguard", "Foe", Side::opposition, foe}}},
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
// The bonus of the fighter's first logged attack roll.
std::optional<int> attack_bonus(const CombatSession &c)
{
    for (const auto &line : c.snapshot().log())
    {
        if (line.starts_with("Fighter -> Foe: d20 "))
        {
            return std::stoi(line.substr(line.find(" + ") + 3));
        }
    }
    return std::nullopt;
}

// Potions of Speed and Giant Strength, Dust of Disappearance and the Wand of
// Paralysis.
void other_effects()
{
    const auto rules = module();
    auto speed = fighter_turn(*rules, {{"potion_of_speed", 1}}, {}, 12);
    check(submit(*speed, "potion_of_speed_item", 1) && logged(*speed, "gains Haste") &&
              uses_left(*speed, 7) == 0,
          "A Potion of Speed hastes its drinker");

    auto plain = fighter_turn(*rules, {}, {"longsword"}, 12, {2, 1});
    check(submit(*plain, "melee", 2), "Attack without the potion");
    auto strong = fighter_turn(*rules, {{"potion_of_giant_strength", 1}}, {"longsword"}, 12, {2, 1});
    check(submit(*strong, "potion_of_giant_strength_item", 1) &&
              logged(*strong, "Strength becomes 21") && submit(*strong, "melee", 2),
          "Drink a Potion of Giant Strength, then attack");
    check(attack_bonus(*plain) && attack_bonus(*strong) && *attack_bonus(*strong) > *attack_bonus(*plain),
          "Strength 21 raises the attack roll");
    check(rules->restore(strong->save())->save() == strong->save(),
          "A checkpoint keeps the giant's Strength");

    auto dust = fighter_turn(*rules, {{"dust_of_disappearance", 1}}, {}, 12);
    check(submit(*dust, "dust_of_disappearance_item", 1) && logged(*dust, "turns Invisible") &&
              uses_left(*dust, 7) == 0 && !offered(*dust, "dash"),
          "Dust of Disappearance turns its user Invisible with the Utilize action");

    auto wand = fighter_turn(*rules, {{"wand:paralysis", 23}}, {"wand:paralysis"}, 12);
    check(submit(*wand, "wand_of_paralysis_item", 2) && logged(*wand, "uses the Wand of Paralysis") &&
              logged(*wand, "DC 15") && uses_left(*wand, 7) == 22,
          "The Wand of Paralysis calls for a DC 15 Constitution save");
}

// The Javelin of Lightning's bolt strikes each creature in its line, then the
// javelin's throw; once until a Long Rest.
void javelin()
{
    const auto rules = module();
    const auto hero = fighter();
    Participant p{1, "campaign-character", "Fighter", Side::party, {1, 1},
                  rules->character_profile(hero.sheet(), {}).data};
    p.inventory.push_back({7, "javelin:lightning", 1});
    auto c = rules->create({{12, 6, std::vector<Terrain>(72)},
                            {p,
                             {2, "vanguard", "Foe", Side::opposition, {6, 1}},
                             {3, "vanguard", "Between", Side::opposition, {3, 1}}}},
                           5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
    {
        check(submit(*c, "end"), "End the foes' turns");
    }
    check(submit(*c, "javelin_lightning", 2) &&
              logged(*c, "javelin becomes a bolt of lightning") &&
              (logged(*c, "Between takes") || logged(*c, "Between: Dexterity")),
          "The bolt strikes the creature in its line");
    check(logged(*c, "Fighter -> Foe"), "Then the javelin is thrown at its target");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the spent bolt");
}

// A Thief of the given level; Fast Hands from level 3, Use Magic Device from 13.
Character thief(int level = 3)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "rogue";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Thief";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.training = {{"class:rogue", {"acrobatics", "stealth", "perception", "investigation"}},
                  {"class:rogue:expertise", {"investigation", "perception"}}};
    Character hero(*srd5::character_rules(), d, {});
    const auto rules = module();
    VitalState life{hero.sheet().hit_points, false, {}};
    while (hero.sheet().level < level)
    {
        check(hero.advance(*rules, life), "The Rogue advances");
    }
    return hero;
}

// Fast Hands takes the Magic action to use a wand as a Bonus Action.
void fast_hands()
{
    const auto rules = module();
    const auto hero = thief();
    Participant p{1, "campaign-character", "Thief", Side::party, {1, 1},
                  rules->character_profile(hero.sheet(), std::vector<std::string>{"wand:magic_missiles"}).data};
    p.inventory.push_back({7, "wand:magic_missiles", 5});
    auto c = rules->create({{12, 6, std::vector<Terrain>(72)},
                            {p, {2, "vanguard", "Foe", Side::opposition, {6, 1}}}},
                           5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "End the foe's turn");
    }
    check(offered(*c, "magic_missile_item") && offered(*c, "magic_missile_item_bonus"),
          "The wand is offered with the Action and, by Fast Hands, the Bonus Action");
    for (unsigned dart = 0; dart < 3; ++dart)
    {
        check(submit(*c, "magic_missile_item_bonus", 2), "Aim a dart");
    }
    check(uses_left(*c, 7) == 4 && offered(*c, "magic_missile_item") &&
              !offered(*c, "magic_missile_item_bonus"),
          "The Bonus Action spent a charge and left the Action");

    auto fighter_fight = fighter_turn(*rules, {{"wand:magic_missiles", 5}}, {"wand:magic_missiles"}, 12);
    check(!offered(*fighter_fight, "magic_missile_item_bonus"),
          "Without Fast Hands a wand takes the Action");
}

// Use Magic Device (Thief 13): a 6 on 1d6 keeps a wand's spent charge.
void use_magic_device()
{
    const auto rules = module();
    const auto profile =
        rules->character_profile(thief(13).sheet(), std::vector<std::string>{"wand:paralysis"}).data;
    bool kept = false, spent = false;
    for (std::uint64_t seed = 1; seed < 80 && !(kept && spent); ++seed)
    {
        Participant p{1, "campaign-character", "Thief", Side::party, {1, 1}, profile};
        p.inventory.push_back({7, "wand:paralysis", 23});
        auto c = rules->create({{12, 6, std::vector<Terrain>(72)},
                                {p, {2, "vanguard", "Foe", Side::opposition, {6, 1}}}},
                               seed);
        test::keep_initiative(*c);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        {
            check(submit(*c, "end"), "End the foe's turn");
        }
        check(submit(*c, "wand_of_paralysis_item", 2), "Use the wand");
        const bool saved_charge = logged(*c, "Use Magic Device: the charge is not spent.");
        check(uses_left(*c, 7) == (saved_charge ? 23u : 22u), "A charge is spent unless kept");
        kept = kept || saved_charge;
        spent = spent || !saved_charge;
    }
    check(kept && spent, "Use Magic Device sometimes keeps the charge");
}

// A Quick character uses its items, even with Quick magic off (MAGIC-3).
void quick_items()
{
    const auto rules = module();
    // A Fighter's Second Wind, which costs nothing, comes first; a Thief has none.
    const auto rogue = thief();
    Participant hurt_thief{1,       "campaign-character",
                           "Thief", Side::party,
                           {1, 1},  rules->character_profile(rogue.sheet(), {}).data};
    hurt_thief.state = VitalState{3, false, {}};
    hurt_thief.inventory.push_back({7, "potion_of_healing", 1});
    auto hurt = rules->create({{12, 6, std::vector<Terrain>(72)},
                               {hurt_thief, {2, "vanguard", "Foe", Side::opposition, {6, 1}}}},
                              5);
    test::keep_initiative(*hurt);
    for (unsigned turns = 0; hurt->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*hurt, "end"), "End the foe's turn");
    }
    check(choose_quick_command(*hurt, false).verb == "potion_of_healing_item",
          "A badly hurt Quick character drinks a Potion of Healing");

    auto wand = fighter_turn(*rules, {{"wand:magic_missiles", 5}}, {"wand:magic_missiles"}, 12);
    check(choose_quick_command(*wand, false).verb == "magic_missile_item",
          "A Quick character uses a held wand with Quick magic off");

    // The necklace's Fireball, aimed only where no ally can be caught.
    auto necklace = fighter_turn(*rules, {{"necklace_of_missiles", 6}}, {}, 12, {9, 1});
    bool exploded = false;
    for (unsigned step = 0; step < 6 && !exploded; ++step)
    {
        check(necklace->submit(choose_quick_command(*necklace, false)), "Quick acts");
        exploded = logged(*necklace, "uses the Necklace of Missiles");
    }
    check(exploded, "A Quick character throws a missile at a distant foe");

    const auto hero = fighter();
    Participant p{1,         "campaign-character",
                  "Fighter", Side::party,
                  {1, 1},    rules->character_profile(hero.sheet(), {}).data};
    p.inventory.push_back({7, "necklace_of_missiles", 6});
    auto close = rules->create({{12, 6, std::vector<Terrain>(72)},
                                {p,
                                 {2, "vanguard", "Foe", Side::opposition, {3, 1}},
                                 {3, "healer", "Ally", Side::party, {3, 2}}}},
                               5);
    test::keep_initiative(*close);
    for (unsigned turns = 0; close->snapshot().actor != 1 && turns < 6; ++turns)
    {
        check(submit(*close, "end"), "Reach the fighter");
    }
    check(choose_quick_command(*close, false).verb != "fireball_item",
          "No Fireball where it would catch an ally");
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
    auto necklace = fighter_turn(*rules, {{"necklace_of_missiles", 6}}, {}, 12);
    check(aim_and_cast(*necklace, "fireball_item", {6, 1}) &&
              logged(*necklace, "uses the Necklace of Missiles") &&
              uses_left(*necklace, 7) == 5,
          "A missile of the necklace explodes as a Fireball");
    check(logged(*necklace, "DC 15"), "Its save is DC 15");

    auto wand = fighter_turn(*rules, {{"wand:lightning_bolts", 3}}, {"wand:lightning_bolts"}, 12);
    check(aim_and_cast(*wand, "lightning_bolt_item", {6, 1}) &&
              logged(*wand, "uses the Wand of Lightning Bolts") && uses_left(*wand, 7) == 2,
          "The wand casts Lightning Bolt and spends a charge");
}
// The Manual of Bodily Health raises Constitution by 2, and Hit Points as
// though the new modifier had counted from level 1.
void manual()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.award_experience(900, "test:xp");
    for (unsigned n = 1; n < 3; ++n)
    {
        party.advance(id, party.default_advancement(id));
    }
    check(party.award_loot({}, {original(70, 1, 90)}, "test:manual"), "Collect the Manual");
    const auto book = party.member(id).character.inventory().items().back().id;
    const auto before = party.member(id).character.sheet();
    const int hp = party.member(id).vitals.hit_points;
    party.read_manual(id, book);
    const auto &after = party.member(id).character.sheet();
    check(after.scores[Ability::constitution] == before.scores[Ability::constitution] + 2 &&
              after.saving_throws[Ability::constitution] ==
                  before.saving_throws[Ability::constitution] + 1,
          "Constitution rises by 2");
    check(after.hit_points == before.hit_points + 3 &&
              party.member(id).vitals.hit_points == hp + 3,
          "A level-3 character gains 3 Hit Points, current and maximum");
    check(!party.member(id).character.inventory().find(book), "The Manual's magic is spent");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "manual"),
                                     *srd5::character_rules(), *rules, "manual", nullptr)
                         .party);
    check(restored.member(id).character.sheet().hit_points == after.hit_points &&
              restored.member(id).character.sheet().scores == after.scores,
          "A save rebuilds the Manual's Constitution");
    party.award_experience(1800, "test:xp2");
    party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().level == 4, "The reader still advances");
}

// Outside combat a healing potion restores Hit Points to a wounded drinker; a
// Potion of Speed waits for a fight.
void drinking_outside_combat()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    check(party.award_loot({}, {original(70, 3, 3), original(71, 1, 80)}, "test:drinks"),
          "Collect potions");
    const auto items = party.member(id).character.inventory().items();
    const auto greater = items[items.size() - 2].id, speed = items.back().id;
    test::rejects(
        [&]
        {
            party.drink_potion(id, greater);
        },
        "Unhurt, there is nothing to heal");
    auto wounded = party.checkpoint();
    wounded.roster.front().vitals.hit_points = 2;
    party.restore(std::move(wounded));
    party.drink_potion(id, greater);
    check(party.member(id).vitals.hit_points >= 10 &&
              party.member(id).character.inventory().find(greater)->get().quantity == 2,
          "A Potion of Greater Healing restores 4d4 + 4 and two are left");
    test::rejects(
        [&]
        {
            party.drink_potion(id, speed);
        },
        "A Potion of Speed works only in combat");
    check(srd5::usable_outside_combat("potion_of_healing") &&
              srd5::usable_outside_combat("manual_of_bodily_health") &&
              !srd5::usable_outside_combat("potion_of_speed"),
          "The inventory's Use offers what works outside combat");
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

    // Giant Strength lasts its hour past the fight.
    check(party.award_loot({}, {original(84, 1, 82)}, "test:strength"), "Collect the potion");
    auto again = party.participants();
    again.front().cell = {1, 1};
    again.push_back({2, "vanguard", "Foe", Side::opposition, {6, 1}});
    party.begin_combat();
    auto fight = rules->create({{12, 6, std::vector<Terrain>(72)},
                                again,
                                party.state().next_combat_scope,
                                100,
                                coins(party.state().wealth, Coin::gold)},
                               5);
    test::keep_initiative(*fight);
    for (unsigned turns = 0; fight->snapshot().actor != id && turns < 4; ++turns)
    {
        check(submit(*fight, "end"), "Reach the fighter's turn");
    }
    check(submit(*fight, "potion_of_giant_strength_item", id), "Drink Giant Strength");
    party.apply_combat(fight->snapshot());
    party.end_combat();
    const auto strong = party.profile(id).melee_attack_bonus;
    party.advance_time(std::chrono::minutes{61});
    check(party.profile(id).melee_attack_bonus <= strong, "An hour later it has worn off");
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
        other_effects();
        javelin();
        fast_hands();
        quick_items();
        use_magic_device();
        manual();
        drinking_outside_combat();
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
