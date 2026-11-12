// The original rules' tables and creation (docs/ORIGINAL-RULES.md).
#include "opengold/original.h"
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

constexpr std::array<Aging, 3> every_aging{Aging::off, Aging::original, Aging::add};

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
                    const auto rolled = roll_character(race, profession, random);
                    const auto who = named(race, gender, profession);
                    check(rolled.gold >= 20 && rolled.gold <= 200,
                          "Starting gold leaves its range: " + who);
                    check(rolled.starting_age > 0, "A character has no age: " + who);
                    for (const auto aging : every_aging)
                    {
                        const auto scores = ability_scores(race, gender, profession, rolled, aging,
                                                           rolled.starting_age);
                        for (const auto ability : every_ability)
                        {
                            check(scores[ability] >= limits.minimum[ability] &&
                                      scores[ability] <= limits.maximum[ability] &&
                                      scores[ability] >= class_minimums(profession)[ability],
                                  "A rolled score leaves its limits: " + who);
                        }
                        const bool fighter =
                            level_in(first_levels(profession), ClassSlot::fighter) > 0;
                        const int strength = scores[Ability::strength];
                        const int exceptional =
                            exceptional_strength(race, gender, profession, rolled, strength);
                        check(exceptional <= limits.exceptional_strength_maximum &&
                                  (exceptional > 0) == (fighter && strength == 18 &&
                                                        limits.exceptional_strength_maximum > 0),
                              "Exceptional Strength is wrong: " + who);
                        const int constitution = scores[Ability::constitution];
                        const int hit_points =
                            first_level_hit_points(profession, rolled, constitution);
                        check(hit_points >=
                                      lowest_first_level_hit_points(profession, constitution) &&
                                  hit_points <=
                                      highest_first_level_hit_points(profession, constitution),
                              "First-level Hit Points leave their range: " + who);
                    }
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
    const auto score = [&random](Race race, Ability ability)
    {
        const auto rolled = roll_character(race, Profession::fighter, random);
        return ability_scores(race, Gender::male, Profession::fighter, rolled, Aging::original,
                              rolled.starting_age)[ability];
    };
    bool dwarf_nineteen = false;
    bool elf_nineteen = false;
    for (int n = 0; n < 2000; ++n)
    {
        dwarf_nineteen |= score(Race::dwarf, Ability::constitution) == 19;
        elf_nineteen |= score(Race::elf, Ability::dexterity) == 19;
    }
    check(dwarf_nineteen && elf_nineteen,
          "Racial adjustments raise a dwarf's Constitution and an elf's Dexterity to 19");
}

void rolls_repeat_from_the_same_state()
{
    RandomState first{99};
    RandomState second{99};
    const auto profession = Profession::cleric_fighter_magic_user;
    const auto a = roll_character(Race::half_elf, profession, first);
    const auto b = roll_character(Race::half_elf, profession, second);
    check(a.rolls == b.rolls && a.first_level_hit_dice == b.first_level_hit_dice &&
              a.gold == b.gold && a.starting_age == b.starting_age && first == second,
          "The same dice state rolls the same character");
    check(ability_scores(Race::half_elf, Gender::female, profession, a, Aging::original,
                         a.starting_age)[Ability::wisdom] >= 13,
          "A cleric multi-class has at least 13 Wisdom");
}

void aging()
{
    check(age_category(Race::human, 18) == AgeCategory::young_adult &&
              age_category(Race::human, 21) == AgeCategory::mature &&
              age_category(Race::human, 95) == AgeCategory::venerable,
          "A human's age groups");
    using opengold::rules::AbilityArray;
    // Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma.
    check(age_changes(Aging::off, Race::human, 95) == AbilityArray<int>{},
          "Without aging, age changes nothing");
    check(age_changes(Aging::original, Race::human, 18) == AbilityArray<int>{0, 0, 0, 0, -1, 0},
          "The original's young adult loses 1 Wisdom");
    check(age_changes(Aging::add, Race::human, 18) == AbilityArray<int>{0, 0, 1, 0, -1, 0},
          "AD&D's young adult also gains 1 Constitution");
    check(age_changes(Aging::original, Race::human, 30) == AbilityArray<int>{1, 0, 0, 0, 0, 0},
          "A mature character gains 1 Strength and loses the young adult's Wisdom");
    check(age_changes(Aging::original, Race::human, 95) == AbilityArray<int>{-3, -3, -3, 2, 3, 0},
          "A venerable character's changes add up");
    check(age_changes(Aging::add, Race::human, 95) == AbilityArray<int>{-3, -3, -2, 2, 3, 0},
          "AD&D keeps the young adult's Constitution into old age");

    RolledCharacter rolled;
    rolled.rolls = {12, 12, 12, 12, 12, 12};
    rolled.starting_age = 18;
    const auto scores_at = [&rolled](Aging aging, int age)
    {
        return ability_scores(Race::human, Gender::male, Profession::fighter, rolled, aging, age);
    };
    check(scores_at(Aging::off, 95) == AbilityArray<int>{12, 12, 12, 12, 12, 12},
          "Without aging the rolls are the scores");
    check(scores_at(Aging::original, 18) == AbilityArray<int>{12, 12, 12, 12, 11, 12} &&
              scores_at(Aging::original, 95) == scores_at(Aging::original, 18),
          "The original changes the scores once, by the starting age");
    check(scores_at(Aging::add, 18) == AbilityArray<int>{12, 12, 13, 12, 11, 12},
          "AD&D aging adds the young adult's Constitution at creation");
    check(scores_at(Aging::add, 95) == AbilityArray<int>{9, 9, 10, 14, 15, 12},
          "AD&D aging changes the scores as the character grows older");
    rolled.rolls[Ability::strength] = 4;
    check(scores_at(Aging::add, 18)[Ability::strength] == 9 &&
              scores_at(Aging::add, 95)[Ability::strength] == 6,
          "Age takes a fighter's Strength below the class's minimum, which only creation keeps");
    check(ability_scores(Race::human, Gender::male, Profession::magic_user, rolled, Aging::add,
                         95)[Ability::strength] == 3,
          "Age never takes a score below 3");
}

void thief_skill_chances()
{
    const auto human = [](int level, int dexterity)
    {
        return thief_skills(Race::human, level, dexterity);
    };
    check(human(1, 13) == std::array<int, 8>{30, 25, 20, 15, 10, 10, 85, 0},
          "A level 1 human thief's chances");
    check(skill_chance(human(1, 10), ThiefSkill::pick_pockets) == 20,
          "Dexterity 10 costs 10 percent of Pick Pockets, not the original's 19");
    check(skill_chance(human(1, 16), ThiefSkill::open_locks) == 30,
          "Dexterity 16 adds 5 percent to Open Locks, not the original's -5");
    check(skill_chance(human(1, 9), ThiefSkill::move_silently) == 0,
          "A chance never falls below 0, where the original wrapped around");
    check(human(4, 5) == human(4, 9) && human(4, 23) == human(4, 19),
          "Dexterity beyond the original's table uses its ends");
    check(human(12, 13) == human(9, 13), "A thief above level 9 keeps level 9's chances");
    check(skill_chance(thief_skills(Race::dwarf, 1, 13), ThiefSkill::climb_walls) == 75,
          "A dwarf climbs 10 percent worse");
    check(skill_chance(thief_skills(Race::half_orc, 1, 13), ThiefSkill::read_languages) == 0,
          "A racial penalty above the chance leaves none");
    check(thief_skills(Race::human, 0, 18) == std::array<int, 8>{},
          "A character without thief levels has no chances");
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

opengold::rules::CharacterDraft draft(std::string race, std::string profession,
                                      std::string alignment)
{
    opengold::rules::CharacterDraft result;
    result.name = "Tester";
    result.race = std::move(race);
    result.gender = "male";
    result.character_class = std::move(profession);
    result.alignment = std::move(alignment);
    return result;
}

void creation_behind_the_rules_interface()
{
    using opengold::rules::CreationField;
    const auto rules = character_rules();
    check(rules->identity() == identity() && identity().module == "opengold.original",
          "The original rules name themselves");
    check(rules->ability_generation() == opengold::rules::AbilityGeneration::rolled_in_place,
          "The original rolls each ability in place");
    const auto races = rules->choices(CreationField::race);
    check(races.size() == 6 && std::ranges::none_of(races,
                                                    [](const auto &race)
                                                    {
                                                        return race.id == "half_orc";
                                                    }),
          "Creation offers the original's six races");
    check(rules->choices(CreationField::background).empty(), "The original has no backgrounds");
    auto dwarf = draft("dwarf", "fighter", "lawful_good");
    check(!rules->allows(dwarf, CreationField::character_class, "magic_user") &&
              rules->allows(dwarf, CreationField::character_class, "fighter_thief"),
          "A dwarf may not be a magic-user, but may be a fighter/thief");
    auto thief = draft("human", "thief", "lawful_good");
    check(!rules->allows(thief, CreationField::alignment, "lawful_good") &&
              rules->allows(thief, CreationField::alignment, "neutral"),
          "A thief is never lawful good");

    RandomState random{11};
    auto mage = draft("elf", "fighter_magic_user", "chaotic_good");
    check(!rules->ability_score(mage, Ability::strength), "No score before the roll");
    rules->roll_in_place(mage, random);
    const auto sheet = rules->evaluate(mage, opengold::rules::NameRequirement::required);
    check(sheet.character_class == "Fighter/Magic-User" && sheet.level == 1 &&
              sheet.level_classes == std::vector<std::string>{"fighter", "magic_user"},
          "A fighter/magic-user starts at level 1 in both classes");
    check(sheet.original && sheet.original->thac0 == 20 && sheet.hit_points >= 1 &&
              sheet.original->magic_user_slots == std::array<int, 3>{1, 0, 0},
          "The sheet carries the original's THAC0, Hit Points and spell slots");
    for (const auto ability : every_ability)
    {
        check(rules->ability_score(mage, ability) == sheet.scores[ability],
              "The draft's scores are the sheet's");
    }
    const auto rolled_again = mage.rules_data;
    rules->roll_in_place(mage, random);
    check(mage.rules_data != rolled_again, "Rolling again replaces the roll");

    // The game's aging rule changes the same rolls' scores.
    const auto youth = draft("human", "fighter", "neutral");
    auto rolled = youth;
    RandomState same{5};
    rules->roll_in_place(rolled, same);
    const auto original_con = rules->evaluate(rolled, opengold::rules::NameRequirement::required)
                                  .scores[Ability::constitution];
    const auto add_con = character_rules(Aging::add)
                             ->evaluate(rolled, opengold::rules::NameRequirement::required)
                             .scores[Ability::constitution];
    check(add_con == std::min(original_con + 1, 18),
          "AD&D aging gives a young adult 1 more Constitution");

    auto unnamed = rolled;
    unnamed.name.clear();
    rejects(
        [&]
        {
            (void)rules->evaluate(unnamed, opengold::rules::NameRequirement::required);
        });
    auto tampered = rolled;
    tampered.rules_data = "1 99 12";
    rejects(
        [&]
        {
            (void)rules->evaluate(tampered, opengold::rules::NameRequirement::required);
        });
    auto forbidden = draft("dwarf", "magic_user", "neutral");
    rejects(
        [&]
        {
            rules->roll_in_place(forbidden, random);
        });
}
} // namespace

// Combat's tables (docs/ORIGINAL-RULES.md, plan step 4), from the original's.
void combat_tables()
{
    check(strength_scale(17, 0) == 17 && strength_scale(18, 0) == 18 &&
              strength_scale(18, 50) == 19 && strength_scale(18, 51) == 20 &&
              strength_scale(18, 90) == 21 && strength_scale(18, 99) == 22 &&
              strength_scale(18, 100) == 23 && strength_scale(19, 0) == 24 &&
              strength_scale(25, 0) == 30,
          "Strength's scale");
    check(strength_to_hit_bonus(3, 0) == -3 && strength_to_hit_bonus(7, 0) == -1 &&
              strength_to_hit_bonus(16, 0) == 0 && strength_to_hit_bonus(17, 0) == 1 &&
              strength_to_hit_bonus(18, 50) == 1 && strength_to_hit_bonus(18, 51) == 2 &&
              strength_to_hit_bonus(18, 100) == 3 && strength_to_hit_bonus(21, 0) == 4 &&
              strength_to_hit_bonus(25, 0) == 7,
          "Strength's to-hit bonus");
    check(strength_damage_bonus(2, 0) == -2 && strength_damage_bonus(5, 0) == -1 &&
              strength_damage_bonus(15, 0) == 0 && strength_damage_bonus(16, 0) == 1 &&
              strength_damage_bonus(17, 0) == 1 && strength_damage_bonus(18, 0) == 2 &&
              strength_damage_bonus(18, 50) == 3 && strength_damage_bonus(18, 75) == 3 &&
              strength_damage_bonus(18, 90) == 4 && strength_damage_bonus(18, 100) == 6 &&
              strength_damage_bonus(19, 0) == 7 && strength_damage_bonus(25, 0) == 14,
          "Strength's damage bonus");
    check(weight_allowance(3, 0) == -350 && weight_allowance(10, 0) == 0 &&
              weight_allowance(16, 0) == 350 && weight_allowance(17, 0) == 500 &&
              weight_allowance(18, 0) == 750 && weight_allowance(18, 100) == 3000 &&
              weight_allowance(25, 0) == 15000,
          "Strength's weight allowance");
    check(encumbered_movement(12, 1012, 500) == 12 && encumbered_movement(12, 1013, 500) == 9 &&
              encumbered_movement(12, 1524, 500) == 6 && encumbered_movement(12, 1525, 500) == 3,
          "A load slows in steps past 512, 768 and 1,024 coins over the allowance");
    check(encumbered_movement(6, 1013, 500) == 6, "A load never speeds a slow creature up");
    check(attacks_this_round(3, 1) == 2 && attacks_this_round(3, 2) == 1 &&
              attacks_this_round(2, 1) == 1 && attacks_this_round(2, 2) == 1,
          "Three attacks in two rounds: two, then one");
    check(!attack_hits(1, 2, 10, 20) && attack_hits(20, 30, -10, -20),
          "A natural 1 misses and a natural 20 hits");
    check(attack_hits(15, 20, 5, 0) && !attack_hits(14, 20, 5, 0) && attack_hits(14, 20, 5, 1),
          "THAC0 20 against Armor Class 5 needs 15, or 14 with a +1");
    check(hit_damage(1, -2) == 0 && hit_damage(4, -2) == 2,
          "Damage below zero is none (the original wrapped it past 250)");
}

int main()
{
    try
    {
        every_offered_profession_fits_its_race();
        rolled_characters_keep_the_limits();
        racial_adjustments_apply();
        rolls_repeat_from_the_same_state();
        aging();
        thief_skill_chances();
        thac0_is_the_best_class();
        saving_throws_are_the_best_class();
        experience_and_level_limits();
        spell_slots_and_wisdom();
        dexterity_bonuses();
        constitution_and_age();
        creation_behind_the_rules_interface();
        combat_tables();
        std::cout << "Original rules passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
