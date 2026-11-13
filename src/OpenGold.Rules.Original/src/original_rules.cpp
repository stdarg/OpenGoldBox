#include "opengold/original_rules.h"
#include "opengold/dice.h"
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace opengold::original
{
namespace
{
using rules::Ability;
using rules::AbilityArray;

template <class Enum> [[nodiscard]] constexpr std::size_t index(Enum value) noexcept
{
    return static_cast<std::size_t>(value);
}

// The original lists abilities as Strength, Intelligence, Wisdom, Dexterity,
// Constitution and Charisma; OpenGoldBox's arrays keep the SRD's order.
[[nodiscard]] constexpr AbilityArray<int> in_original_order(int strength, int intelligence,
                                                            int wisdom, int dexterity,
                                                            int constitution, int charisma)
{
    return {strength, dexterity, constitution, intelligence, wisdom, charisma};
}

constexpr std::array<Ability, 6> original_ability_order{
    Ability::strength,  Ability::intelligence, Ability::wisdom,
    Ability::dexterity, Ability::constitution, Ability::charisma};

using C = ClassSlot;
using P = Profession;

constexpr std::array<std::array<C, 3>, 17> profession_classes{
    {{C::cleric},
     {C::druid},
     {C::fighter},
     {C::paladin},
     {C::ranger},
     {C::magic_user},
     {C::thief},
     {C::monk},
     {C::cleric, C::fighter},
     {C::cleric, C::fighter, C::magic_user},
     {C::cleric, C::ranger},
     {C::cleric, C::magic_user},
     {C::cleric, C::thief},
     {C::fighter, C::magic_user},
     {C::fighter, C::thief},
     {C::fighter, C::magic_user, C::thief},
     {C::magic_user, C::thief}}};

constexpr std::array<std::size_t, 17> profession_class_counts{1, 1, 1, 1, 1, 1, 1, 1, 2,
                                                              3, 2, 2, 2, 2, 2, 3, 2};

constexpr std::array<P, 3> small_folk_professions{P::fighter, P::thief, P::fighter_thief};
constexpr std::array<P, 7> elf_professions{P::fighter,         P::magic_user,
                                           P::thief,           P::fighter_magic_user,
                                           P::fighter_thief,   P::fighter_magic_user_thief,
                                           P::magic_user_thief};
constexpr std::array<P, 11> half_elf_professions{P::cleric,
                                                 P::fighter,
                                                 P::magic_user,
                                                 P::thief,
                                                 P::cleric_fighter,
                                                 P::cleric_fighter_magic_user,
                                                 P::cleric_magic_user,
                                                 P::fighter_magic_user,
                                                 P::fighter_thief,
                                                 P::fighter_magic_user_thief,
                                                 P::magic_user_thief};
constexpr std::array<P, 6> half_orc_professions{
    P::cleric, P::fighter, P::thief, P::cleric_fighter, P::cleric_thief, P::fighter_thief};
constexpr std::array<P, 4> human_professions{P::cleric, P::fighter, P::magic_user, P::thief};

using A = Alignment;
constexpr std::array<A, 9> every_alignment{A::lawful_good,  A::lawful_neutral,  A::lawful_evil,
                                           A::neutral_good, A::true_neutral,    A::neutral_evil,
                                           A::chaotic_good, A::chaotic_neutral, A::chaotic_evil};
// A thief, and a fighter or magic-user who is also one, is never lawful good
// or chaotic good.
constexpr std::array<A, 7> thief_alignments{A::lawful_neutral, A::lawful_evil,  A::neutral_good,
                                            A::true_neutral,   A::neutral_evil, A::chaotic_neutral,
                                            A::chaotic_evil};
constexpr std::array<A, 5> druid_alignments{A::lawful_neutral, A::neutral_good, A::true_neutral,
                                            A::neutral_evil, A::chaotic_neutral};
constexpr std::array<A, 1> paladin_alignments{A::lawful_good};

struct RaceLimits
{
    std::array<int, 2> strength_minimum, strength_maximum, exceptional_maximum;
    int intelligence_minimum, intelligence_maximum, wisdom_minimum, wisdom_maximum,
        dexterity_minimum, dexterity_maximum, constitution_minimum, constitution_maximum,
        charisma_minimum, charisma_maximum;
};

// By race; Strength by gender, male then female.
constexpr std::array<RaceLimits, 7> race_limits{{
    {{8, 8}, {18, 17}, {99, 0}, 3, 18, 3, 18, 3, 17, 12, 19, 3, 16},
    {{3, 3}, {18, 16}, {75, 0}, 8, 18, 3, 18, 7, 19, 6, 18, 8, 18},
    {{6, 6}, {18, 15}, {50, 0}, 7, 18, 3, 18, 3, 18, 8, 18, 3, 18},
    {{3, 3}, {18, 17}, {90, 0}, 4, 18, 3, 18, 6, 18, 6, 18, 3, 18},
    {{6, 6}, {17, 14}, {0, 0}, 6, 18, 3, 17, 8, 18, 10, 19, 3, 18},
    {{6, 6}, {18, 18}, {99, 75}, 3, 17, 3, 14, 3, 17, 13, 19, 3, 12},
    {{3, 3}, {18, 18}, {100, 50}, 3, 18, 3, 18, 3, 18, 3, 18, 3, 18},
}};

// By profession, in the original's ability order.
constexpr std::array<std::array<int, 6>, 17> profession_minimums{{{6, 6, 9, 0, 0, 0},
                                                                  {0, 0, 12, 0, 0, 15},
                                                                  {9, 0, 6, 6, 7, 0},
                                                                  {13, 9, 12, 0, 9, 17},
                                                                  {0, 13, 14, 0, 14, 0},
                                                                  {0, 9, 6, 6, 0, 0},
                                                                  {6, 6, 0, 9, 0, 0},
                                                                  {15, 0, 15, 15, 11, 0},
                                                                  {9, 0, 9, 0, 0, 0},
                                                                  {9, 9, 9, 0, 0, 0},
                                                                  {0, 13, 14, 0, 14, 0},
                                                                  {0, 9, 9, 0, 0, 0},
                                                                  {0, 0, 9, 9, 0, 0},
                                                                  {9, 9, 0, 0, 0, 0},
                                                                  {9, 0, 0, 9, 0, 0},
                                                                  {9, 9, 0, 9, 0, 0},
                                                                  {0, 9, 0, 9, 0, 0}}};

struct AgeDice
{
    int base{}, count{}, sides{};
};

// By race and class slot, cleric to thief; a zero base where the race cannot
// take the class.
constexpr std::array<std::array<AgeDice, 7>, 7> starting_ages{{
    {{{250, 2, 20}, {}, {40, 5, 4}, {}, {}, {}, {75, 3, 6}}},
    {{{650, 10, 10}, {}, {130, 5, 6}, {}, {}, {150, 5, 6}, {100, 5, 6}}},
    {{{300, 3, 12}, {}, {60, 5, 4}, {}, {}, {100, 2, 12}, {80, 5, 4}}},
    {{{40, 2, 4}, {}, {22, 3, 4}, {}, {}, {30, 2, 8}, {22, 3, 8}}},
    {{{}, {}, {20, 3, 4}, {}, {}, {}, {40, 2, 4}}},
    {{{20, 1, 4}, {}, {13, 1, 4}, {}, {}, {}, {20, 2, 4}}},
    {{{18, 1, 4}, {18, 1, 4}, {15, 1, 4}, {17, 1, 4}, {20, 1, 4}, {24, 2, 4}, {18, 1, 4}}},
}};

// The oldest age of each age category but the last, by race: young adult,
// mature, middle-aged and old.
constexpr std::array<std::array<int, 4>, 7> age_categories{{{50, 150, 250, 350},
                                                            {175, 550, 875, 1200},
                                                            {90, 300, 450, 600},
                                                            {40, 100, 175, 250},
                                                            {33, 68, 101, 144},
                                                            {15, 30, 45, 60},
                                                            {20, 40, 60, 90}}};

struct Dice
{
    int count{}, sides{};
};

constexpr std::array<Dice, class_slot_count> hit_dice{
    {{1, 8}, {1, 8}, {1, 10}, {1, 10}, {2, 8}, {1, 4}, {1, 6}, {2, 4}}};
constexpr std::array<Dice, class_slot_count> starting_gold_dice{
    {{3, 6}, {3, 6}, {5, 4}, {5, 4}, {5, 4}, {2, 4}, {2, 6}, {5, 4}}};

// The original stores 60 minus THAC0, by class slot and level 0 to 10.
constexpr std::array<std::array<int, 11>, class_slot_count> stored_thac0{
    {{40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46},
     {40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49},
     {40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49},
     {40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49},
     {40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41},
     {40, 40, 40, 40, 40, 41, 41, 41, 41, 44, 44},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46}}};

using SaveRow = std::array<int, 5>;
constexpr std::array<SaveRow, 3> cleric_saves{
    {{10, 13, 14, 16, 15}, {9, 12, 13, 15, 14}, {7, 10, 11, 13, 12}}};
constexpr std::array<SaveRow, 5> fighter_saves{{{14, 15, 16, 17, 17},
                                                {13, 14, 15, 16, 16},
                                                {11, 12, 13, 13, 14},
                                                {10, 11, 12, 12, 13},
                                                {8, 9, 10, 9, 11}}};
constexpr std::array<SaveRow, 2> magic_user_saves{{{14, 13, 11, 15, 12}, {13, 11, 9, 13, 10}}};
constexpr std::array<SaveRow, 3> thief_saves{
    {{13, 12, 14, 16, 15}, {12, 11, 12, 15, 13}, {11, 10, 10, 14, 11}}};

// A class's saving throws at a level from 1 to 9: clerics, druids and monks
// improve every three levels, fighters, paladins and rangers every two, and
// magic-users at level 6.
[[nodiscard]] SaveRow class_saves(ClassSlot slot, int level)
{
    const auto at = static_cast<std::size_t>(level - 1);
    switch (slot)
    {
    case C::cleric:
    case C::druid:
        return cleric_saves[at / 3];
    case C::fighter:
    case C::paladin:
    case C::ranger:
        return fighter_saves[at / 2];
    case C::magic_user:
        return magic_user_saves[at / 5];
    case C::thief:
    case C::monk:
        return thief_saves[at < 4 ? 0 : at < 8 ? 1 : 2];
    }
    throw std::invalid_argument("Unknown class");
}

// Experience for levels 2 to 9 by class slot; 0 where the class cannot reach
// the level in this game.
constexpr std::array<std::array<int, 8>, class_slot_count> experience_table{
    {{1501, 3001, 6001, 13001, 27501},
     {},
     {2001, 4001, 8001, 18001, 35001, 70001, 125001},
     {},
     {},
     {2501, 5001, 10001, 22501, 40001},
     {1251, 2501, 5001, 10001, 20001, 42501, 70001, 110001},
     {}}};

using SlotRow = std::array<int, 3>;
// Spell slots of levels 1 to 3, for class levels 2 to 6.
constexpr std::array<SlotRow, 5> cleric_slots{
    {{2, 0, 0}, {2, 1, 0}, {3, 2, 0}, {3, 3, 1}, {3, 3, 2}}};
constexpr std::array<SlotRow, 5> magic_user_slots{
    {{2, 0, 0}, {2, 1, 0}, {3, 2, 0}, {4, 2, 1}, {4, 2, 2}}};

using SkillRow = std::array<int, 8>;
// A thief's chances by thief level 1 to 9.
constexpr std::array<SkillRow, 9> thief_base{{{30, 25, 20, 15, 10, 10, 85, 0},
                                              {35, 29, 25, 21, 15, 10, 86, 0},
                                              {40, 33, 30, 27, 20, 15, 87, 0},
                                              {45, 37, 35, 33, 25, 15, 88, 20},
                                              {50, 42, 40, 40, 31, 20, 90, 25},
                                              {55, 47, 45, 47, 37, 20, 92, 30},
                                              {60, 52, 50, 55, 43, 25, 94, 35},
                                              {65, 57, 55, 62, 49, 25, 96, 40},
                                              {70, 62, 60, 70, 56, 30, 98, 45}}};
constexpr std::array<SkillRow, 7> thief_race_adjustments{{{0, 10, 15, 0, 0, 0, -10, -5},
                                                          {5, -5, 0, 5, 10, 5, 0, 0},
                                                          {0, 5, 10, 5, 5, 10, -15, 0},
                                                          {10, 0, 0, 0, 5, 0, 0, 0},
                                                          {5, 5, 5, 10, 15, 5, -15, -5},
                                                          {-5, 5, 5, 0, 0, 5, 5, -10},
                                                          {}}};
// Dexterity 9 to 19's adjustments to the first five skills. The original has
// -19 for Pick Pockets at 10 and -5 for Open Locks at 16, out of step with
// their neighbors; the user approved -10 and +5 (docs/DEVIATIONS.md).
constexpr std::array<std::array<int, 5>, 11> thief_dexterity_adjustments{{{-15, -10, -10, -20, -10},
                                                                          {-10, -5, -10, -15, -5},
                                                                          {-5, 0, -5, -10, 0},
                                                                          {0, 0, 0, -5, 0},
                                                                          {},
                                                                          {},
                                                                          {},
                                                                          {0, 5, 0, 0, 0},
                                                                          {5, 10, 0, 5, 5},
                                                                          {10, 15, 5, 10, 10},
                                                                          {15, 20, 10, 12, 12}}};

[[nodiscard]] int roll_dice(RandomState &random, Dice dice)
{
    int total = 0;
    for (int n = 0; n < dice.count; ++n)
    {
        total += roll_die(random, dice.sides);
    }
    return total;
}

[[nodiscard]] bool has_class(Profession profession, ClassSlot slot)
{
    const auto classes = classes_of(profession);
    return std::ranges::find(classes, slot) != classes.end();
}

// What the original's age categories do to an ability at this age. Each
// category's changes add to those before it; the young adult's -1 Wisdom is
// undone at maturity.
[[nodiscard]] int original_age_change(Race race, Ability ability, int age)
{
    const auto &category = age_categories[index(race)];
    const auto over = [&](std::size_t n)
    {
        return age > category[n] ? 1 : 0;
    };
    switch (ability)
    {
    case Ability::strength:
        return over(0) - over(1) - 2 * over(2) - over(3);
    case Ability::intelligence:
        return over(1) + over(3);
    case Ability::wisdom:
        return -(1 - over(0)) + over(1) + over(2) + over(3);
    case Ability::dexterity:
        return -2 * over(2) - over(3);
    case Ability::constitution:
        return -over(1) - over(2) - over(3);
    case Ability::charisma:
        return 0;
    }
    return 0;
}

[[nodiscard]] bool is_cleric_multi_class(Profession profession)
{
    return profession >= P::cleric_fighter && profession <= P::cleric_thief;
}

[[nodiscard]] int best_of_six(int adjustment, RandomState &random)
{
    int best = 0;
    for (int roll = 0; roll < 6; ++roll)
    {
        best = std::max(best, roll_dice(random, {3, 6}) + adjustment);
    }
    return best;
}

// The score at the starting age, as the original's creation leaves it.
[[nodiscard]] int starting_score(Race race, Gender gender, Profession profession,
                                 const RolledCharacter &rolled, Aging aging, Ability ability)
{
    const auto limits = ability_limits(race, gender);
    const int aged = rolled.rolls[ability] + age_changes(aging, race, rolled.starting_age)[ability];
    int score = std::clamp(aged, limits.minimum[ability], limits.maximum[ability]);
    score = std::max(score, class_minimums(profession)[ability]);
    if (ability == Ability::wisdom && is_cleric_multi_class(profession))
    {
        score = std::max(score, 13);
    }
    return score;
}

[[nodiscard]] int divided_among_classes(int total, int bonus, int class_count)
{
    if (bonus < 0 && total <= -bonus + class_count)
    {
        return 1;
    }
    return (total + bonus) / class_count;
}

[[nodiscard]] int starting_gold(Profession profession, RandomState &random)
{
    int total = 0;
    for (const auto slot : classes_of(profession))
    {
        total += roll_dice(random, starting_gold_dice[index(slot)]);
    }
    return total * 10 / static_cast<int>(classes_of(profession).size());
}
} // namespace

std::span<const ClassSlot> classes_of(Profession profession)
{
    const auto at = index(profession);
    return {profession_classes[at].data(), profession_class_counts[at]};
}

ClassLevels first_levels(Profession profession)
{
    ClassLevels levels{};
    for (const auto slot : classes_of(profession))
    {
        level_in(levels, slot) = 1;
    }
    return levels;
}

std::span<const Profession> professions_for(Race race)
{
    switch (race)
    {
    case Race::dwarf:
    case Race::gnome:
    case Race::halfling:
        return small_folk_professions;
    case Race::elf:
        return elf_professions;
    case Race::half_elf:
        return half_elf_professions;
    case Race::half_orc:
        return half_orc_professions;
    case Race::human:
        return human_professions;
    }
    throw std::invalid_argument("Unknown race");
}

std::span<const Alignment> alignments_for(Profession profession)
{
    switch (profession)
    {
    case P::druid:
        return druid_alignments;
    case P::paladin:
        return paladin_alignments;
    case P::thief:
    case P::fighter_thief:
    case P::fighter_magic_user_thief:
    case P::magic_user_thief:
        return thief_alignments;
    default:
        return every_alignment;
    }
}

AbilityLimits ability_limits(Race race, Gender gender)
{
    const auto &limits = race_limits[index(race)];
    const auto g = index(gender);
    return {in_original_order(limits.strength_minimum[g], limits.intelligence_minimum,
                              limits.wisdom_minimum, limits.dexterity_minimum,
                              limits.constitution_minimum, limits.charisma_minimum),
            in_original_order(limits.strength_maximum[g], limits.intelligence_maximum,
                              limits.wisdom_maximum, limits.dexterity_maximum,
                              limits.constitution_maximum, limits.charisma_maximum),
            limits.exceptional_maximum[g]};
}

AbilityArray<int> racial_adjustments(Race race)
{
    switch (race)
    {
    case Race::dwarf:
        return in_original_order(0, 0, 0, 0, 1, -1);
    case Race::elf:
        return in_original_order(0, 0, 0, 1, -1, 0);
    case Race::halfling:
        return in_original_order(-1, 0, 0, 1, 0, 0);
    case Race::half_orc:
        return in_original_order(1, 0, 0, 0, 1, -2);
    case Race::gnome:
    case Race::half_elf:
    case Race::human:
        return {};
    }
    throw std::invalid_argument("Unknown race");
}

AbilityArray<int> class_minimums(Profession profession)
{
    const auto &m = profession_minimums[index(profession)];
    return in_original_order(m[0], m[1], m[2], m[3], m[4], m[5]);
}

AgeCategory age_category(Race race, int age)
{
    const auto &category = age_categories[index(race)];
    const auto older = std::ranges::count_if(category,
                                             [age](int oldest)
                                             {
                                                 return age > oldest;
                                             });
    return static_cast<AgeCategory>(older);
}

namespace
{
constexpr std::array<std::string_view, 3> aging_names{"off", "original", "add"};
} // namespace

std::string_view aging_name(Aging aging) noexcept
{
    return aging_names[static_cast<std::size_t>(aging)];
}

std::optional<Aging> aging_named(std::string_view name) noexcept
{
    const auto found = std::ranges::find(aging_names, name);
    if (found == aging_names.end())
    {
        return std::nullopt;
    }
    return static_cast<Aging>(found - aging_names.begin());
}

AbilityArray<int> age_changes(Aging aging, Race race, int age)
{
    AbilityArray<int> changes;
    if (aging == Aging::off)
    {
        return changes;
    }
    for (const auto ability : original_ability_order)
    {
        changes[ability] = original_age_change(race, ability, age);
    }
    if (aging == Aging::add)
    {
        // AD&D's young adult also gains 1 Constitution, which every older age
        // group keeps.
        changes[Ability::constitution] += 1;
    }
    return changes;
}

RolledCharacter roll_character(Race race, Profession profession, RandomState &random)
{
    RolledCharacter rolled;
    rolled.starting_age = starting_age(race, profession, random);
    for (const auto ability : original_ability_order)
    {
        rolled.rolls[ability] = best_of_six(racial_adjustments(race)[ability], random);
    }
    rolled.strength_percentile = roll_die(random, 100);
    rolled.gold = starting_gold(profession, random);
    const auto classes = classes_of(profession);
    for (std::size_t n = 0; n < classes.size(); ++n)
    {
        const auto dice = hit_dice[index(classes[n])];
        rolled.first_level_hit_dice[n] = std::max(roll_dice(random, dice), dice.sides * 2 / 3);
    }
    return rolled;
}

AbilityArray<int> ability_scores(Race race, Gender gender, Profession profession,
                                 const RolledCharacter &rolled, Aging aging, int current_age)
{
    AbilityArray<int> scores;
    const auto since_start = [&](Ability ability)
    {
        if (aging != Aging::add)
        {
            return 0;
        }
        return age_changes(aging, race, current_age)[ability] -
               age_changes(aging, race, rolled.starting_age)[ability];
    };
    const auto limits = ability_limits(race, gender);
    for (const auto ability : original_ability_order)
    {
        const int start = starting_score(race, gender, profession, rolled, aging, ability);
        const int change = since_start(ability);
        scores[ability] =
            change == 0 ? start : std::clamp(start + change, 3, limits.maximum[ability]);
    }
    return scores;
}

int exceptional_strength(Race race, Gender gender, Profession profession,
                         const RolledCharacter &rolled, int strength)
{
    if (strength != 18 || !has_class(profession, C::fighter))
    {
        return 0;
    }
    return std::min(rolled.strength_percentile,
                    ability_limits(race, gender).exceptional_strength_maximum);
}

int first_level_hit_points(Profession profession, const RolledCharacter &rolled, int constitution)
{
    const auto class_count = classes_of(profession).size();
    int total = 0;
    for (std::size_t n = 0; n < class_count; ++n)
    {
        total += rolled.first_level_hit_dice[n];
    }
    return divided_among_classes(total, constitution_hit_point_bonus(profession, constitution),
                                 static_cast<int>(class_count));
}

int starting_age(Race race, Profession profession, RandomState &random)
{
    const auto &ages = starting_ages[index(race)];
    const auto classes = classes_of(profession);
    if (classes.size() == 1)
    {
        if (classes.front() == C::monk)
        {
            throw std::invalid_argument("The original has no starting age for a monk");
        }
        const auto dice = ages[index(classes.front())];
        return dice.base + roll_dice(random, {dice.count, dice.sides});
    }
    // A multi-class character is as old as one of its classes allows at most:
    // its cleric's, else its magic-user's, else its fighter's.
    const auto oldest = has_class(profession, C::cleric)       ? C::cleric
                        : has_class(profession, C::magic_user) ? C::magic_user
                                                               : C::fighter;
    const auto dice = ages[index(oldest)];
    return dice.base + dice.count * dice.sides;
}

int constitution_hit_point_bonus(Profession profession, int constitution)
{
    const int score = std::clamp(constitution, 3, 20);
    const int per_class = score == 3 ? -2 : score <= 6 ? -1 : score <= 14 ? 0 : score == 15 ? 1 : 2;
    int bonus = per_class * static_cast<int>(classes_of(profession).size());
    if (profession == P::fighter)
    {
        bonus += (score > 16 ? 1 : 0) + (score > 17 ? 1 : 0);
    }
    return bonus;
}

// Above 18 each step comes two points sooner than in the original, so an elf's
// natural 19 is worth more than 18 (docs/DEVIATIONS.md, the original rules).
int dexterity_armor_class_bonus(int dexterity) noexcept
{
    if (dexterity <= 3)
    {
        return -4;
    }
    if (dexterity <= 6)
    {
        return dexterity - 7;
    }
    if (dexterity <= 14)
    {
        return 0;
    }
    if (dexterity <= 18)
    {
        return dexterity - 14;
    }
    return dexterity <= 21 ? 5 : 6;
}

int dexterity_missile_bonus(int dexterity) noexcept
{
    if (dexterity <= 2)
    {
        return -4;
    }
    if (dexterity <= 5)
    {
        return dexterity - 6;
    }
    if (dexterity <= 15)
    {
        return 0;
    }
    if (dexterity <= 18)
    {
        return dexterity - 15;
    }
    return dexterity <= 21 ? 4 : 5;
}

std::array<int, 8> thief_skills(Race race, int thief_level, int dexterity)
{
    std::array<int, 8> skills{};
    if (thief_level < 1)
    {
        return skills;
    }
    const auto &base = thief_base[static_cast<std::size_t>(std::min(thief_level, 9) - 1)];
    const auto &racial = thief_race_adjustments[index(race)];
    const auto &nimble =
        thief_dexterity_adjustments[static_cast<std::size_t>(std::clamp(dexterity, 9, 19) - 9)];
    for (std::size_t skill = 0; skill < skills.size(); ++skill)
    {
        // A racial penalty larger than the chance leaves none at all, as in the
        // original. The original kept the chance in a byte, so a Dexterity
        // penalty larger than the chance wrapped around to more than 200
        // percent; OpenGoldBox stops at 0.
        if (racial[skill] < 0 && -racial[skill] > base[skill])
        {
            continue;
        }
        const int dexterity_change = skill < nimble.size() ? nimble[skill] : 0;
        skills[skill] = std::max(0, base[skill] + racial[skill] + dexterity_change);
    }
    return skills;
}

int thac0(const ClassLevels &levels)
{
    int best = 0;
    for (std::size_t slot = 0; slot < class_slot_count; ++slot)
    {
        const auto level = static_cast<std::size_t>(std::clamp(levels[slot], 0, 10));
        best = std::max(best, stored_thac0[slot][level]);
    }
    return 60 - best;
}

std::array<int, 5> saving_throws(const ClassLevels &levels)
{
    std::array<int, 5> best{20, 20, 20, 20, 20};
    for (std::size_t slot = 0; slot < class_slot_count; ++slot)
    {
        if (levels[slot] <= 0)
        {
            continue;
        }
        const auto row = class_saves(static_cast<ClassSlot>(slot), std::min(levels[slot], 9));
        for (std::size_t save = 0; save < best.size(); ++save)
        {
            best[save] = std::min(best[save], row[save]);
        }
    }
    return best;
}

std::optional<int> experience_for_level(ClassSlot slot, int level)
{
    if (level == 1)
    {
        return 0;
    }
    if (level < 2 || level > 9)
    {
        return std::nullopt;
    }
    const int needed = experience_table[index(slot)][static_cast<std::size_t>(level - 2)];
    return needed == 0 ? std::nullopt : std::optional<int>{needed};
}

std::optional<int> racial_level_limit(Race race, ClassSlot slot, int strength)
{
    const auto by_strength = [strength](int below_17, int at_17, int above)
    {
        return strength < 17 ? below_17 : strength == 17 ? at_17 : above;
    };
    if (race == Race::half_elf && slot == C::cleric)
    {
        return 5;
    }
    if (slot != C::fighter)
    {
        return std::nullopt;
    }
    switch (race)
    {
    case Race::dwarf:
        // A dwarf of 18 Strength is limited only by the experience table.
        return strength < 17    ? std::optional<int>{7}
               : strength == 17 ? std::optional<int>{8}
                                : std::nullopt;
    case Race::elf:
        return by_strength(5, 6, 7);
    case Race::gnome:
        return strength < 18 ? 5 : 6;
    case Race::half_elf:
        return by_strength(6, 7, 8);
    case Race::halfling:
        return by_strength(4, 5, 6);
    case Race::half_orc:
    case Race::human:
        return std::nullopt;
    }
    return std::nullopt;
}

std::array<int, 3> spell_slots(ClassSlot slot, int level, int wisdom)
{
    if ((slot != C::cleric && slot != C::magic_user) || level < 1)
    {
        return {};
    }
    const auto &table = slot == C::cleric ? cleric_slots : magic_user_slots;
    SlotRow slots =
        level == 1 ? SlotRow{1, 0, 0} : table[static_cast<std::size_t>(std::min(level, 6) - 2)];
    if (slot == C::cleric)
    {
        // The original's Wisdom bonus adds a slot only at a spell level the
        // cleric already casts.
        const auto add = [&slots](std::size_t spell_level, bool earned)
        {
            if (earned && slots[spell_level] > 0)
            {
                ++slots[spell_level];
            }
        };
        add(0, wisdom > 12);
        add(0, wisdom > 13);
        add(1, wisdom > 14);
        add(1, wisdom > 15);
        add(2, wisdom > 16);
    }
    return slots;
}

int strength_scale(int strength, int exceptional_strength) noexcept
{
    if (strength < 18)
    {
        return std::max(strength, 0);
    }
    if (strength > 18)
    {
        return std::min(strength, 25) + 5;
    }
    if (exceptional_strength <= 0)
    {
        return 18;
    }
    if (exceptional_strength <= 50)
    {
        return 19;
    }
    if (exceptional_strength <= 75)
    {
        return 20;
    }
    if (exceptional_strength <= 90)
    {
        return 21;
    }
    return exceptional_strength <= 99 ? 22 : 23;
}

int strength_to_hit_bonus(int strength, int exceptional_strength) noexcept
{
    const auto scale = strength_scale(strength, exceptional_strength);
    if (scale < 1)
    {
        return 0;
    }
    if (scale <= 3)
    {
        return -3;
    }
    if (scale <= 5)
    {
        return -2;
    }
    if (scale <= 7)
    {
        return -1;
    }
    if (scale <= 16)
    {
        return 0;
    }
    if (scale <= 19)
    {
        return 1;
    }
    if (scale <= 22)
    {
        return 2;
    }
    if (scale <= 25)
    {
        return 3;
    }
    return scale <= 27 ? 4 : scale - 23;
}

int strength_damage_bonus(int strength, int exceptional_strength) noexcept
{
    const auto scale = strength_scale(strength, exceptional_strength);
    if (scale < 1)
    {
        return 0;
    }
    if (scale <= 2)
    {
        return -2;
    }
    if (scale <= 5)
    {
        return -1;
    }
    if (scale <= 15)
    {
        return 0;
    }
    if (scale <= 19)
    {
        return scale == 16 ? 1 : scale - 16;
    }
    return scale <= 29 ? scale - 17 : 14;
}

int weight_allowance(int strength, int exceptional_strength) noexcept
{
    const auto scale = strength_scale(strength, exceptional_strength);
    if (scale >= 1 && scale <= 3)
    {
        return -350;
    }
    if (scale >= 4 && scale <= 5)
    {
        return -250;
    }
    if (scale >= 6 && scale <= 7)
    {
        return -150;
    }
    if (scale >= 12 && scale <= 13)
    {
        return 100;
    }
    if (scale >= 14 && scale <= 15)
    {
        return 200;
    }
    if (scale == 16)
    {
        return 350;
    }
    if (scale >= 17 && scale <= 21)
    {
        return (scale - 17) * 250 + 500;
    }
    if (scale >= 22 && scale <= 26)
    {
        return (scale - 22) * 1000 + 2000;
    }
    if (scale == 27)
    {
        return 7500;
    }
    if (scale >= 28)
    {
        return (scale - 28) * 3000 + 9000;
    }
    return 0;
}

int encumbered_movement(int movement, int load, int allowance) noexcept
{
    const int over = std::max(load - allowance, 0);
    const int limit = over <= 512 ? movement : over <= 768 ? 9 : over <= 1024 ? 6 : 3;
    return std::min(movement, limit);
}

int attacks_this_round(int rate_per_two_rounds, int round) noexcept
{
    return (rate_per_two_rounds + (round % 2 != 0 ? 1 : 0)) / 2;
}

bool attack_hits(int natural_roll, int thac0, int armor_class, int modifier) noexcept
{
    if (natural_roll <= 1)
    {
        return false;
    }
    return natural_roll >= 20 || natural_roll + modifier >= thac0 - armor_class;
}

int hit_damage(int dice_total, int modifier) noexcept
{
    return std::max(dice_total + modifier, 0);
}
} // namespace opengold::original
