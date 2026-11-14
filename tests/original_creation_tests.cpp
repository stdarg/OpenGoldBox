#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/character_pool.h"
#include "opengold/creature_catalog.h"
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

// Curse's camp spells (its reconstruction's ovr023): Cure Serious Wounds
// heals 2d8+1, and Raise Dead brings back a dead member with 1 Hit Point.
void curse_clerics_heal_and_raise_in_camp()
{
    const auto module = original::rules_module();
    const auto creation =
        original::character_rules(original::default_aging, GoldBoxGame::curse_of_the_azure_bonds);
    RandomState random{19};
    auto draft = rules::CharacterDraft{};
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "cleric";
    draft.alignment = "lawful_good";
    draft.name = "Kirin";
    creation->roll_in_place(draft, random);
    auto priest = creation->evaluate(draft, rules::NameRequirement::required);
    priest.prepared_spells = {"cleric:cure_serious_wounds", "cleric:raise_dead"};
    rules::VitalState own{priest.hit_points, false, {}, {}};
    const auto actions = module->camp_actions(priest, own);
    check(actions.size() == 2, "The priest can cast both in camp");
    rules::VitalState hurt{1, false, {}, {}};
    module->use_camp_action(priest, own, priest, hurt, "cast:cleric:cure_serious_wounds", random);
    check(hurt.hit_points >= std::min(4, priest.hit_points) &&
              hurt.hit_points <= std::min(18, priest.hit_points),
          "Cure Serious Wounds heals 2d8+1");
    rules::VitalState dead{0, true, {}, {}};
    module->use_camp_action(priest, own, priest, dead, "cast:cleric:raise_dead", random);
    check(!dead.dead && dead.hit_points == 1, "Raise Dead brings the dead back with 1 Hit Point");
    const auto cost = module->revival_changes(priest, rules::Revival::spell);
    auto raised = priest;
    check(cost.size() == 1 && module->advance_character(raised, dead, cost.front()) &&
              raised.scores[rules::Ability::constitution] ==
                  priest.scores[rules::Ability::constitution] - 1 &&
              raised.hit_points == priest.hit_points,
          "The spell costs a point of Constitution and no Hit Points");
    check(module->camp_actions(priest, own).empty(), "Both spells are spent");
}

// A Pool of Radiance character brought into Curse (its ConvertPoolRadPlayer):
// its levels, Hit Points, experience and items come along, with 300
// platinum; its money stays behind.
void a_pool_character_comes_to_curse()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        std::cout
            << "Pool of Radiance's files are not here; bringing a character across is not checked.\n";
        return;
    }
    std::ifstream file(std::filesystem::path(game) / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    const auto types = *por::decode_item_templates(bytes);
    std::shared_ptr<const rules::CharacterRules> pool = original::character_rules();
    CampaignParty first(original::rules_module({{}, types}), pool);
    const auto id = first.add_pc(made_fighter(pool));
    first.add_coins(Coin::gold, 3000);
    first.award_experience(2001, "test:carried");
    first.train_at_hall_level(id, TrainingHall::fighters, {});
    const auto sword = carried(first, id, original_item(types, 36, 60));
    first.equip(id, sword, rules::EquipmentOperation::equip);
    const auto &veteran = first.member(id);

    const auto curse_game = GoldBoxGame::curse_of_the_azure_bonds;
    original::Content content;
    content.game = curse_game;
    content.item_types = types;
    std::shared_ptr<const rules::CharacterRules> curse =
        original::character_rules(original::default_aging, curse_game);
    CampaignParty second(original::rules_module(content), curse);
    const auto arrived = second.bring_across(veteran);
    const auto &member = second.member(arrived);
    const auto &sheet = member.character.sheet();
    check(sheet.original->class_levels == veteran.character.sheet().original->class_levels &&
              sheet.hit_points == veteran.character.sheet().hit_points &&
              member.experience == veteran.experience &&
              original::game_of(sheet.identity) == curse_game,
          "Its levels, Hit Points and experience come along, under Curse's rules");
    check(member.character.inventory().items().size() ==
                  veteran.character.inventory().items().size() &&
              member.equipped.size() == veteran.equipped.size(),
          "Its items come along, readied as they were");
    check(second.state().wealth[4] == 300 && second.state().wealth[3] == 0,
          "It brings 300 platinum and none of its gold");
    check(second.purse_gold() == 1500 && gold_value(second.state().wealth, srd_coin_rates) == 3000,
          "Under the original rules a platinum piece is worth 5 gold, under the SRD 10");
    // The Pool of Radiance save the game's check brings characters from
    // (opengold_godot_curse_from_pool): this fighter, a cleric and five
    // fighters, one of them out of the party: seven, one more than Curse's
    // party takes.
    if (const char *fixture = std::getenv("OPENGOLD_POOL_SAVE_FIXTURE"))
    {
        (void)member_with(first, pool, "cleric", "Elmira");
        for (const char *name : {"Brom", "Cassia", "Dain"})
        {
            (void)member_with(first, pool, "fighter", name);
        }
        first.remove(member_with(first, pool, "fighter", "Ferris"));
        (void)member_with(first, pool, "fighter", "Hilda");
        write_campaign_file(fixture,
                            encode_campaign(first, nullptr, campaign_asset_identity(game)));
    }
}

// Curse's paladin is protected from evil and lays on hands once a day for 2
// Hit Points a level; its ranger strikes giants harder (createPlayer,
// PaladinHeal, AffectRangerVsGiant in its reconstruction).
// Secret of the Silver Blades makes its characters with 200,000 experience
// a single class, trained up at once, and reads THAC0, saving throws and
// experience from its own longer tables (docs/ORIGINAL-RULES.md).
void secret_characters_start_trained()
{
    const auto creation = original::character_rules(original::default_aging,
                                                    GoldBoxGame::secret_of_the_silver_blades);
    RandomState random{31};
    const auto made = [&](std::string profession)
    {
        rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "male";
        draft.character_class = std::move(profession);
        draft.alignment = "neutral_good";
        draft.name = "Tarn";
        creation->roll_in_place(draft, random);
        return creation->evaluate(draft, rules::NameRequirement::required);
    };
    const auto fighter = made("fighter");
    const auto &warrior = *fighter.original;
    check(warrior.experience == 200000 && warrior.class_levels[2] == 8 && warrior.gold == 0,
          "A fighter starts with 200,000 experience at level 8 and no gold");
    check(warrior.thac0 == 13, "A level 8 fighter's THAC0 is 13");
    check(warrior.saving_throws == std::array<int, 5>{10, 11, 12, 12, 13},
          "A level 8 fighter saves as Secret's table has it");
    const auto magic_user = made("magic_user");
    const auto &mage = *magic_user.original;
    check(mage.class_levels[5] == 9 && mage.thac0 == 19 &&
              mage.saving_throws == std::array<int, 5>{13, 11, 9, 13, 10},
          "A magic-user reaches level 9, with THAC0 19 and a level 9 magic-user's saves");
    const auto knows = [&mage](int number)
    {
        return std::ranges::find(mage.known_spells, number) != mage.known_spells.end();
    };
    // Training to level 9 teaches its four sets: Fireball at 6, Charm
    // Monsters at 7, Fire Shield at 8, Hold Monster at 9; and Shield from the
    // start. Its level 6 set's Cure Light Wounds is left out.
    check(knows(19) && knows(47) && knows(181) && knows(185) && knows(194) && !knows(3),
          "A magic-user learns Secret's creation spells");
    check(original::experience_for_level(original::ClassSlot::thief, 18,
                                         GoldBoxGame::secret_of_the_silver_blades) == 1760001 &&
              !original::experience_for_level(original::ClassSlot::fighter, 16,
                                              GoldBoxGame::secret_of_the_silver_blades),
          "Thieves rise to level 18, fighters to 15");

    // Secret's training takes no money (its TrainingMenu charges nothing).
    std::shared_ptr<const rules::CharacterRules> secret_creation = original::character_rules(
        original::default_aging, GoldBoxGame::secret_of_the_silver_blades);
    original::Content secret_content;
    secret_content.game = GoldBoxGame::secret_of_the_silver_blades;
    CampaignParty party(original::rules_module(secret_content), secret_creation);
    const auto id = party.add_pc(made_fighter(secret_creation));
    party.award_experience(50000, "test:secret");
    const auto hall = party.hall_training(id, TrainingHall::fighters);
    check(hall.denial == TrainingDenial::none, "A Secret hall trains a party with no gold");
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    check(party.member(id).character.sheet().original->class_levels[2] == 9 &&
              gold_value(party.state().wealth) == 0,
          "and the training costs nothing");

    // Secret's fighters and paladins strike four times in two rounds above
    // level 12 (its RecalcClassStats).
    // Training keeps the experience short of the level after next, so each
    // level needs its own award.
    for (int level = 9; level < 13; ++level)
    {
        party.award_experience(500'000, "test:secret-veteran-" + std::to_string(level));
        party.train_at_hall_level(id, TrainingHall::fighters, {});
    }
    const auto &veteran = party.member(id).character.sheet();
    const auto numbers = *original::Combatant::decode(
        original::rules_module(secret_content)->character_profile(veteran, {}).data);
    check(veteran.original->class_levels[2] == 13 && numbers.attacks[0].rate_per_two_rounds == 4,
          "A level 13 fighter attacks four times in two rounds");
}

void curse_paladins_and_rangers()
{
    const auto module = original::rules_module();
    const auto creation =
        original::character_rules(original::default_aging, GoldBoxGame::curse_of_the_azure_bonds);
    RandomState random{23};
    const auto made = [&](std::string profession, std::string alignment)
    {
        rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "male";
        draft.character_class = std::move(profession);
        draft.alignment = std::move(alignment);
        draft.name = "Valen";
        creation->roll_in_place(draft, random);
        return creation->evaluate(draft, rules::NameRequirement::required);
    };
    const auto paladin = made("paladin", "lawful_good");
    const auto ranger = made("ranger", "neutral_good");
    const auto has = [&](const rules::CharacterSheet &sheet, int code)
    {
        const auto numbers =
            *original::Combatant::decode(module->character_profile(sheet, {}).data);
        return std::ranges::any_of(numbers.effects,
                                   [code](const original::Effect &e)
                                   {
                                       return e.code == code;
                                   });
    };
    check(has(paladin, 8) && has(ranger, 334),
          "A paladin is protected from evil; a ranger strikes giants harder");
    rules::VitalState own{paladin.hit_points, false, {}, {}};
    rules::VitalState hurt{1, false, {}, {}};
    const int level = paladin.original->class_levels[3];
    module->use_camp_action(paladin, own, paladin, hurt, "paladin:lay_on_hands", random);
    check(hurt.hit_points == std::min(1 + 2 * level, paladin.hit_points),
          "Laying on hands heals 2 Hit Points a paladin level");
    check(std::ranges::none_of(module->camp_actions(paladin, own),
                               [](const rules::CampAction &a)
                               {
                                   return a.id == "paladin:lay_on_hands";
                               }),
          "Once a day");
}

// The original's temple: Cure Light Wounds heals 1d8 a wounded member, and
// Raise Dead brings the dead back with 1 Hit Point.
void the_temple_heals_and_raises()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = member_with(party, creation, "fighter", "Gareth");
    const auto &sheet = party.member(id).character.sheet();
    const auto module = original::rules_module();
    RandomState random{8};
    rules::VitalState wounded{1, false, {}, {}};
    check(module->temple_service_helps(sheet, wounded, rules::TempleService::cure_light_wounds) &&
              !module->temple_service_helps(sheet, wounded, rules::TempleService::raise_dead),
          "A wounded member can be cured, not raised");
    module->temple_service(wounded, sheet, rules::TempleService::cure_light_wounds, random);
    check(wounded.hit_points > 1 && wounded.hit_points <= std::min(9, sheet.hit_points),
          "Cure Light Wounds heals 1d8");
    rules::VitalState dead{0, true, {}, {}};
    module->temple_service(dead, sheet, rules::TempleService::raise_dead, random);
    check(!dead.dead && dead.hit_points == 1, "Raise Dead brings the dead back with 1 Hit Point");
    check(!module->temple_service_helps(sheet, dead, rules::TempleService::resurrection) &&
              !module->offers_temple_service(rules::TempleService::resurrection) &&
              module->offers_temple_service(rules::TempleService::raise_dead),
          "The original's temple has no Resurrection");
    rules::VitalState hurt{1, false, {}, {}};
    module->temple_service(hurt, sheet, rules::TempleService::heal, random);
    check(hurt.hit_points >= std::max(1, sheet.hit_points - 4),
          "Curse's Heal restores all but 1d4 Hit Points");
}

// Raise Dead's cost (temple.pas, RaiseDead): a point of Constitution, and at
// the temple the Hit Points its formula takes; the cost is in the member's
// level history, so a saved game keeps it.
void raise_dead_costs_constitution()
{
    using original::raised_hit_point_loss;
    constexpr original::ClassLevels fighter_9{0, 0, 9, 0, 0, 0, 0, 0};
    constexpr original::ClassLevels mage_5{0, 0, 0, 0, 0, 5, 0, 0};
    constexpr original::ClassLevels cleric_4{4, 0, 0, 0, 0, 0, 0, 0};
    check(raised_hit_point_loss(fighter_9, 16, 90, 63) == 1,
          "A fighter's 27 bonus Hit Points over its 18 at Constitution 16 lose 1");
    check(raised_hit_point_loss(cleric_4, 15, 30, 22) == 2,
          "A cleric's 8 bonus Hit Points over its 4 at Constitution 15 lose 2");
    check(raised_hit_point_loss(cleric_4, 13, 30, 22) == 0, "Below Constitution 14 none are lost");
    check(raised_hit_point_loss(mage_5, 17, 30, 20) == 0,
          "At 17 or more one without fighter levels loses none");

    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = party.add_pc(made_fighter(creation));
    party.add_coins(Coin::gold, 20000);
    party.award_experience(5000, "test:raised-training");
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    const auto before = party.member(id).character.sheet();
    check(before.original->rolled_hit_points > 0 &&
              before.original->rolled_hit_points <= before.hit_points + 2,
          "The sheet keeps the Hit Points its dice rolled");
    auto state = party.state();
    state.roster.front().vitals.dead = true;
    state.roster.front().vitals.hit_points = 0;
    party.restore(state);
    party.temple_service(id, rules::TempleService::raise_dead, 5500);
    const auto &raised = party.member(id);
    const int constitution = before.scores[rules::Ability::constitution] - 1;
    const int expected =
        std::max(1, before.hit_points - raised_hit_point_loss(before.original->class_levels,
                                                              constitution, before.hit_points,
                                                              before.original->rolled_hit_points));
    check(!raised.vitals.dead && raised.vitals.hit_points == 1 &&
              raised.character.sheet().scores[rules::Ability::constitution] == constitution &&
              raised.character.sheet().hit_points == expected,
          "The temple raises it with 1 Hit Point, a point of Constitution and its formula's loss");
    const auto module = original::rules_module();
    const auto loaded = decode_campaign(encode_campaign(party, nullptr, "raised-check"), *creation,
                                        *module, "raised-check", nullptr);
    const auto &kept = loaded.party.roster.front().character.sheet();
    check(kept.scores[rules::Ability::constitution] == constitution && kept.hit_points == expected,
          "A saved game keeps the cost");

    // Curse's characters start above level 1 with no history to replay.
    std::shared_ptr<const rules::CharacterRules> curse =
        original::character_rules(original::default_aging, GoldBoxGame::curse_of_the_azure_bonds);
    original::Content curse_content;
    curse_content.game = GoldBoxGame::curse_of_the_azure_bonds;
    CampaignParty started(original::rules_module(curse_content), curse);
    const auto &curse_module = started.rule_module();
    const int level = started.member(started.add_pc(made_fighter(curse))).character.sheet().level;
    const auto reloaded = decode_campaign(encode_campaign(started, nullptr, "curse-check"), *curse,
                                          curse_module, "curse-check", nullptr);
    check(level > 1 && reloaded.party.roster.front().character.sheet().level == level,
          "A Curse character saved at its starting level loads");
}

// A script's harm under the original rules: an arrow of THAC0 20 hits an
// unarmored fighter on 11 and up; a save of the script's category avoids
// the damage; a member falls at 0 and dies at -10.
void scripts_harm_as_the_original()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = member_with(party, creation, "fighter", "Gareth");
    const auto &sheet = party.member(id).character.sheet();
    const auto module = original::rules_module();
    const auto armor_class = original::Combatant::decode(party.profile(id).data)->armor_class;
    RandomState random{9};
    int hits = 0;
    for (int n = 0; n < 400; ++n)
    {
        rules::VitalState state{100, false, {}, {}};
        rules::HazardAttack arrow{0, 1, 6, 0, "piercing"};
        arrow.original_attack = 40;
        const auto result = module->hazard_attack(state, sheet, {}, arrow, random);
        check(result.hit == (result.natural == 20 ||
                             (result.natural > 1 && result.natural + 40 > 60 - armor_class)),
              "An arrow hits by the original's roll");
        hits += result.hit ? 1 : 0;
    }
    check(hits > 0 && hits < 400, "Some arrows hit and some miss");
    rules::HazardAttack gas{0, 30, 1, 0, "poison"};
    gas.automatic = true;
    gas.original_save = 0;
    int saved = 0;
    for (int n = 0; n < 200; ++n)
    {
        rules::VitalState state{5, false, {}, {}};
        const auto result = module->hazard_attack(state, sheet, {}, gas, random);
        saved += result.saved ? 1 : 0;
        check(result.saved ? state.hit_points == 5 : state.dead && state.hit_points == 0,
              "A save avoids the damage; 30 damage kills a member of 5 Hit Points");
    }
    check(saved > 0 && saved < 200, "Some save and some do not");
}

// The original's Potion of Healing outside a fight heals 2d4+2; an item of no
// such power is not used out of one.
void a_potion_heals_outside_a_fight()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = member_with(party, creation, "fighter", "Gareth");
    const auto &sheet = party.member(id).character.sheet();
    std::array<std::uint8_t, 63> raw{};
    raw[46] = 65;
    raw[60] = 1;
    raw[61] = 62 + 23;
    const auto potion = por::original_item_key(por::decode_items(raw)->front());
    std::array<std::uint8_t, 63> plain{};
    plain[46] = 36;
    const auto sword = por::original_item_key(por::decode_items(plain)->front());
    const auto module = original::rules_module();
    check(module->usable_outside_combat(potion) && !module->usable_outside_combat(sword),
          "A Potion of Healing is drunk outside a fight; a sword is not used");
    RandomState random{12};
    rules::VitalState state{1, false, {}, {}};
    module->drink_potion(sheet, state, potion, random);
    check(state.hit_points >= std::min(5, sheet.hit_points) &&
              state.hit_points <= std::min(11, sheet.hit_points),
          "The Potion of Healing heals 2d4+2");
}

// An NPC who joins under the original rules is its record: its scores,
// class levels and Hit Points.
void an_npc_joins_as_its_record()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        return;
    }
    const auto catalog = por::CreatureCatalog::load(game);
    const auto rules = original::character_rules();
    unsigned made = 0;
    for (const auto &[id, creature] : catalog.all())
    {
        const auto &record = creature.stored;
        const auto &l = record.levels;
        if (record.race < 1 || record.race > 7 || l.fighter + l.cleric + l.mage + l.thief == 0 ||
            record.max_hit_points == 0)
        {
            continue;
        }
        const auto draft = rules->record_draft(record.raw);
        check(draft.has_value(), "The original rules make a draft of a record");
        const Character npc(*rules, *draft, {});
        const auto &sheet = npc.sheet();
        check(sheet.hit_points == record.max_hit_points && sheet.original &&
                  sheet.original->class_levels[2] == l.fighter &&
                  sheet.original->class_levels[5] == l.mage,
              id.key() + " joins with its record's levels and Hit Points");
        ++made;
    }
    check(made > 0, "Some records are characters who could join");
}

// A level drain outlasts the fight (effhndlr.pas, the level drain): the
// character keeps the lower level and Hit Points, and its next training gives
// the level back (chargen.pas, TrainingMenu).
void a_drain_lasts_beyond_the_fight()
{
    std::shared_ptr<const rules::CharacterRules> creation = original::character_rules();
    CampaignParty party(original::rules_module(), creation);
    const auto id = party.add_pc(made_fighter(creation));
    party.add_coins(Coin::gold, 3000);
    party.award_experience(2001, "test:drain-first");
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    party.award_experience(2001, "test:drain-second");
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    const auto &module = party.rule_module();
    auto character = party.member(id).character;
    const auto before = character.sheet();
    check(before.original->class_levels[2] == 3, "The fighter trained to level 3");

    // What a wight's touch leaves at the fight's end: a level and its share.
    auto drained = *original::Combatant::decode(module.character_profile(before, {}).data);
    --drained.class_levels[2];
    drained.level = 2;
    drained.max_hit_points -= before.hit_points / 3;
    rules::VitalState after{
        std::min(before.hit_points, drained.max_hit_points), false, drained.encode(), {}};
    const auto changes = module.lasting_changes(before, after);
    check(changes.size() == 1, "The drain is a lasting change");
    check(module.lasting_changes(before, {before.hit_points, false, {}, {}}).empty(),
          "A fight that drains nothing leaves nothing");
    check(character.advance(module, after, changes.front()), "The character takes the drain");
    const auto &lower = character.sheet();
    check(lower.original->class_levels[2] == 2 && lower.level == 2 &&
              lower.hit_points == drained.max_hit_points &&
              module.hit_point_maximum(lower, after) == drained.max_hit_points &&
              lower.original->drained_levels == 1 &&
              lower.original->drained_hit_points == before.hit_points / 3 &&
              lower.original->thac0 == 19,
          "It is a level-2 fighter with the drained Hit Points gone");

    auto training = module.default_advancement(lower);
    RandomState random{5};
    module.roll_advancement(lower, party.member(id).experience, training, random);
    check(character.advance(module, after, training) &&
              character.sheet().original->class_levels[2] == 3 &&
              character.sheet().original->drained_levels == 0,
          "Training gives the drained level back");
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
        the_temple_heals_and_raises();
        raise_dead_costs_constitution();
        scripts_harm_as_the_original();
        a_potion_heals_outside_a_fight();
        an_npc_joins_as_its_record();
        a_drain_lasts_beyond_the_fight();
        curse_clerics_heal_and_raise_in_camp();
        a_pool_character_comes_to_curse();
        curse_paladins_and_rangers();
        secret_characters_start_trained();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
