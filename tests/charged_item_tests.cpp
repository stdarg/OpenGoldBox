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
} // namespace

int main()
{
    try
    {
        conversion();
        campaign();
        std::cout << "Charged item tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
