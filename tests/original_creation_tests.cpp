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

    // A cursed item, once readied, stays on until Remove Curse lets its
    // wearer put it down (temple.pas, RemoveCurse).
    auto cursed_sword = original_item(types, long_sword, 60);
    std::array<std::uint8_t, 63> raw{};
    raw[46] = long_sword;
    raw[54] = 1;
    cursed_sword.stored = por::decode_items(raw)->front();
    const auto warrior = member_with(party, creation, "fighter", "Brand");
    const auto stuck = carried(party, warrior, cursed_sword);
    party.equip(warrior, stuck, rules::EquipmentOperation::equip);
    check(throws(
              [&]
              {
                  party.unequip(warrior, stuck);
              }),
          "A cursed sword cannot be put away");
    party.temple_service(warrior, rules::TempleService::remove_curse, 0);
    check(party.member(warrior).equipped.empty(), "Remove Curse lets its wearer put it down");
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

// Secret's Heal in camp (its reconstruction's SpellHeal): every Hit Point but
// 1d4, and nothing for one already that close.
void secret_clerics_heal_in_camp()
{
    const auto module = original::rules_module();
    const auto creation = original::character_rules(original::default_aging,
                                                    GoldBoxGame::secret_of_the_silver_blades);
    RandomState random{23};
    auto draft = rules::CharacterDraft{};
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "cleric";
    draft.alignment = "lawful_good";
    draft.name = "Mira";
    creation->roll_in_place(draft, random);
    auto priest = creation->evaluate(draft, rules::NameRequirement::required);
    priest.prepared_spells = {"cleric:heal", "cleric:heal#2"};
    rules::VitalState own{priest.hit_points, false, {}, {}};
    check(module->camp_actions(priest, own).size() == 1, "The priest can cast Heal in camp");
    rules::VitalState hurt{1, false, {}, {}};
    module->use_camp_action(priest, own, priest, hurt, "cast:cleric:heal", random);
    check(hurt.hit_points >= priest.hit_points - 4 && hurt.hit_points <= priest.hit_points - 1,
          "Heal leaves 1d4 short of the most");
    rules::VitalState scratched{priest.hit_points, false, {}, {}};
    module->use_camp_action(priest, own, priest, scratched, "cast:cleric:heal", random);
    check(scratched.hit_points == priest.hit_points, "One already that close gains nothing");
    check(module->camp_actions(priest, own).empty(), "Both are spent");
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

// Gateway to the Savage Frontier plays Curse's tables, but a new character
// starts with 3,000 experience and 35 platinum, and every class stops at its
// own level whatever the race (its levelCaps).
void gateway_characters_start_low_and_stop_at_caps()
{
    const auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
    std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, gateway);
    const auto made = [&](std::string profession)
    {
        rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "male";
        draft.character_class = std::move(profession);
        draft.alignment = "neutral_good";
        draft.name = "Ivo";
        RandomState random{12};
        creation->roll_in_place(draft, random);
        return creation->evaluate(draft, rules::NameRequirement::required);
    };
    const auto fighter = made("fighter");
    check(fighter.original->experience == 3000 && fighter.original->class_levels[2] == 2 &&
              fighter.original->platinum == 35 && fighter.original->gold == 0,
          "A Gateway fighter starts at level 2 with 3,000 experience and 35 platinum");
    check(made("thief").original->class_levels[6] == 3, "A thief reaches level 3");
    check(std::ranges::find(made("magic_user").original->known_spells, 12) !=
              made("magic_user").original->known_spells.end(),
          "A magic-user knows Enlarge, as in Curse");
    original::Content content;
    content.game = gateway;
    CampaignParty party(original::rules_module(content), creation);
    const auto id = party.add_pc(made_fighter(creation));
    party.add_coins(Coin::gold, 20000);
    for (int round = 0; round < 12; ++round)
    {
        party.award_experience(400'000, "test:gateway-" + std::to_string(round));
        if (party.hall_training(id, TrainingHall::fighters).denial == TrainingDenial::none)
        {
            party.train_at_hall_level(id, TrainingHall::fighters, {});
        }
    }
    check(party.member(id).character.sheet().original->class_levels[2] == 8,
          "A Gateway fighter stops at level 8");

    // Curse and Gateway train one class a visit: the one whose next level
    // needs the most experience (here the thief's level 3, 2,501, before the
    // fighter's level 2, 2,001).
    const auto pair = [&]
    {
        rules::CharacterDraft draft;
        draft.race = "halfling";
        draft.gender = "male";
        draft.character_class = "fighter_thief";
        draft.alignment = "neutral_good";
        draft.name = "Pip";
        RandomState random{5};
        creation->roll_in_place(draft, random);
        return Character(*creation, draft, {});
    }();
    const auto both = party.add_pc(pair);
    party.award_experience(30000, "test:gateway-pair");
    const auto before = party.member(both).character.sheet().original->class_levels;
    party.train_at_hall_level(both, TrainingHall::every_class, {});
    const auto after = party.member(both).character.sheet().original->class_levels;
    check(after[6] == before[6] + 1 && after[2] == before[2],
          "A fighter/thief trains only one class on one visit");
}

// Pools of Darkness starts a single class at 1,500,001 experience, trained
// up at once, with 200 platinum, and reads its own tables to level 40
// (docs/ORIGINAL-RULES.md, Pools of Darkness).
void darkness_characters_start_high()
{
    const auto darkness = GoldBoxGame::pools_of_darkness;
    std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, darkness);
    const auto made = [&](std::string profession)
    {
        rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "male";
        draft.alignment = profession == "paladin" ? "lawful_good" : "neutral_good";
        draft.character_class = std::move(profession);
        draft.name = "Vala";
        RandomState random{23};
        creation->roll_in_place(draft, random);
        return creation->evaluate(draft, rules::NameRequirement::required);
    };
    const auto fighter = made("fighter");
    const auto &warrior = *fighter.original;
    check(warrior.experience == 1500001 && warrior.class_levels[2] == 14 &&
              warrior.platinum == 200 && warrior.gold == 0,
          "A fighter starts with 1,500,001 experience at level 14 and 200 platinum");
    check(warrior.thac0 == 8, "A level 14 fighter's THAC0 is 8, from Darkness's table");
    check(warrior.saving_throws == std::array<int, 5>{5, 6, 7, 5, 8},
          "and it saves as a level 14 fighter");
    const auto magic_user = made("magic_user");
    const auto &mage = *magic_user.original;
    check(mage.class_levels[5] == 14 && mage.thac0 == 16,
          "A magic-user reaches level 14, with THAC0 16");
    const auto knows = [&mage](int number)
    {
        return std::ranges::find(mage.known_spells, number) != mage.known_spells.end();
    };
    // Every spell of levels 1 to 3 (Fireball, 47), the fourth level's set
    // (Charm Monsters, its 81) and the fifth's (Cloudkill, its 91).
    check(knows(19) && knows(47) && knows(181) && knows(191),
          "A magic-user learns Darkness's creation spells: " +
              std::to_string(mage.known_spells.size()));
    check(made("thief").original->class_levels[6] == 16 &&
              made("paladin").original->class_levels[3] == 12,
          "A thief reaches level 16, a paladin 12");
    check(original::experience_for_level(original::ClassSlot::paladin, 12, darkness) == 1400000 &&
              original::experience_for_level(original::ClassSlot::fighter, 40, darkness) ==
                  8000001 &&
              !original::experience_for_level(original::ClassSlot::fighter, 41, darkness),
          "Levels rise to 40, a paladin's twelfth at 1,400,000");
    // Every class needs a Charisma of 6 at least; a paladin keeps its 17.
    using original::Profession;
    check(original::class_minimums(Profession::fighter, darkness)[rules::Ability::charisma] == 6 &&
              original::class_minimums(Profession::paladin, darkness)[rules::Ability::charisma] ==
                  17 &&
              original::class_minimums(Profession::fighter)[rules::Ability::charisma] == 0,
          "Darkness asks a Charisma of 6 of every class");
    // A paladin or a ranger of Strength 18 rolls Exceptional Strength.
    original::RolledCharacter strong;
    strong.strength_percentile = 50;
    check(original::exceptional_strength(original::Race::human, original::Gender::male,
                                         Profession::paladin, strong, 18, darkness) == 50 &&
              original::exceptional_strength(original::Race::human, original::Gender::male,
                                             Profession::paladin, strong, 18) == 0,
          "A Darkness paladin rolls Exceptional Strength");

    // Its training takes no money.
    original::Content content;
    content.game = darkness;
    CampaignParty party(original::rules_module(content), creation);
    const auto id = party.add_pc(made_fighter(creation));
    party.award_experience(300'000, "test:darkness");
    check(party.hall_training(id, TrainingHall::fighters).denial == TrainingDenial::none,
          "A Darkness hall trains a party with no gold");
}

// Treasures of the Savage Frontier: a new character starts with 22,600
// experience (11,300 for each class of two, 7,533 of three), trained at once,
// and 200 platinum, on the tables of Pools of Darkness, its engine's.
void treasures_characters_start_with_their_experience()
{
    const auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;
    std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, treasures);
    const auto made = [&](std::string race, std::string profession)
    {
        rules::CharacterDraft draft;
        draft.race = std::move(race);
        draft.gender = "male";
        draft.character_class = std::move(profession);
        draft.alignment = "neutral_good";
        draft.name = "Brok";
        RandomState random{31};
        creation->roll_in_place(draft, random);
        return creation->evaluate(draft, rules::NameRequirement::required);
    };
    const auto fighter = made("human", "fighter");
    check(original::game_of(fighter.identity) == treasures, "The sheet names Treasures' edition");
    check(fighter.original->experience == 22600 && fighter.original->class_levels[2] == 5 &&
              fighter.original->platinum == 200,
          "A Treasures fighter starts at level 5 with 22,600 experience and 200 platinum");
    check(made("human", "thief").original->class_levels[6] == 6, "A thief reaches level 6");
    const auto pair = made("halfling", "fighter_thief");
    check(pair.original->experience == 11300 && pair.original->class_levels[2] == 4 &&
              pair.original->class_levels[6] == 5,
          "A fighter/thief has 11,300 for each class: fighter 4, thief 5");

    // A fighter stops at level 11 (its trainingLimits), and each visit costs
    // 200 platinum (1,000 gold's worth).
    original::Content content;
    content.game = treasures;
    CampaignParty party(original::rules_module(content), creation);
    const auto id = party.add_pc(made_fighter(creation));
    party.add_coins(Coin::platinum, 20000);
    const auto platinum_before = party.state().wealth[4];
    party.award_experience(30'000, "test:treasures-first");
    party.train_at_hall_level(id, TrainingHall::fighters, {});
    check(party.state().wealth[4] == platinum_before - 200,
          "Training costs 200 platinum: " + std::to_string(party.state().wealth[4]));
    for (int round = 0; round < 20; ++round)
    {
        party.award_experience(400'000, "test:treasures-" + std::to_string(round));
        if (party.hall_training(id, TrainingHall::fighters).denial == TrainingDenial::none)
        {
            party.train_at_hall_level(id, TrainingHall::fighters, {});
        }
    }
    check(party.member(id).character.sheet().original->class_levels[2] == 11,
          "A Treasures fighter stops at level 11");
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

// Secret's rangers start trained to level 8, so they memorize druid spells
// in the slots their level gives (classcal.pas,
// RecalcSpellLists).
void secret_rangers_memorize_their_spells()
{
    const auto module = original::rules_module();
    const auto creation = original::character_rules(original::default_aging,
                                                    GoldBoxGame::secret_of_the_silver_blades);
    RandomState random{29};
    rules::CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "ranger";
    draft.alignment = "neutral_good";
    draft.name = "Tamsin";
    creation->roll_in_place(draft, random);
    const auto ranger = creation->evaluate(draft, rules::NameRequirement::required);
    const auto &numbers = *ranger.original;
    check(numbers.class_levels[4] == 8 && numbers.druid_slots[0] > 0 &&
              numbers.magic_user_slots[0] == 0,
          "A Secret ranger starts at level 8, with a druid slot and no magic-user one yet");
    const auto options =
        module->spell_choice_options(ranger, rules::SpellChoiceContext::long_rest).preparation;
    const auto offered = [&options](std::string_view id)
    {
        return std::ranges::any_of(options,
                                   [id](const rules::CreationChoice &c)
                                   {
                                       return c.id == id;
                                   });
    };
    check(offered("druid:entangle") && !offered("magic_user:magic_missile"),
          "It may memorize the druid's spells");
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
    // A member turned to stone stays a statue outside fights: no rest heals
    // it, no cure of wounds or camp spell helps it, and Stone to Flesh brings
    // it back okay at 1 Hit Point (temple.pas, StoneToFlesh).
    rules::VitalState statue{0, false, {}, std::string(original::stoned_description)};
    module->recover(statue, sheet);
    check(statue.hit_points == 0, "A statue heals nothing as the party rests");
    check(!module->temple_service_helps(sheet, statue, rules::TempleService::heal) &&
              !module->temple_service_helps(sheet, statue, rules::TempleService::raise_dead) &&
              module->temple_service_helps(sheet, statue, rules::TempleService::stone_to_flesh) &&
              module->camp_actions(sheet, statue).empty(),
          "Only Stone to Flesh helps a statue");
    check(!module->temple_service_helps(sheet, hurt, rules::TempleService::stone_to_flesh),
          "Stone to Flesh helps no one else");
    module->temple_service(statue, sheet, rules::TempleService::stone_to_flesh, random);
    check(statue.hit_points == 1 && statue.description.empty(),
          "Stone to Flesh brings a statue back with 1 Hit Point");
}

// A character of a game's original rules, by its class and alignment.
rules::CharacterSheet original_character(GoldBoxGame game, std::string profession,
                                         std::string alignment, std::uint64_t seed)
{
    const auto creation = original::character_rules(original::default_aging, game);
    RandomState random{seed};
    rules::CharacterDraft draft;
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = std::move(profession);
    draft.alignment = std::move(alignment);
    draft.name = "Aldo";
    creation->roll_in_place(draft, random);
    return creation->evaluate(draft, rules::NameRequirement::required);
}

// A member's state as a fight left it, with these effects.
rules::VitalState left_with(const rules::CharacterSheet &sheet,
                            std::vector<original::Effect> effects, bool dead = false)
{
    original::Combatant numbers;
    numbers.effects = std::move(effects);
    return {dead ? 0 : sheet.hit_points, dead, numbers.encode(), {}};
}

bool carries(const rules::CharacterSheet &sheet, const rules::VitalState &state, int code)
{
    const auto carried =
        original::carried_state(state.resources, original::game_of(sheet.identity));
    return std::ranges::any_of(carried.conditions,
                               [code](const original::Effect &e)
                               {
                                   return e.code == code;
                               });
}

// The conditions that outlast a fight (effhndlr.pas; temple.pas): blindness,
// disease, poison and a curse stay through a day's rest, a disease taking
// hold as its time comes, while a fight's other effects end; the temple and
// the camp's spells cure them.
void conditions_outlast_fights()
{
    const auto module = original::rules_module();
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto sheet = original_character(curse, "cleric", "lawful_good", 31);
    RandomState random{37};
    auto blinded = left_with(sheet, {{33, 0, 4, false}, {1, 6, 4, false}, {34, 30, 4, false}});
    module->recover(blinded, sheet);
    check(carries(sheet, blinded, 33) && !carries(sheet, blinded, 1) &&
              !carries(sheet, blinded, 34) && carries(sheet, blinded, 43) &&
              carries(sheet, blinded, 44),
          "Blindness lasts the night, Bless ends, and the disease takes hold");
    for (const auto service :
         {rules::TempleService::cure_blindness, rules::TempleService::cure_disease})
    {
        check(module->temple_service_helps(sheet, blinded, service),
              "The temple can cure what the member has");
        module->temple_service(blinded, sheet, service, random);
    }
    check(blinded.resources.empty() &&
              !module->temple_service_helps(sheet, blinded, rules::TempleService::cure_blindness),
          "Cured, it carries nothing");

    auto cursed = left_with(sheet, {{36, 0, 4, false}});
    check(module->temple_service_helps(sheet, cursed, rules::TempleService::remove_curse),
          "Remove Curse lifts a curse");
    module->temple_service(cursed, sheet, rules::TempleService::remove_curse, random);
    check(!carries(sheet, cursed, 36), "The curse is lifted");

    // Curse's temple neutralizes the poison of the living only; the poison
    // a member died of goes with its raising, which costs as ever.
    auto poisoned_dead = left_with(sheet, {{55, 0, 0, false}}, true);
    check(!module->temple_service_helps(sheet, poisoned_dead,
                                        rules::TempleService::neutralize_poison),
          "Curse's temple does not stand up the poisoned dead");
    module->temple_service(poisoned_dead, sheet, rules::TempleService::raise_dead, random);
    check(!poisoned_dead.dead && !carries(sheet, poisoned_dead, 55) &&
              module->revival_changes(sheet, rules::Revival::temple).size() == 1,
          "Raise Dead takes the poison away");

    // Camp: Cure Blindness and Neutralize Poison (Curse's camp spells), the
    // second standing the poisoned dead up at no cost.
    auto priest = sheet;
    priest.prepared_spells = {"cleric:cure_blindness", "cleric:neutralize_poison"};
    rules::VitalState own{priest.hit_points, false, {}, {}};
    auto blind = left_with(sheet, {{33, 0, 4, false}});
    module->use_camp_action(priest, own, sheet, blind, "cast:cleric:cure_blindness", random);
    check(!carries(sheet, blind, 33), "Cure Blindness in camp");
    auto dead = left_with(sheet, {{55, 0, 0, false}}, true);
    module->use_camp_action(priest, own, sheet, dead, "cast:cleric:neutralize_poison", random);
    auto raised = sheet;
    const auto cost = module->revival_changes(raised, rules::Revival::spell);
    check(!dead.dead && dead.hit_points >= 1 && cost.size() == 1 &&
              module->advance_character(raised, dead, cost.front()) &&
              raised.scores[rules::Ability::constitution] ==
                  sheet.scores[rules::Ability::constitution],
          "Neutralize Poison stands the poisoned dead up, at no cost");
}

// Secret's and Gateway's temples stand up one poison killed (their
// NeutralizePoison), and Gateway's Raise Dead raises one at no cost.
void later_temples_stand_up_the_poisoned()
{
    const auto module = original::rules_module();
    RandomState random{41};
    for (const auto game :
         {GoldBoxGame::secret_of_the_silver_blades, GoldBoxGame::gateway_to_the_savage_frontier})
    {
        const auto sheet = original_character(game, "fighter", "neutral_good", 43);
        auto dead = left_with(sheet, {{55, 0, 0, false}}, true);
        check(module->temple_service_helps(sheet, dead, rules::TempleService::neutralize_poison),
              "The temple can stand up the poisoned dead");
        module->temple_service(dead, sheet, rules::TempleService::neutralize_poison, random);
        auto raised = sheet;
        const auto cost = module->revival_changes(raised, rules::Revival::temple);
        check(!dead.dead && module->advance_character(raised, dead, cost.front()) &&
                  raised.scores[rules::Ability::constitution] ==
                      sheet.scores[rules::Ability::constitution] &&
                  raised.hit_points == sheet.hit_points,
              "Neutralize Poison stands it up, at no cost");
    }
    const auto gateway = GoldBoxGame::gateway_to_the_savage_frontier;
    const auto sheet = original_character(gateway, "fighter", "neutral_good", 47);
    auto dead = left_with(sheet, {{55, 0, 0, false}}, true);
    module->temple_service(dead, sheet, rules::TempleService::raise_dead, random);
    auto raised = sheet;
    const auto cost = module->revival_changes(raised, rules::Revival::temple);
    check(module->advance_character(raised, dead, cost.front()) &&
              raised.scores[rules::Ability::constitution] ==
                  sheet.scores[rules::Ability::constitution],
          "Gateway raises one poison killed at no cost");
}

// A paladin cures disease once a week, and once more for each five levels
// past the first (Curse's PaladinCureDisease and its week-long effect 141).
void a_paladin_cures_disease()
{
    const auto module = original::rules_module();
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto paladin = original_character(curse, "paladin", "lawful_good", 53);
    const auto offers_cure = [&](const rules::VitalState &state)
    {
        return std::ranges::any_of(module->camp_actions(paladin, state),
                                   [](const rules::CampAction &a)
                                   {
                                       return a.id == "paladin:cure_disease";
                                   });
    };
    RandomState random{59};
    rules::VitalState own{paladin.hit_points, false, {}, {}};
    check(paladin.original->class_levels[3] <= 5 && offers_cure(own), "A paladin may cure disease");
    auto sick = left_with(paladin, {{43, 0, 1, false}, {44, 0, 1, false}});
    module->use_camp_action(paladin, own, paladin, sick, "paladin:cure_disease", random);
    check(!carries(paladin, sick, 43) && !carries(paladin, sick, 44) && !offers_cure(own),
          "It cures, once this week");
    for (int day = 0; day < 6; ++day)
    {
        module->recover(own, paladin);
    }
    check(!offers_cure(own), "Not again within the week");
    module->recover(own, paladin);
    check(offers_cure(own), "Again the next week");
}

// A fight starts with the conditions its members carry (the original's
// effects stay on a character between fights).
void a_fight_starts_with_lasting_conditions()
{
    const auto module = original::rules_module();
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto sheet = original_character(curse, "fighter", "lawful_good", 61);
    auto state = left_with(sheet, {{33, 0, 4, false}});
    module->recover(state, sheet);
    rules::Encounter encounter;
    encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
    rules::Participant member;
    member.id = 1;
    member.name = "Aldo";
    member.definition = "member";
    member.side = rules::Side::party;
    member.cell = {1, 1};
    member.character_profile = module->character_profile(sheet, {}).data;
    member.state = state;
    original::Combatant kobold;
    kobold.hit_points = kobold.max_hit_points = 5;
    rules::Participant foe;
    foe.id = 2;
    foe.name = "Kobold";
    foe.definition = "monster";
    foe.side = rules::Side::opposition;
    foe.cell = {6, 6};
    foe.character_profile = kobold.encode();
    encounter.participants = {member, foe};
    const auto session = original::rules_module({{}, {}, curse})->create(std::move(encounter), 67);
    const auto snapshot = session->snapshot();
    const auto &aldo =
        *std::ranges::find(snapshot.combatants, std::string("Aldo"), &rules::CombatantView::name);
    check(original::carried_state(aldo.persistent.resources, curse).conditions.size() == 1,
          "The fighter starts its fight still blind");
}

// The first combatant of the party a fight starts from this state.
original::Combatant fought_from(const rules::RulesModule &module,
                                const rules::CharacterSheet &sheet, const rules::VitalState &state,
                                GoldBoxGame game)
{
    rules::Encounter encounter;
    encounter.battlefield = {8, 8, std::vector<rules::Terrain>(64, rules::Terrain::open)};
    rules::Participant member;
    member.id = 1;
    member.name = "Aldo";
    member.definition = "member";
    member.side = rules::Side::party;
    member.cell = {1, 1};
    member.character_profile = module.character_profile(sheet, {}).data;
    member.state = state;
    original::Combatant kobold;
    rules::Participant foe;
    foe.id = 2;
    foe.name = "Kobold";
    foe.definition = "monster";
    foe.side = rules::Side::opposition;
    foe.cell = {6, 6};
    foe.character_profile = kobold.encode();
    encounter.participants = {member, foe};
    const auto session = original::rules_module({{}, {}, game})->create(std::move(encounter), 71);
    const auto snapshot = session->snapshot();
    const auto &aldo =
        *std::ranges::find(snapshot.combatants, std::string("Aldo"), &rules::CombatantView::name);
    return *original::Combatant::decode(aldo.persistent.resources);
}

// The Strength spells cast in camp last into the next fight (magic.pas);
// a disease that has taken hold wastes Strength and Hit Points through a
// day's rest and leaves its member helpless (effhndlr.pas, 43 and 44), until
// its cure gives the Strength back; Slow Poison stands one poison killed up
// for a while, and the poison kills again when it runs out.
void camp_spells_last_and_diseases_waste()
{
    const auto module = original::rules_module();
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    auto mage = original_character(curse, "magic_user", "neutral_good", 73);
    mage.prepared_spells = {"magic_user:enlarge"};
    const auto fighter = original_character(curse, "fighter", "neutral_good", 79);
    RandomState random{83};
    rules::VitalState own{mage.hit_points, false, {}, {}};
    rules::VitalState fresh{fighter.hit_points, false, {}, {}};
    module->use_camp_action(mage, own, fighter, fresh, "cast:magic_user:enlarge", random);
    const auto enlarged = fought_from(*module, fighter, fresh, curse);
    const auto own_strength = fighter.scores[rules::Ability::strength];
    const auto [base, exceptional] =
        original::enlarged_strength(mage.original->class_levels[5], curse);
    const bool gains = original::stronger_than(base, exceptional, own_strength,
                                               fighter.original->exceptional_strength);
    check(carries(fighter, fresh, 12) &&
              (gains ? enlarged.strength == base && enlarged.exceptional_strength == exceptional
                     : enlarged.strength == own_strength),
          "Enlarge cast in camp makes the next fight's Strength");

    auto sick = left_with(fighter, {{43, 0, 1, false}, {44, 0, 1, false}});
    module->recover(sick, fighter);
    check(sick.hit_points == 1 && carries(fighter, sick, 31) &&
              carries(fighter, sick, original::strength_wasted),
          "A day's disease wastes Hit Points and Strength and leaves one helpless");
    const auto weak = fought_from(*module, fighter, sick, curse);
    check(weak.strength == std::max(3, own_strength - 24), "The next fight starts weak");
    module->temple_service(sick, fighter, rules::TempleService::cure_disease, random);
    check(sick.resources.empty() &&
              fought_from(*module, fighter, sick, curse).strength == own_strength,
          "Cured, the Strength comes back");

    auto priest = original_character(curse, "cleric", "lawful_good", 89);
    priest.prepared_spells = {"cleric:slow_poison"};
    rules::VitalState priest_state{priest.hit_points, false, {}, {}};
    auto dead = left_with(fighter, {{55, 0, 0, false}}, true);
    module->use_camp_action(priest, priest_state, fighter, dead, "cast:cleric:slow_poison", random);
    check(!dead.dead && dead.hit_points >= 1, "Slow Poison stands the poisoned up for a while");
    module->recover(dead, fighter);
    check(dead.dead, "The poison kills again when the spell runs out");
}

// Spells cast in camp for their effects carry into the next fight
// (camp.pas, NoncombatCastMenu): Bless on the whole party, Protection from
// Evil on one member; Blink only where the game's table lets it be cast out
// of a fight (Gateway's).
void camp_effects_carry_into_fights()
{
    const auto module = original::rules_module();
    const auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    auto priest = original_character(curse, "cleric", "lawful_good", 101);
    priest.prepared_spells = {"cleric:bless", "cleric:protection_from_evil"};
    const auto fighter = original_character(curse, "fighter", "lawful_good", 103);
    RandomState random{107};
    rules::VitalState own{priest.hit_points, false, {}, {}};
    rules::VitalState guard{fighter.hit_points, false, {}, {}};
    const auto actions = module->camp_actions(priest, own);
    const auto bless =
        std::ranges::find(actions, std::string("cast:cleric:bless"), &rules::CampAction::id);
    check(bless != actions.end() && bless->whole_party, "Bless is cast on the party in camp");
    std::vector<rules::CampTarget> party{{priest, own}, {fighter, guard}};
    module->use_party_camp_action(priest, own, party, "cast:cleric:bless", random);
    module->use_camp_action(priest, own, fighter, guard, "cast:cleric:protection_from_evil",
                            random);
    const auto blessed = fought_from(*module, fighter, guard, curse);
    const auto has = [&blessed](int code)
    {
        return std::ranges::any_of(blessed.effects,
                                   [code](const original::Effect &e)
                                   {
                                       return e.code == code;
                                   });
    };
    check(has(1) && has(8) && carries(priest, own, 1),
          "Bless and Protection from Evil cast in camp start the next fight");

    auto mage = original_character(curse, "magic_user", "neutral_good", 109);
    mage.prepared_spells = {"magic_user:blink"};
    rules::VitalState mage_state{mage.hit_points, false, {}, {}};
    const auto offers_blink = [&](const rules::CharacterSheet &sheet)
    {
        return std::ranges::any_of(module->camp_actions(sheet, mage_state),
                                   [](const rules::CampAction &a)
                                   {
                                       return a.id == "cast:magic_user:blink";
                                   });
    };
    auto gateway_mage = original_character(GoldBoxGame::gateway_to_the_savage_frontier,
                                           "magic_user", "neutral_good", 113);
    gateway_mage.prepared_spells = {"magic_user:blink"};
    check(!offers_blink(mage) && offers_blink(gateway_mage),
          "Blink is cast in camp only in Gateway");
}

// The races' innate effects as each game gives them (chargen.pas): Secret's
// gnome has no defense against giants (47) and no race the Constitution save
// against poison (90); its arrows and quarrels are item types 30 and 12, and
// an elf's bows and swords 18, 19 and 22 to 26.
void secret_races_and_items()
{
    const auto module = original::rules_module();
    const auto effects_of = [&](GoldBoxGame game, std::string race)
    {
        const auto creation = original::character_rules(original::default_aging, game);
        RandomState random{97};
        rules::CharacterDraft draft;
        draft.race = std::move(race);
        draft.gender = "male";
        draft.character_class = "fighter";
        draft.alignment = "neutral_good";
        draft.name = "Bolo";
        creation->roll_in_place(draft, random);
        const auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
        std::vector<int> codes;
        const auto numbers =
            *original::Combatant::decode(module->character_profile(sheet, {}).data);
        for (const auto &e : numbers.effects)
        {
            codes.push_back(e.code);
        }
        return codes;
    };
    const auto has = [](const std::vector<int> &codes, int code)
    {
        return std::ranges::find(codes, code) != codes.end();
    };
    const auto curse_gnome = effects_of(GoldBoxGame::curse_of_the_azure_bonds, "gnome");
    const auto secret_gnome = effects_of(GoldBoxGame::secret_of_the_silver_blades, "gnome");
    const auto secret_dwarf = effects_of(GoldBoxGame::secret_of_the_silver_blades, "dwarf");
    check(has(curse_gnome, 47) && !has(secret_gnome, 47) && has(secret_gnome, 48) &&
              has(secret_dwarf, 47) && !has(secret_dwarf, 90) && has(secret_dwarf, 97),
          "Secret's gnome has no defense against giants, and no race the poison save");
    const auto secret = original::ammunition_types(GoldBoxGame::secret_of_the_silver_blades);
    const auto pool = original::ammunition_types(GoldBoxGame::pool_of_radiance);
    check(secret.arrows == 30 && secret.quarrels == 12 && pool.arrows == 73 &&
              pool.quarrels == 28 &&
              original::elf_weapon(22, GoldBoxGame::secret_of_the_silver_blades) &&
              !original::elf_weapon(22, GoldBoxGame::pool_of_radiance) &&
              original::elf_weapon(41, GoldBoxGame::curse_of_the_azure_bonds),
          "Secret numbers its arrows, quarrels and an elf's weapons its own way");
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
        conditions_outlast_fights();
        later_temples_stand_up_the_poisoned();
        a_paladin_cures_disease();
        a_fight_starts_with_lasting_conditions();
        camp_spells_last_and_diseases_waste();
        secret_races_and_items();
        camp_effects_carry_into_fights();
        raise_dead_costs_constitution();
        scripts_harm_as_the_original();
        a_potion_heals_outside_a_fight();
        an_npc_joins_as_its_record();
        a_drain_lasts_beyond_the_fight();
        curse_clerics_heal_and_raise_in_camp();
        secret_clerics_heal_in_camp();
        a_pool_character_comes_to_curse();
        curse_paladins_and_rangers();
        secret_rangers_memorize_their_spells();
        secret_characters_start_trained();
        gateway_characters_start_low_and_stop_at_caps();
        darkness_characters_start_high();
        treasures_characters_start_with_their_experience();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
