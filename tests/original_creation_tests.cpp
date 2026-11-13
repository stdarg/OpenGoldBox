#include "opengold/character_creator.h"
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
} // namespace

int main()
{
    try
    {
        the_original_pages_in_their_order();
        std::cout << "Original creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
