#include "combat_fixture.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// The original's rings, bracers, cloak, gauntlets and necklace (#264, MAGIC-4).
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

Character fighter(std::string name = "Fighter")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

// An original record: its type, bonus (byte 50) and effect bytes 60-62.
por::Equipment original(std::uint8_t type, int bonus, std::array<std::uint8_t, 3> effects = {},
                        bool cursed = false)
{
    por::Equipment item;
    item.stored.type = type;
    item.stored.magic_bonus = static_cast<std::int8_t>(bonus);
    item.stored.effect_codes = effects;
    item.stored.cursed_raw = cursed ? 1 : 0;
    return item;
}

void conversion()
{
    check(equipment_conversion(original(93, 2)) == "ring_of_protection+2" &&
              equipment_conversion(original(69, 3, {0, 61, 0x81})) == "ring_of_fire_resistance" &&
              equipment_conversion(original(90, 3)) == "ring_of_feather_falling",
          "Rings convert");
    check(equipment_conversion(original(77, 4)) == "bracers+4" &&
              equipment_conversion(original(77, 7)) == "bracers+7",
          "Bracers AC 6 and AC 3 convert with their improvement");
    check(equipment_conversion(original(92, 2, {0, 89, 0x85})) == "cloak_of_displacement" &&
              equipment_conversion(original(67, 8, {0, 89, 0x85})) == "cloak_of_displacement",
          "Both Cloak of Displacement records convert");
    check(equipment_conversion(original(63, 3, {0, 38, 0x83})) == "gauntlets_of_ogre_power" &&
              equipment_conversion(original(67, 1, {0, 38, 0x83})) == "gauntlets_of_ogre_power",
          "Gauntlets of Ogre Power convert, as random treasure makes them too");
    check(equipment_conversion(original(76, -5, {}, true)) == "necklace-5:cursed",
          "The Cursed Necklace converts with its curse");
}

CharacterProfile profile(const RulesModule &rules, std::vector<std::string> gear)
{
    return rules.character_profile(fighter().sheet(), gear);
}

void armor_class()
{
    const auto rules = module();
    const int bare = profile(*rules, {}).armor_class;
    const int dex = bare - 10;
    check(profile(*rules, {"ring_of_protection+2"}).armor_class == bare + 2 &&
              profile(*rules, {"ring_of_protection+1", "ring_of_protection+2"}).armor_class ==
                  bare + 2,
          "A Ring of Protection adds its bonus; only the better of two counts");
    check(profile(*rules, {"plate+1", "ring_of_protection+2"}).armor_class == 18 + 1 + 2,
          "A Ring of Protection stacks with magic armor");
    check(profile(*rules, {"bracers+4"}).armor_class == 10 + 4 + dex &&
              profile(*rules, {"bracers+6", "chain_mail"}).armor_class == 10 + 6 + dex &&
              profile(*rules, {"bracers+4", "plate"}).armor_class == 18 &&
              profile(*rules, {"bracers+4", "shield"}).armor_class == 10 + 4 + dex + 2,
          "Bracers set Armor Class to 10 + the bonus + Dexterity, or the armor's if better; a "
          "shield still adds");
    check(profile(*rules, {"cloak_of_displacement"}).armor_class == bare + 2,
          "The Cloak of Displacement adds 2");
    check(profile(*rules, {"necklace-5:cursed"}).armor_class == bare - 5,
          "The Cursed Necklace takes 5");
    check(profile(*rules, {"ring_of_fire_resistance", "ring_of_feather_falling"}).armor_class ==
              bare,
          "The other rings leave Armor Class alone");
}

void saves()
{
    const auto rules = module();
    check(profile(*rules, {}).item_save_bonus == 0 &&
              profile(*rules, {"ring_of_protection+2"}).item_save_bonus == 2 &&
              profile(*rules, {"ring_of_protection+3", "necklace-5:cursed"}).item_save_bonus ==
                  -2,
          "A Ring of Protection adds to saves; the Cursed Necklace takes 5");
}

void strength()
{
    const auto rules = module();
    const int plain = profile(*rules, {"longsword"}).melee_attack_bonus;
    const int modifier = fighter().sheet().modifiers[Ability::strength];
    check(modifier < 4 &&
              profile(*rules, {"longsword", "gauntlets_of_ogre_power"}).melee_attack_bonus ==
                  plain + 4 - modifier,
          "Gauntlets of Ogre Power make Strength 19");
}

void limits()
{
    const auto rules = module();
    check(profile(*rules, {"longsword", "shield", "chain_mail", "ring_of_protection+1",
                           "ring_of_fire_resistance", "bracers+4", "cloak_of_displacement",
                           "gauntlets_of_ogre_power", "necklace-5:cursed"})
                  .armor_class > 0,
          "Two rings and one of each other worn item, beside a weapon, armor and a shield");
    test::rejects(
        [&]
        {
            (void)profile(*rules, {"ring_of_protection+1", "ring_of_fire_resistance",
                                   "ring_of_feather_falling"});
        },
        "A third ring is refused");
    test::rejects(
        [&]
        {
            (void)profile(*rules, {"cloak_of_displacement", "cloak_of_displacement"});
        },
        "A second cloak is refused");
    check(rules->equipment_info("ring_of_protection+1").slot == EquipmentSlot::worn &&
              rules->equipment_info("bracers+4").hands == 0,
          "Worn items need no hand");
}

// Loot from the original is worn, named once identified, and kept by a save.
void campaign()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    auto ring = original(69, 3, {0, 61, 0x81});
    ring.stored.name_components = {0, 1, 2};
    ring.stored.hidden_name_words = 6;
    ring.decoded_name = "Ring of Fire Resistance";
    ring.unidentified_name = "Ring";
    check(party.award_loot({}, {original(93, 2), ring}, "test:worn"), "Collect two rings");
    const auto items = party.member(id).character.inventory().items();
    const auto protection = items[items.size() - 2].id, fire = items.back().id;
    const int before = party.profile(id).armor_class;
    party.equip(id, protection);
    party.equip(id, fire);
    check(party.profile(id).armor_class == before + 2, "Worn, the Ring of Protection +2 adds 2");
    check(party.profile(id).item_modifiers.find("Fire Resistance") == std::string::npos,
          "The sheet does not say what an unidentified ring does");
    check(srd5::equipment_note(party.member(id).character.sheet(),
                               rules_definition(party.member(id), items.back())) ==
              "Worn: it needs no hand.",
          "Nor does the inventory");
    Purse purse{};
    coins(purse, Coin::gold) = 200;
    party.set_wealth(purse);
    check(party.identify_at_shop(id, fire) &&
              party.profile(id).item_modifiers.find("Ring of Fire Resistance") !=
                  std::string::npos,
          "Identified, the sheet names it and says what it does");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "worn"),
                                     *srd5::character_rules(), *rules, "worn", nullptr)
                         .party);
    check(restored.profile(id).armor_class == before + 2 &&
              restored.member(id).equipped == party.member(id).equipped,
          "A save keeps the worn rings");
}

// The Cursed Necklace cannot be taken off until Remove Curse.
void cursed_necklace()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    check(party.award_loot({}, {original(76, -5, {}, true)}, "test:necklace"),
          "Collect the necklace");
    const auto necklace = party.member(id).character.inventory().items().back().id;
    const int before = party.profile(id).armor_class;
    party.equip(id, necklace);
    check(party.profile(id).armor_class == before - 5 &&
              party.profile(id).item_save_bonus == -5,
          "Worn, it takes 5 from Armor Class and saves");
    test::rejects(
        [&]
        {
            party.unequip(id, necklace);
        },
        "It cannot be taken off");
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

unsigned count_logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return static_cast<unsigned>(std::count_if(log.begin(), log.end(),
                                               [&](const auto &line)
                                               {
                                                   return line.find(text) != std::string::npos;
                                               }));
}

// The first fighter's turn in a fight with a second fighter, the foe, who
// wears `worn` at `foe`.
std::unique_ptr<CombatSession> duel(const RulesModule &rules,
                                    std::vector<std::pair<std::string, unsigned>> carried,
                                    std::vector<std::string> worn, Cell foe, std::uint64_t seed)
{
    Participant p{1, "campaign-character", "Fighter", Side::party, {1, 1},
                  rules.character_profile(fighter().sheet(), std::vector<std::string>{}).data};
    std::uint64_t id = 7;
    for (const auto &[definition, quantity] : carried)
    {
        p.inventory.push_back({id++, definition, quantity});
    }
    Participant other{2, "campaign-character", "Foe", Side::opposition, foe,
                      rules.character_profile(fighter("Foe").sheet(), worn).data};
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)}, {p, other}}, seed);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "End the foe's turn");
    }
    check(c->snapshot().actor == 1, "The fighter acts");
    return c;
}

// The first attack roll against the cloak's wearer in a fight misses.
void displacement()
{
    const auto rules = module();
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto c = duel(*rules, {}, {"cloak_of_displacement"}, {2, 1}, seed);
        check(submit(*c, "melee", 2), "Attack the cloak's wearer");
        if (!count_logged(*c, "strikes Foe's displaced image"))
        {
            continue; // A natural 1, which leaves the cloak's miss for later.
        }
        check(rules->restore(c->save())->save() == c->save(),
              "A checkpoint keeps the spent displacement");
        check(submit(*c, "end") && submit(*c, "end") && c->snapshot().actor == 1 &&
                  submit(*c, "melee", 2) && count_logged(*c, "displaced image") == 1,
              "Only the first attack misses");
        return;
    }
    check(false, "An attack strikes the displaced image");
}

// The original's Ring of Fire Resistance: magical fire deals 2 less per die,
// at least 1 per die; ordinary fire deals none.
void fire_resistance()
{
    const auto rules = module();
    auto c = duel(*rules, {{"necklace_of_missiles", 1}}, {"ring_of_fire_resistance"}, {6, 1}, 5);
    check(submit(*c, "fireball_item"), "Throw a missile");
    for (const auto &command : c->legal_commands())
    {
        if (command.verb == "area_move" && command.destination == Cell{6, 1})
        {
            (void)c->submit(command);
            break;
        }
    }
    check(submit(*c, "area_cast"), "The missile explodes on the ring's wearer");
    const auto log = c->snapshot().log();
    const auto line = std::find_if(log.begin(), log.end(),
                                   [](const auto &text)
                                   {
                                       return text.find("Ring of Fire Resistance reduces") !=
                                              std::string::npos;
                                   });
    check(line != log.end(), "The ring reduces the Fireball's damage");
    const auto from = line->find(" from "), to = line->find(" to ");
    const int before = std::stoi(line->substr(from + 6)), after = std::stoi(line->substr(to + 4));
    check(after == std::max(8, before - 16), "Eight dice: 16 less, but at least 8");

    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto torch = duel(*rules, {{"torch", 1}}, {"ring_of_fire_resistance"}, {2, 1}, seed);
        check(submit(*torch, "torch", 2), "Strike with a Torch");
        if (count_logged(*torch, "reduces the Fire damage from 1 to 0"))
        {
            return;
        }
    }
    check(false, "A Torch's ordinary fire deals no damage");
}
} // namespace

// The Rogue's Shop's gear (SECRET-1): Boots and a Cloak of Elvenkind give
// Stealth checks Advantage, a Stone of Good Luck adds 1 to ability checks and
// saving throws, and Magic Lock Picks give the check to pick a lock Advantage.
void rogue_shop_gear()
{
    check(equipment_conversion(original(215, 0)) == "magic_lock_picks" &&
              equipment_conversion(original(216, 0)) == "boots_of_elvenkind" &&
              equipment_conversion(original(217, 0)) == "cloak_of_elvenkind" &&
              equipment_conversion(original(218, 0)) == "stone_of_good_luck",
          "The shop's gear converts");
    const auto rules = module();
    const auto sheet = fighter().sheet();
    const auto check_with = [&](std::vector<std::string> gear, std::string_view skill)
    {
        return rules->ability_check(sheet, gear, Ability::dexterity, skill);
    };
    check(!check_with({}, "stealth").advantage &&
              check_with({"boots_of_elvenkind"}, "stealth").advantage &&
              check_with({"cloak_of_elvenkind"}, "stealth").advantage &&
              !check_with({"boots_of_elvenkind"}, "acrobatics").advantage,
          "Elvenkind boots and cloak give Stealth checks Advantage");
    check(check_with({"stone_of_good_luck"}, "acrobatics").total ==
                  check_with({}, "acrobatics").total + 1 &&
              profile(*rules, {"stone_of_good_luck"}).item_save_bonus == 1 &&
              profile(*rules, {"stone_of_good_luck"}).armor_class == profile(*rules, {}).armor_class,
          "A Stone of Good Luck adds 1 to ability checks and saving throws");
    check(check_with({"magic_lock_picks"}, "sleight_of_hand").advantage &&
              !check_with({}, "sleight_of_hand").advantage &&
              rules->equipment_info("magic_lock_picks").slot == EquipmentSlot::carried,
          "Magic Lock Picks are carried and give picking a lock Advantage");
    check(profile(*rules, {"boots_of_elvenkind", "cloak_of_elvenkind", "stone_of_good_luck",
                           "ring_of_protection+1"})
                      .armor_class > 0 &&
              rules->equipment_info("boots_of_elvenkind").slot == EquipmentSlot::worn &&
              rules->equipment_info("stone_of_good_luck").hands == 0,
          "The boots, cloak and stone are worn together, needing no hand");
    test::rejects(
        [&]
        {
            (void)profile(*rules, {"cloak_of_elvenkind", "cloak_of_displacement"});
        },
        "Only one cloak is worn");
}

int main()
{
    try
    {
        conversion();
        rogue_shop_gear();
        armor_class();
        saves();
        strength();
        limits();
        campaign();
        cursed_necklace();
        displacement();
        fire_resistance();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Worn item checks passed\n";
    return 0;
}
