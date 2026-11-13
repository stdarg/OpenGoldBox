#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/character_pool.h"
#include "opengold/coin_purse.h"
#include "opengold/training_hall.h"
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
    check(!party.ready_to_advance(id), "A fighter is not ready below 2001 experience");
    party.award_experience(1001, "test:second");
    check(party.ready_to_advance(id) && !party.can_advance(id),
          "A fighter is ready at 2001 experience, and trains only at a hall");
    party.add_coins(Coin::gold, 3000);
    check(party.hall_training(id, TrainingHall::clerics).denial == TrainingDenial::other_hall,
          "The clerics' hall does not train a fighter");
    const auto hall = party.hall_training(id, TrainingHall::fighters);
    check(hall.denial == TrainingDenial::none &&
              hall.classes == std::vector<std::string>{"fighter"},
          "The fighters' hall trains it as a fighter");
    const int before = party.member(id).character.sheet().hit_points;
    const auto gold = gold_value(party.state().wealth);
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    check(gold_value(party.state().wealth) == gold - 1000, "Training costs 1000 gp");
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
    party.train_at_hall_level(id, TrainingHall::fighters, {});
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

// An archer's arrows under the original rules: the fight takes their count
// from the inventory, spends one a shot (two in a round), and the inventory
// keeps what is left.
void arrows_spent_leave_the_inventory()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        return;
    }
    std::ifstream file(std::filesystem::path(game) / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    const auto types = *por::decode_item_templates(bytes);
    por::CharacterRecord kobold;
    kobold.name = "KOBOLD";
    kobold.base_thac0 = 20;
    kobold.base_armor_class = 7;
    kobold.max_hit_points = 40;
    kobold.base_movement = 1;
    kobold.base_attacks[0] = {2, {1, 4, 0}};
    original::Content content{{}, types};
    content.bestiary.emplace("MON2CHA.DAX:0", original::combatant_from_record(kobold));
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(content), creation);
    const auto archer = member_with(party, creation, "fighter", "Robin");
    constexpr std::uint8_t long_bow = 41, arrows = 73;
    const auto bow = carried(party, archer, original_item(types, long_bow, 30));
    auto quiver = original_item(types, arrows, 1);
    quiver.stored.stack_size = 5;
    quiver.stored.raw[57] = 5;
    const auto shafts = carried(party, archer, quiver);
    check(party.member(archer).character.inventory().find(shafts)->get().quantity == 5,
          "A stack of five arrows is five in the inventory");
    party.equip(archer, bow, rules::EquipmentOperation::equip);
    party.equip(archer, shafts, rules::EquipmentOperation::equip);

    const auto module = original::rules_module(content);
    rules::Encounter encounter;
    encounter.battlefield = {12, 6, std::vector<rules::Terrain>(72, rules::Terrain::open)};
    encounter.participants = party.participants();
    rules::Participant target;
    target.id = 1000;
    target.definition = "MON2CHA.DAX:0";
    target.name = "Kobold";
    target.side = rules::Side::opposition;
    target.cell = {10, 1};
    encounter.participants.push_back(target);
    party.begin_combat();
    auto session = module->create(encounter, 5);
    int shots = 0;
    for (int step = 0;
         step < 200 && session->snapshot().outcome == rules::Outcome::ongoing && shots < 1; ++step)
    {
        const auto commands = session->legal_commands();
        const auto shoot = std::ranges::find_if(commands,
                                                [&](const rules::Command &command)
                                                {
                                                    return command.actor == archer &&
                                                           command.target == 1000 &&
                                                           command.verb != "cast";
                                                });
        const auto end = std::ranges::find(commands, std::string("end"), &rules::Command::verb);
        if (shoot != commands.end())
        {
            ++shots;
        }
        check(session->submit(shoot != commands.end() ? *shoot : *end), "The fight goes on");
    }
    const auto snapshot = session->snapshot();
    const auto &view = *std::ranges::find(snapshot.combatants, archer, &rules::CombatantView::id);
    const auto reported =
        std::ranges::find(view.item_uses_left, shafts, &std::pair<std::uint64_t, unsigned>::first);
    check(shots > 0 && reported != view.item_uses_left.end() && reported->second < 5 &&
              reported->second > 0,
          "The fight reports the arrows left after its shots");
    party.apply_combat(snapshot);
    party.end_combat();
    check(party.member(archer).character.inventory().find(shafts)->get().quantity ==
              reported->second,
          "The inventory keeps the arrows the fight left");
}

// A magic-user's magic under the original rules: it knows Detect Magic, Read
// Magic, Shield and Sleep; after a day's rest it memorizes as many as its
// slots hold, fights with them, keeps those it cast spent until the next
// day's rest, and learns a spell at its next training.
void a_magic_user_memorizes_and_learns()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto mage = member_with(party, creation, "magic_user", "Mira");
    const auto &known = party.member(mage).character.sheet().original->known_spells;
    check(known == std::vector<int>{11, 18, 19, 21},
          "A new magic-user knows Detect Magic, Read Magic, Shield and Sleep");
    check(party.rest(), "The party rests a day");
    const auto options = party.spell_choice_options(mage);
    check(options.may_prepare && options.prepared_count == 1 && options.preparation.size() == 4,
          "A level-1 magic-user memorizes one of its four spells");
    check(throws(
              [&]
              {
                  party.choose_spells(
                      mage,
                      {{}, std::vector<std::string>{"magic_user:sleep", "magic_user:shield"}});
              }),
          "No more spells than its slots");
    party.choose_spells(mage, {{}, std::vector<std::string>{"magic_user:sleep"}});
    const auto fighting = party.participants();
    const auto numbers = original::Combatant::decode(fighting.front().character_profile);
    check(numbers && numbers->spells == std::vector<int>{21}, "It fights with Sleep memorized");

    // A fight's state that left no spell: the next fight starts without one,
    // and a day's rest gives it back.
    auto cast = *numbers;
    cast.spells.clear();
    auto state = party.member(mage).vitals;
    state.resources = cast.encode();
    rules::Encounter encounter;
    encounter.battlefield = {6, 4, std::vector<rules::Terrain>(24, rules::Terrain::open)};
    encounter.participants = fighting;
    encounter.participants.front().state = state;
    original::Content content;
    por::CharacterRecord kobold;
    kobold.name = "KOBOLD";
    kobold.max_hit_points = 4;
    kobold.base_thac0 = 20;
    kobold.base_armor_class = 7;
    content.bestiary.emplace("MON2CHA.DAX:0", original::combatant_from_record(kobold));
    rules::Participant foe;
    foe.id = 1000;
    foe.definition = "MON2CHA.DAX:0";
    foe.name = "Kobold";
    foe.side = rules::Side::opposition;
    foe.cell = {5, 3};
    encounter.participants.push_back(foe);
    const auto session = original::rules_module(content)->create(encounter, 3);
    const auto started = session->snapshot();
    const auto &view = *std::ranges::find(started.combatants, mage, &rules::CombatantView::id);
    check(original::Combatant::decode(view.persistent.resources)->spells.empty(),
          "A spell cast in an earlier fight is still spent");
    auto rested = state;
    original::rules_module()->recover(rested, party.member(mage).character.sheet());
    check(rested.resources.empty(), "A day's rest memorizes the spent spells again");

    party.award_experience(2501, "test:mage-level");
    check(party.ready_to_advance(mage), "A magic-user is ready at 2501 experience");
    const auto learning = party.advancement_options(mage, party.default_advancement(mage));
    check(std::ranges::any_of(learning.spells,
                              [](const rules::AdvancementOption &option)
                              {
                                  return option.id == "magic_user:magic_missile";
                              }),
          "Training offers the magic-user a spell it does not know");
    party.add_coins(Coin::gold, 1000);
    party.train_at_hall_level(mage, TrainingHall::magic_users, {"magic_user:magic_missile"});
    const auto &after = party.member(mage).character.sheet().original->known_spells;
    check(std::ranges::find(after, 15) != after.end(), "It learns Magic Missile");
}

// A cleric's Cure Light Wounds in camp heals a member 1d8, up to its Hit
// Points, and spends the memorized copy.
void a_cleric_cures_in_camp()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto cleric = member_with(party, creation, "cleric", "Tobin");
    const auto fighter = member_with(party, creation, "fighter", "Gareth");
    check(party.rest(), "The party rests a day");
    party.choose_spells(cleric, {{}, std::vector<std::string>{"cleric:cure_light_wounds"}});
    check(party.camp_actions(cleric).size() == 1, "The cleric can cast Cure Light Wounds");
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        if (member.id == fighter)
        {
            member.vitals.hit_points = 1;
        }
    }
    party.restore(state);
    party.use_camp_action(cleric, fighter, party.camp_actions(cleric).front().id);
    const int healed = party.member(fighter).vitals.hit_points;
    check(healed > 1 && healed <= std::min(9, party.member(fighter).character.sheet().hit_points),
          "Cure Light Wounds heals 1d8");
    check(party.camp_actions(cleric).empty(), "The spell is spent");
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
        arrows_spent_leave_the_inventory();
        a_magic_user_memorizes_and_learns();
        a_cleric_cures_in_camp();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
