#include "combat_fixture.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/item_key.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

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

// An AC 1 target that hits back, and the same as an Undead.
std::unique_ptr<RulesModule> module()
{
    return srd5::parse_content(read(source / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature risen 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "type risen undead\n");
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

por::Equipment original(std::uint8_t type, int bonus, std::array<std::uint8_t, 3> words = {},
                        std::array<std::uint8_t, 3> effects = {})
{
    por::Equipment item;
    item.stored.type = type;
    item.stored.magic_bonus = bonus;
    item.stored.name_components = words;
    item.stored.effect_codes = effects;
    item.stored.stack_size = 1;
    return item;
}

void item_keys()
{
    const auto flame = parse_item_key("longsword+2:flame_tongue");
    check(flame.base == "longsword" && flame.bonus == 2 && flame.power == ItemPower::flame_tongue,
          "A key carries its base, bonus and power");
    const auto cursed = parse_item_key("longsword-2:cursed");
    check(cursed.base == "longsword" && cursed.bonus == -2 && cursed.power == ItemPower::cursed,
          "A penalty is a negative bonus");
    check(parse_item_key("shield+1").base == "shield" && parse_item_key("shield+1").bonus == 1,
          "A plain enchantment");
    check(parse_item_key("light_crossbow").base == "light_crossbow" &&
              parse_item_key("por:unsupported:36").base == "por:unsupported:36" &&
              parse_item_key("longsword+0").base == "longsword+0",
          "Keys without an enchantment are all base");
    check(item_key("greatsword", 1, ItemPower::bane_undead) == "greatsword+1:undead" &&
              item_key("chain_mail", 0) == "chain_mail",
          "Keys are written as they are read");
}

// The original's records, as docs/MAGIC_ITEMS.md lists them.
void conversion()
{
    check(equipment_conversion(original(36, 1, {0, 162, 36})) == "longsword+1", "Long Sword +1");
    check(equipment_conversion(original(36, 2, {241, 163, 36}, {0, 22, 0})) ==
              "longsword+2:flame_tongue",
          "Long Sword +2 Flame Tongue");
    check(equipment_conversion(original(38, 1, {253, 162, 38}, {0, 3, 0x88})) ==
              "greatsword+1:undead",
          "Two-Handed Sword +1 +3 vs Undead");
    auto broad = original(35, -2, {255, 202, 35});
    broad.stored.cursed_raw = 1;
    check(equipment_conversion(broad) == "longsword-2:cursed", "Cursed Broad Sword -2");
    check(equipment_conversion(original(36, 3, {0, 164, 36}, {0, 82, 0x84})) == "longsword+3",
          "An undecoded special effect leaves the plain enchantment");
    check(equipment_conversion(original(55, 1)) == "chain_mail+1" &&
              equipment_conversion(original(59, 2)) == "shield+2" &&
              equipment_conversion(original(73, 1)) == "arrow+1" &&
              equipment_conversion(original(47, 2, {163, 200, 47})) == "sling+2",
          "Armor, shields, ammunition and the Sling of Seeking");
    check(equipment_conversion(original(21, 1, {157, 167, 21}, {1, 83, 0})) == "por:unsupported:21",
          "The Javelin of Lightning's charge is not an enchantment");
    check(equipment_conversion(original(79, 0, {}, {20, 88, 0})) == "por:unsupported:79",
          "A charged wand stays unsupported");
}

void profile()
{
    const auto rules = module();
    const auto hero = fighter();
    const auto with = [&](std::vector<std::string> gear)
    {
        return rules->character_profile(hero.sheet(), gear);
    };
    const auto plain = with({"longsword", "chain_mail", "shield"});
    const auto magic = with({"longsword+2", "chain_mail+1", "shield+2"});
    check(magic.melee_attack_bonus == plain.melee_attack_bonus + 2,
          "A +2 weapon shows on the attack bonus");
    check(magic.armor_class == plain.armor_class + 3, "Armor +1 and a Shield +2 add to AC");
    check(with({"longsword-2:cursed"}).melee_attack_bonus ==
              with({"longsword"}).melee_attack_bonus - 2,
          "A -2 weapon lowers it");
    const auto notes = magic.item_modifiers;
    check(notes.find("Longsword +2. Its magic adds +2 to attack and damage rolls.") !=
                  std::string::npos &&
              notes.find("Chain Mail +1. Its magic adds +1 to Armor Class.") != std::string::npos,
          "The sheet says what the magic adds");
}

struct Strike
{
    int bonus{}, damage{};
};

// The fighter's first attack on a creature of the given kind, with a fixed seed.
std::optional<Strike> strike(const RulesModule &rules, const std::string &weapon,
                             const std::string &enemy)
{
    const auto profile = rules.character_profile(fighter().sheet(), std::vector{weapon}).data;
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
                           {{1, "campaign-character", "Fighter", Side::party, {1, 1}, profile},
                            {2, enemy, "Foe", Side::opposition, {2, 1}}}},
                          5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        for (const auto &command : c->legal_commands())
        {
            if (command.verb == "end")
            {
                check(c->submit(command), "End the foe's turn");
                break;
            }
        }
    }
    for (const auto &command : c->legal_commands())
    {
        if (command.verb == "melee" && command.target == 2)
        {
            check(c->submit(command), "Attack");
            break;
        }
    }
    for (const auto &line : c->snapshot().log())
    {
        const auto hits = line.find(" hits for ");
        if (line.starts_with("Fighter -> Foe: d20 ") && hits != std::string::npos)
        {
            return Strike{std::stoi(line.substr(line.find(" + ") + 3)),
                          std::stoi(line.substr(hits + 10))};
        }
    }
    return std::nullopt;
}

void attacks()
{
    const auto rules = module();
    const auto plain = strike(*rules, "longsword", "target");
    const auto magic = strike(*rules, "longsword+2", "target");
    check(plain && magic, "Both attacks hit");
    check(magic->bonus == plain->bonus + 2 && magic->damage == plain->damage + 2,
          "A +2 weapon adds 2 to the attack and damage rolls");

    const auto bane = strike(*rules, "greatsword+1:undead", "target");
    const auto undead = strike(*rules, "greatsword+1:undead", "risen");
    const auto greatsword = strike(*rules, "greatsword", "target");
    check(bane && undead && greatsword, "The two-handed attacks hit");
    check(bane->bonus == greatsword->bonus + 1 && undead->bonus == greatsword->bonus + 3,
          "+1, or +3 against the Undead");
}

// Loot from the original keeps its enchantment in the party and in a save.
void campaign()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    const auto plain = party.profile(id).melee_attack_bonus;
    check(party.award_loot({}, {original(36, 2, {0, 163, 36})}, "test:magic-loot"),
          "Collect a Long Sword +2");
    const auto &sword = party.member(id).character.inventory().items().back();
    check(sword.definition_id == "longsword+2", "The loot converts with its bonus");
    party.equip(id, sword.id);
    check(party.profile(id).melee_attack_bonus == plain + 2, "Equipped, it adds its bonus");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "magic"),
                                     *srd5::character_rules(), *rules, "magic", nullptr)
                         .party);
    check(restored.member(id).character.inventory().items().back().definition_id == "longsword+2" &&
              restored.profile(id).melee_attack_bonus == plain + 2,
          "A save keeps the item and its bonus");
}
} // namespace

int main()
{
    try
    {
        item_keys();
        conversion();
        profile();
        attacks();
        campaign();
        std::cout << "Enchanted item tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
