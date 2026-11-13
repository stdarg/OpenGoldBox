#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/original_combat.h"
#include "opengold/original.h"
#include "test_support.h"
#include <iostream>

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

} // namespace

int main()
{
    try
    {
        the_original_pages_in_their_order();
        a_fighter_trains_and_keeps_the_level();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
