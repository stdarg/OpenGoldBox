// The original rules' tables and creation (docs/ORIGINAL-RULES.md).
#include "opengold/original_rules.h"
#include "test_support.h"
#include <iostream>
#include <string>

using namespace opengold::original;
using opengold::RandomState;
using opengold::rules::Ability;

namespace
{
using opengold::test::check;
using opengold::test::rejects;

constexpr std::array<Race, 7> every_race{Race::dwarf,    Race::elf,      Race::gnome,
                                         Race::half_elf, Race::halfling, Race::half_orc,
                                         Race::human};
constexpr std::array<Gender, 2> both_genders{Gender::male, Gender::female};
constexpr std::array<Ability, 6> every_ability{Ability::strength,     Ability::dexterity,
                                               Ability::constitution, Ability::intelligence,
                                               Ability::wisdom,       Ability::charisma};

std::string named(Race race, Gender gender, Profession profession)
{
    return "race " + std::to_string(static_cast<unsigned>(race)) + ", gender " +
           std::to_string(static_cast<unsigned>(gender)) + ", profession " +
           std::to_string(static_cast<unsigned>(profession));
}

// Every class a race is offered can be rolled: no class minimum lies above the
// race's maximum, which would leave a score outside its limits.
void every_offered_profession_fits_its_race()
{
    for (const auto race : every_race)
    {
        for (const auto gender : both_genders)
        {
            const auto limits = ability_limits(race, gender);
            for (const auto profession : professions_for(race))
            {
                for (const auto ability : every_ability)
                {
                    check(class_minimums(profession)[ability] <= limits.maximum[ability],
                          "A class minimum is above the race's maximum: " +
                              named(race, gender, profession));
                }
            }
        }
    }
}

int lowest_first_level_hit_points(Profession profession, int constitution)
{
    // Two thirds of each class's die, as the original guarantees at level 1.
    int total = 0;
    for (const auto slot : classes_of(profession))
    {
        total += slot == ClassSlot::magic_user ? 2
                 : slot == ClassSlot::thief    ? 4
                 : slot == ClassSlot::fighter  ? 6
                                               : 5;
    }
    const int bonus = constitution_hit_point_bonus(profession, constitution);
    const int classes = static_cast<int>(classes_of(profession).size());
    return bonus < 0 && total <= -bonus + classes ? 1 : (total + bonus) / classes;
}

int highest_first_level_hit_points(Profession profession, int constitution)
{
    int total = 0;
    for (const auto slot : classes_of(profession))
    {
        total += slot == ClassSlot::magic_user ? 4
                 : slot == ClassSlot::thief    ? 6
                 : slot == ClassSlot::fighter  ? 10
                                               : 8;
    }
    const int bonus = constitution_hit_point_bonus(profession, constitution);
    const int classes = static_cast<int>(classes_of(profession).size());
    return bonus < 0 && total <= -bonus + classes ? 1 : (total + bonus) / classes;
}

void rolled_characters_keep_the_limits()
{
    RandomState random{20261112};
    for (const auto race : every_race)
    {
        for (const auto gender : both_genders)
        {
            const auto limits = ability_limits(race, gender);
            for (const auto profession : professions_for(race))
            {
                for (int n = 0; n < 300; ++n)
                {
                    const auto rolled = roll_character(race, gender, profession, random);
                    const auto who = named(race, gender, profession);
                    for (const auto ability : every_ability)
                    {
                        const int score = rolled.scores[ability];
                        check(score >= limits.minimum[ability] &&
                                  score <= limits.maximum[ability] &&
                                  score >= class_minimums(profession)[ability],
                              "A rolled score leaves its limits: " + who);
                    }
                    const bool fighter = level_in(first_levels(profession), ClassSlot::fighter) > 0;
                    const bool eighteen = rolled.scores[Ability::strength] == 18;
                    check(rolled.exceptional_strength <= limits.exceptional_strength_maximum &&
                              (rolled.exceptional_strength > 0) ==
                                  (fighter && eighteen && limits.exceptional_strength_maximum > 0),
                          "Exceptional Strength is wrong: " + who);
                    const int constitution = rolled.scores[Ability::constitution];
                    check(rolled.hit_points >=
                                  lowest_first_level_hit_points(profession, constitution) &&
                              rolled.hit_points <=
                                  highest_first_level_hit_points(profession, constitution),
                          "First-level Hit Points leave their range: " + who);
                    check(rolled.gold >= 20 && rolled.gold <= 200,
                          "Starting gold leaves its range: " + who);
                    check(rolled.age > 0, "A character has no age: " + who);
                }
            }
        }
    }
}

// The original's creation never applied racial adjustments; OpenGoldBox does,
// so a dwarf can roll a Constitution of 19 and an elf a Dexterity of 19, which
// no 3d6 roll reaches alone.
void racial_adjustments_apply()
{
    check(racial_adjustments(Race::dwarf)[Ability::constitution] == 1 &&
              racial_adjustments(Race::dwarf)[Ability::charisma] == -1,
          "A dwarf has +1 Constitution and -1 Charisma");
    check(racial_adjustments(Race::half_orc)[Ability::charisma] == -2,
          "A half-orc has -2 Charisma");
    check(racial_adjustments(Race::human) == opengold::rules::AbilityArray<int>{},
          "A human has no racial adjustment");
    RandomState random{7};
    bool dwarf_nineteen = false;
    bool elf_nineteen = false;
    for (int n = 0; n < 2000; ++n)
    {
        dwarf_nineteen |= roll_character(Race::dwarf, Gender::male, Profession::fighter, random)
                              .scores[Ability::constitution] == 19;
        elf_nineteen |= roll_character(Race::elf, Gender::male, Profession::fighter, random)
                            .scores[Ability::dexterity] == 19;
    }
    check(dwarf_nineteen && elf_nineteen,
          "Racial adjustments raise a dwarf's Constitution and an elf's Dexterity to 19");
}

void rolls_repeat_from_the_same_state()
{
    RandomState first{99};
    RandomState second{99};
    const auto a = roll_character(Race::half_elf, Gender::female,
                                  Profession::cleric_fighter_magic_user, first);
    const auto b = roll_character(Race::half_elf, Gender::female,
                                  Profession::cleric_fighter_magic_user, second);
    check(a.scores == b.scores && a.hit_points == b.hit_points && a.gold == b.gold &&
              a.age == b.age && first == second,
          "The same dice state rolls the same character");
    check(a.scores[Ability::wisdom] >= 13, "A cleric multi-class has at least 13 Wisdom");
}

void thac0_is_the_best_class()
{
    ClassLevels levels{};
    check(thac0(levels) == 20, "No levels hit AC 0 on a 20");
    level_in(levels, ClassSlot::fighter) = 1;
    check(thac0(levels) == 20, "A level 1 fighter has THAC0 20");
    level_in(levels, ClassSlot::fighter) = 2;
    check(thac0(levels) == 19, "A level 2 fighter has THAC0 19");
    level_in(levels, ClassSlot::fighter) = 7;
    check(thac0(levels) == 14, "A level 7 fighter has THAC0 14");
    ClassLevels cleric{};
    level_in(cleric, ClassSlot::cleric) = 4;
    check(thac0(cleric) == 18, "A level 4 cleric has THAC0 18");
    ClassLevels mage{};
    level_in(mage, ClassSlot::magic_user) = 6;
    check(thac0(mage) == 19, "A level 6 magic-user has THAC0 19");
    ClassLevels multi{};
    level_in(multi, ClassSlot::fighter) = 3;
    level_in(multi, ClassSlot::magic_user) = 2;
    check(thac0(multi) == 18, "A fighter/magic-user uses the fighter's THAC0");
}

void saving_throws_are_the_best_class()
{
    const auto fighter = saving_throws(first_levels(Profession::fighter));
    check(fighter == std::array<int, 5>{14, 15, 16, 17, 17}, "A level 1 fighter's saves");
    check(saving_throws(first_levels(Profession::cleric_fighter)) ==
              std::array<int, 5>{10, 13, 14, 16, 15},
          "A cleric/fighter saves as a cleric throughout");
    check(saving_throws(first_levels(Profession::magic_user_thief)) ==
              std::array<int, 5>{13, 12, 11, 15, 12},
          "A magic-user/thief takes each save from the better class");
    ClassLevels mage{};
    level_in(mage, ClassSlot::magic_user) = 6;
    check(saving_throw(saving_throws(mage), Save::spell) == 10,
          "A level 6 magic-user saves against spells on 10");
    ClassLevels fighter9{};
    level_in(fighter9, ClassSlot::fighter) = 9;
    check(saving_throw(saving_throws(fighter9), Save::breath_weapon) == 9,
          "A level 9 fighter saves against breath on 9");
    check(saving_throws(ClassLevels{}) == std::array<int, 5>{20, 20, 20, 20, 20},
          "No class saves only on a 20");
}

void experience_and_level_limits()
{
    check(experience_for_level(ClassSlot::fighter, 1) == 0, "Level 1 needs no experience");
    check(experience_for_level(ClassSlot::fighter, 2) == 2001, "A fighter reaches level 2 at 2001");
    check(experience_for_level(ClassSlot::fighter, 8) == 125001,
          "A fighter reaches level 8 at 125001");
    check(!experience_for_level(ClassSlot::fighter, 9), "A fighter stops at level 8");
    check(experience_for_level(ClassSlot::thief, 9) == 110001, "A thief reaches level 9");
    check(!experience_for_level(ClassSlot::magic_user, 7), "A magic-user stops at level 6");
    check(experience_for_level(ClassSlot::cleric, 6) == 27501, "A cleric reaches level 6 at 27501");
    check(racial_level_limit(Race::elf, ClassSlot::fighter, 16) == 5 &&
              racial_level_limit(Race::elf, ClassSlot::fighter, 17) == 6 &&
              racial_level_limit(Race::elf, ClassSlot::fighter, 18) == 7,
          "An elf fighter's limit follows Strength");
    check(!racial_level_limit(Race::dwarf, ClassSlot::fighter, 18) &&
              racial_level_limit(Race::dwarf, ClassSlot::fighter, 17) == 8,
          "A dwarf fighter of 18 Strength has no racial limit");
    check(racial_level_limit(Race::half_elf, ClassSlot::cleric, 10) == 5,
          "A half-elf cleric stops at level 5");
    check(!racial_level_limit(Race::human, ClassSlot::fighter, 3), "A human has no racial limit");
}

void spell_slots_and_wisdom()
{
    check(spell_slots(ClassSlot::cleric, 1, 12) == std::array<int, 3>{1, 0, 0},
          "A level 1 cleric has one slot");
    check(spell_slots(ClassSlot::cleric, 1, 14) == std::array<int, 3>{3, 0, 0},
          "Wisdom 14 adds two first-level slots");
    check(spell_slots(ClassSlot::cleric, 3, 15) == std::array<int, 3>{4, 2, 0},
          "Wisdom 15 adds a second-level slot at level 3");
    check(spell_slots(ClassSlot::cleric, 5, 17) == std::array<int, 3>{5, 5, 2},
          "Wisdom 17 adds slots at each level a level 5 cleric casts");
    check(spell_slots(ClassSlot::magic_user, 4, 18) == std::array<int, 3>{3, 2, 0},
          "Wisdom does nothing for a magic-user");
    check(spell_slots(ClassSlot::fighter, 5, 18) == std::array<int, 3>{},
          "A fighter has no spell slots");
}

void dexterity_bonuses()
{
    struct Expected
    {
        int dexterity, armor_class, missiles;
    };

    // The original's values up to 18.
    constexpr std::array<Expected, 12> original{{{3, -4, -3},
                                                 {4, -3, -2},
                                                 {5, -2, -1},
                                                 {6, -1, 0},
                                                 {7, 0, 0},
                                                 {14, 0, 0},
                                                 {15, 1, 0},
                                                 {16, 2, 1},
                                                 {17, 3, 2},
                                                 {18, 4, 3},
                                                 {2, -4, -4},
                                                 {1, -4, -4}}};
    for (const auto &expected : original)
    {
        check(dexterity_armor_class_bonus(expected.dexterity) == expected.armor_class &&
                  dexterity_missile_bonus(expected.dexterity) == expected.missiles,
              "Dexterity " + std::to_string(expected.dexterity) + " has the original's bonuses");
    }
    check(dexterity_armor_class_bonus(19) > dexterity_armor_class_bonus(18) &&
              dexterity_missile_bonus(19) > dexterity_missile_bonus(18),
          "A natural 19 is worth more than 18 in a fight");
    check(dexterity_armor_class_bonus(21) == 5 && dexterity_armor_class_bonus(22) == 6 &&
              dexterity_missile_bonus(21) == 4 && dexterity_missile_bonus(25) == 5,
          "Each step above 18 comes two points sooner than in the original");
}

void constitution_and_age()
{
    check(constitution_hit_point_bonus(Profession::fighter, 18) == 4,
          "A fighter of 18 Constitution gains 4 a level");
    check(constitution_hit_point_bonus(Profession::fighter_magic_user, 16) == 4,
          "A fighter/magic-user's bonus counts each class before the division");
    check(constitution_hit_point_bonus(Profession::cleric, 3) == -2,
          "Constitution 3 costs 2 a level");
    RandomState random{3};
    for (int n = 0; n < 200; ++n)
    {
        const int human_fighter = starting_age(Race::human, Profession::fighter, random);
        check(human_fighter >= 16 && human_fighter <= 19, "A human fighter starts at 16 to 19");
    }
    check(starting_age(Race::elf, Profession::fighter_magic_user, random) == 180,
          "A fighter/magic-user elf takes the magic-user's oldest starting age");
    rejects(
        [&]
        {
            (void)starting_age(Race::human, Profession::monk, random);
        });
}
} // namespace

int main()
{
    try
    {
        every_offered_profession_fits_its_race();
        rolled_characters_keep_the_limits();
        racial_adjustments_apply();
        rolls_repeat_from_the_same_state();
        thac0_is_the_best_class();
        saving_throws_are_the_best_class();
        experience_and_level_limits();
        spell_slots_and_wisdom();
        dexterity_bonuses();
        constitution_and_age();
        std::cout << "Original rules passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
