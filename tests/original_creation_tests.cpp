#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/character_pool.h"
#include "opengold/original_combat.h"
#include "opengold/original.h"
#include "test_support.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace opengold;
using opengold::test::check;
using opengold::test::throws;
using rules::CreationField;

namespace
{
// The original's creation, as its pages ask it: race and gender with the
// name, class, alignment, the roll kept or rolled again, the icon, the
// portrait and the sheet.
void the_original_pages_in_their_order()
{
    CharacterCreator creator(original::character_rules(), 9);
    check(creator.draft().race == "dwarf" &&
              creator.rules().allows(creator.draft(), CreationField::character_class,
                                     creator.draft().character_class) &&
              creator.rules().allows(creator.draft(), CreationField::alignment,
                                     creator.draft().alignment),
          "A new character starts on choices its race and class allow");
    check(throws(
              [&]
              {
                  creator.select(CreationField::character_class, "magic_user");
              }),
          "A dwarf cannot be a magic-user");
    creator.select(CreationField::race, "elf");
    creator.select(CreationField::character_class, "fighter_magic_user");
    creator.select(CreationField::race, "dwarf");
    check(creator.draft().character_class != "fighter_magic_user" &&
              creator.rules().allows(creator.draft(), CreationField::character_class,
                                     creator.draft().character_class),
          "A race that does not allow the class takes the first class it does");
    creator.select(CreationField::race, "human");
    creator.select(CreationField::character_class, "thief");
    check(creator.rules().allows(creator.draft(), CreationField::alignment,
                                 creator.draft().alignment) &&
              creator.draft().alignment != "lawful_good",
          "A class that does not allow the alignment takes the first it does");
    check(throws(
              [&]
              {
                  creator.next();
              }),
          "The name comes first");
    creator.name("Tessa");
    creator.next();
    check(creator.step() == CreationStep::character_class, "The class follows the race");
    creator.next();
    check(creator.step() == CreationStep::alignment, "The alignment follows the class");
    creator.next();
    check(creator.step() == CreationStep::attributes, "The roll follows the alignment");
    check(!creator.scores_assigned() && throws(
                                            [&]
                                            {
                                                creator.next();
                                            }),
          "The character must be rolled before going on");
    creator.roll();
    const auto first = creator.draft().rules_data;
    check(creator.scores_assigned() && creator.starting_gold() > 0 &&
              creator.starting_gold() == creator.sheet().original->gold,
          "The roll gives the scores and the gold the class rolled");
    creator.roll();
    check(creator.draft().rules_data != first, "Rolling again replaces the character");
    creator.back();
    creator.back();
    creator.select(CreationField::character_class, "fighter");
    check(!creator.scores_assigned() && creator.starting_gold() == 0,
          "Another class is rolled again");
    creator.next();
    creator.next();
    creator.roll();
    creator.next();
    check(creator.step() == CreationStep::combat_icon,
          "No training or spell page: the icon follows the roll");
    creator.next();
    check(creator.step() == CreationStep::portrait, "The portrait follows the icon");
    creator.next();
    check(creator.step() == CreationStep::sheet, "The sheet ends creation");
    const auto character = creator.create_character();
    check(character.sheet().identity == original::identity() &&
              character.sheet().character_class == "Fighter" && character.sheet().original,
          "The character is made under the original rules");
    creator.back();
    check(creator.step() == CreationStep::portrait, "Back returns along the same pages");
}

// A human fighter made on the original's pages.
Character made_fighter(std::shared_ptr<const rules::CharacterRules> rules)
{
    CharacterCreator creator(std::move(rules), 21);
    creator.select(CreationField::race, "human");
    creator.select(CreationField::character_class, "fighter");
    creator.name("Gareth");
    for (int page = 0; page < 3; ++page)
    {
        creator.next();
    }
    creator.roll();
    for (int page = 0; page < 3; ++page)
    {
        creator.next();
    }
    return creator.create_character();
}

// Training under the original rules: the experience share, readiness by the
// class's table, the level and its Hit Points rolled once and kept, and the
// level made again the same from a save.
void a_fighter_trains_and_keeps_the_level()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = party.add_pc(made_fighter(creation));
    const auto &sheet = party.member(id).character.sheet();
    const bool strong = sheet.scores[rules::Ability::strength] > 15;
    party.award_experience(1000, "test:first");
    check(party.member(id).experience == (strong ? 1100U : 1000U),
          "A fighter's share of experience follows its Strength");
    check(!party.can_advance(id), "A fighter is not ready below 2001 experience");
    party.award_experience(1001, "test:second");
    check(party.can_advance(id), "A fighter is ready at 2001 experience");
    const int before = party.member(id).character.sheet().hit_points;
    party.advance(id, party.default_advancement(id));
    const auto &trained = party.member(id).character.sheet();
    check(trained.original->class_levels[2] == 2 && trained.level == 2 &&
              trained.hit_points > before &&
              party.member(id).vitals.hit_points == trained.hit_points,
          "Training raises the fighter to level 2 with more Hit Points");
    check(!party.member(id).character.advancements().front().rules_data.empty(),
          "The level keeps what it rolled");
    const auto saved = encode_campaign(party, nullptr, "training-check");
    const auto module = original::rules_module();
    const auto loaded = decode_campaign(saved, *creation, *module, "training-check", nullptr);
    check(loaded.party.roster.front().character.sheet().hit_points == trained.hit_points &&
              loaded.party.roster.front().character.sheet().original->class_levels ==
                  trained.original->class_levels,
          "A saved level is made again the same");
    check(throws(
              [&]
              {
                  (void)encode_campaign_in_format(party, nullptr, "training-check", 49);
              }),
          "An older format cannot keep what a level rolled");

    // Experience for two levels trains one, and is cut short of the second.
    party.award_experience(60000, "test:lots");
    party.advance(id, party.default_advancement(id));
    check(party.member(id).character.sheet().original->class_levels[2] == 3 &&
              party.member(id).experience == 8000,
          "Training takes one level and keeps just short of the next but one");
}

// An item of a type, as a shop sells it: no name of its own, with its weight.
por::Equipment original_item(const std::vector<por::ItemTemplate> &types, std::uint8_t type,
                             std::uint16_t weight)
{
    std::array<std::uint8_t, 63> raw{};
    raw[46] = type;
    raw[55] = static_cast<std::uint8_t>(weight & 0xFF);
    raw[56] = static_cast<std::uint8_t>(weight >> 8);
    por::Equipment item;
    item.stored = por::decode_items(raw)->front();
    item.base = types.at(type);
    return item;
}

MemberId member_with(CampaignParty &party,
                     const std::shared_ptr<const rules::CharacterRules> &rules,
                     std::string profession, std::string name)
{
    CharacterCreator creator(rules, 30);
    creator.select(CreationField::race, "human");
    creator.select(CreationField::character_class, profession);
    creator.name(std::move(name));
    for (int page = 0; page < 3; ++page)
    {
        creator.next();
    }
    creator.roll();
    for (int page = 0; page < 3; ++page)
    {
        creator.next();
    }
    return party.add_pc(creator.create_character());
}

std::uint64_t carried(CampaignParty &party, MemberId id, const por::Equipment &item)
{
    party.take_item(id, item);
    return party.member(id).character.inventory().items().back().id;
}

// Readying the original's items as the original allows, and fighting with
// them: their dice, armor and the class that may use them.
void characters_ready_original_gear()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        std::cout << "Pool of Radiance's files are not here; its items are not checked.\n";
        return;
    }
    std::ifstream file(std::filesystem::path(game) / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    const auto types = *por::decode_item_templates(bytes);
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module({{}, types}), creation);
    party.add_coins(Coin::gold, 1000);
    constexpr std::uint8_t long_sword = 36, two_handed_sword = 38, leather = 50, shield = 59;
    const auto fighter = member_with(party, creation, "fighter", "Gareth");
    const auto unarmed = party.profile(fighter);
    const auto sword = carried(party, fighter, original_item(types, long_sword, 60));
    const auto armor = carried(party, fighter, original_item(types, leather, 150));
    check(party.member(fighter).character.inventory().items().front().definition_id.starts_with(
              "original:"),
          "An item under the original rules is keyed by its original record");
    party.equip(fighter, sword, rules::EquipmentOperation::equip);
    party.equip(fighter, armor, rules::EquipmentOperation::equip);
    const auto armed = party.profile(fighter);
    const auto numbers = *original::Combatant::decode(armed.data);
    check(numbers.attacks[0].dice == 1 && numbers.attacks[0].sides == 8,
          "A long sword strikes for 1d8");
    check(armed.armor_class == unarmed.armor_class - 2, "Leather armor is 2 better than none");
    const auto big = carried(party, fighter, original_item(types, two_handed_sword, 150));
    const auto held = carried(party, fighter, original_item(types, shield, 100));
    check(throws(
              [&]
              {
                  party.equip(fighter, big, rules::EquipmentOperation::equip);
              }),
          "Only one weapon is readied at a time");
    party.unequip(fighter, sword);
    party.equip(fighter, big, rules::EquipmentOperation::equip);
    check(throws(
              [&]
              {
                  party.equip(fighter, held, rules::EquipmentOperation::equip);
              }),
          "A two-handed sword leaves no hand for a shield");

    const auto mage = member_with(party, creation, "magic_user", "Mira");
    const auto blade = carried(party, mage, original_item(types, long_sword, 60));
    check(!party.can_use(mage, original_item(types, long_sword, 60)) &&
              throws(
                  [&]
                  {
                      party.equip(mage, blade, rules::EquipmentOperation::equip);
                  }),
          "A magic-user cannot ready a long sword");
}

// The Character Pool under the original rules: four of each class a race may
// be, each a
// race and alignment the class allows, rolled the same each time.
void the_pool_offers_original_characters()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        return;
    }
    const auto art = por::CharacterArt::load(game);
    const auto rules = original::character_rules();
    const auto pool = character_pool(*rules, art);
    const auto again = character_pool(*rules, art);
    check(pool.size() == 44,
          "The pool has four characters of each class a race may be (not the cleric/thief)");
    for (std::size_t n = 0; n < pool.size(); ++n)
    {
        const auto &draft = pool[n].creation_data();
        check(pool[n].sheet().original && pool[n].sheet().identity == original::identity() &&
                  rules->allows(draft, CreationField::character_class, draft.character_class) &&
                  rules->allows(draft, CreationField::alignment, draft.alignment),
              "A pool character is made under the original rules, its choices allowed");
        check(again[n].creation_data().rules_data == draft.rules_data &&
                  again[n].creation_data().race == draft.race,
              "The pool is the same each time");
    }
}

} // namespace

int main()
{
    try
    {
        the_original_pages_in_their_order();
        a_fighter_trains_and_keeps_the_level();
        characters_ready_original_gear();
        the_pool_offers_original_characters();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
