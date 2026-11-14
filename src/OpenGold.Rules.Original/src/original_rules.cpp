#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
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
// Curse opens the ranger to half-elves and the paladin and ranger to humans.
constexpr std::array<P, 13> curse_half_elf_professions{P::cleric,
                                                       P::fighter,
                                                       P::magic_user,
                                                       P::thief,
                                                       P::ranger,
                                                       P::cleric_fighter,
                                                       P::cleric_ranger,
                                                       P::cleric_fighter_magic_user,
                                                       P::cleric_magic_user,
                                                       P::fighter_magic_user,
                                                       P::fighter_thief,
                                                       P::fighter_magic_user_thief,
                                                       P::magic_user_thief};
constexpr std::array<P, 6> curse_human_professions{P::cleric, P::fighter, P::magic_user,
                                                   P::thief,  P::paladin, P::ranger};

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
constexpr std::array<A, 3> ranger_alignments{A::lawful_good, A::neutral_good, A::chaotic_good};

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
                                                                  {12, 9, 13, 0, 9, 17},
                                                                  {13, 13, 14, 0, 14, 0},
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

// Curse's: by level 0 to 12. A fighter, paladin or ranger first improves at
// level 3, a magic-user's THAC0 is 21 to level 5.
constexpr std::array<std::array<int, 13>, class_slot_count> curse_stored_thac0{
    {{40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46},
     {39, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
     {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
     {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51},
     {39, 39, 39, 39, 39, 39, 41, 41, 41, 41, 41, 43, 43},
     {40, 40, 40, 40, 40, 41, 41, 41, 41, 44, 44, 44, 44},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46}}};

// Secret of the Silver Blades' THAC0, level 0 to 18 (its baseToHitTbl):
// Curse's, carried on. Secret has no monks; their row is a cleric's.
constexpr std::array<std::array<int, 19>, class_slot_count> secret_stored_thac0{
    {{40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46, 48, 48, 48, 50, 50, 50},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46, 48, 48, 48, 50, 50, 50},
     {39, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57},
     {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57},
     {40, 40, 40, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57},
     {39, 39, 39, 39, 39, 39, 41, 41, 41, 41, 41, 43, 43, 43, 43, 43, 46, 46, 46},
     {40, 40, 40, 40, 40, 41, 41, 41, 41, 44, 44, 44, 44, 46, 46, 46, 46, 48, 48},
     {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46, 48, 48, 48, 50, 50, 50}}};

// Pools of Darkness's THAC0, level 0 to 21 (its baseToHitTbl): warriors
// improve every second level to 56 (THAC0 4) at 17; a magic-user's is 20 from
// level 1 and improves at 6, 11, 16 and 21; clerics and thieves keep Secret's
// to level 18. It has no monks; their row is a cleric's.
constexpr std::array<std::array<int, 22>, class_slot_count> darkness_stored_thac0 = []
{
    constexpr std::array<int, 22> fighter{39, 40, 40, 42, 42, 44, 44, 46, 46, 48, 48,
                                          50, 50, 52, 52, 54, 54, 56, 56, 56, 56, 56};
    constexpr std::array<int, 22> magic_user{39, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41,
                                             44, 44, 44, 44, 44, 47, 47, 47, 47, 47, 49};
    constexpr std::array<int, 3> cleric_from_19{51, 51, 51}, thief_from_19{48, 48, 50};
    std::array<std::array<int, 22>, class_slot_count> table{};
    const auto secret_then = [](std::size_t slot, const std::array<int, 3> &from_19)
    {
        std::array<int, 22> row{};
        std::copy(secret_stored_thac0[slot].begin(), secret_stored_thac0[slot].end(), row.begin());
        std::copy(from_19.begin(), from_19.end(), row.begin() + 19);
        return row;
    };
    table[0] = table[1] = table[7] = secret_then(0, cleric_from_19);
    table[2] = table[3] = table[4] = fighter;
    table[3][0] = table[4][0] = 40;
    table[5] = magic_user;
    table[6] = secret_then(6, thief_from_19);
    return table;
}();

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
// To level 12, the last rows Curse's alone.
constexpr std::array<SaveRow, 4> cleric_saves{
    {{10, 13, 14, 16, 15}, {9, 12, 13, 15, 14}, {7, 10, 11, 13, 12}, {6, 9, 10, 12, 11}}};
constexpr std::array<SaveRow, 6> fighter_saves{{{14, 15, 16, 17, 17},
                                                {13, 14, 15, 16, 16},
                                                {11, 12, 13, 13, 14},
                                                {10, 11, 12, 12, 13},
                                                {8, 9, 10, 9, 11},
                                                {7, 8, 9, 8, 10}}};
constexpr std::array<SaveRow, 3> magic_user_saves{
    {{14, 13, 11, 15, 12}, {13, 11, 9, 13, 10}, {11, 9, 7, 11, 8}}};
constexpr std::array<SaveRow, 3> thief_saves{
    {{13, 12, 14, 16, 15}, {12, 11, 12, 15, 13}, {11, 10, 10, 14, 11}}};

// Secret of the Silver Blades' saving throws for each class and level 1 to
// 18 (its saveTbl): Curse's rows carried on. Its paladins' are a fighter's
// less 2 but for the same three faults as Curse's, fixed here as there
// (docs/ORIGINAL-RULES.md, Bugs). Druids save as clerics, its absent monks as
// thieves.
constexpr std::array<std::array<SaveRow, 18>, class_slot_count> secret_saves = []
{
    constexpr std::array<SaveRow, 6> cleric{{{10, 13, 14, 16, 15},
                                             {9, 12, 13, 15, 14},
                                             {7, 10, 11, 13, 12},
                                             {6, 9, 10, 12, 11},
                                             {5, 8, 9, 11, 10},
                                             {4, 7, 8, 10, 9}}};
    constexpr std::array<SaveRow, 9> fighter{{{14, 15, 16, 17, 17},
                                              {13, 14, 15, 16, 16},
                                              {11, 12, 13, 13, 14},
                                              {10, 11, 12, 12, 13},
                                              {8, 9, 10, 9, 11},
                                              {7, 8, 9, 8, 10},
                                              {5, 6, 7, 5, 8},
                                              {4, 5, 6, 4, 7},
                                              {3, 4, 5, 4, 6}}};
    constexpr std::array<SaveRow, 4> magic_user{
        {{14, 13, 11, 15, 12}, {13, 11, 9, 13, 10}, {11, 9, 7, 11, 8}, {10, 7, 5, 9, 6}}};
    constexpr std::array<SaveRow, 5> thief{{{13, 12, 14, 16, 15},
                                            {12, 11, 12, 15, 13},
                                            {11, 10, 10, 14, 11},
                                            {10, 9, 8, 13, 9},
                                            {9, 8, 6, 12, 7}}};
    std::array<std::array<SaveRow, 18>, class_slot_count> table{};
    for (std::size_t at = 0; at < 18; ++at)
    {
        const auto clerical = cleric[at / 3];
        const auto fighting = fighter[at / 2];
        auto paladin = fighting;
        for (auto &save : paladin)
        {
            save -= 2;
        }
        const auto thieving = thief[at / 4];
        table[0][at] = clerical;
        table[1][at] = clerical;
        table[2][at] = fighting;
        table[3][at] = paladin;
        table[4][at] = fighting;
        table[5][at] = magic_user[at / 5];
        table[6][at] = thieving;
        table[7][at] = thieving;
    }
    return table;
}();

// Pools of Darkness's saving throws at levels 19 to 21 (its saveTbl); to 18
// they are Secret's. Its cleric and thief rows at 19 repeat level 18's, a
// level early; they are given level 20's here (docs/ORIGINAL-RULES.md, Bugs).
constexpr std::array<std::array<SaveRow, 3>, class_slot_count> darkness_high_saves{{
    {{{2, 5, 6, 8, 7}, {2, 5, 6, 8, 7}, {2, 5, 6, 8, 7}}},
    {{{4, 7, 8, 10, 9}, {4, 7, 8, 10, 9}, {4, 7, 8, 10, 9}}},
    {{{3, 4, 5, 4, 6}, {3, 4, 5, 4, 6}, {3, 4, 5, 4, 6}}},
    {{{1, 2, 3, 2, 4}, {1, 2, 3, 2, 4}, {1, 2, 3, 2, 4}}},
    {{{3, 4, 5, 4, 6}, {3, 4, 5, 4, 6}, {3, 4, 5, 4, 6}}},
    {{{10, 7, 5, 9, 6}, {10, 7, 5, 9, 6}, {8, 5, 3, 7, 4}}},
    {{{9, 8, 6, 12, 7}, {9, 8, 6, 12, 7}, {8, 7, 4, 11, 5}}},
    {{{9, 8, 6, 12, 7}, {9, 8, 6, 12, 7}, {8, 7, 4, 11, 5}}},
}};

// A class's saving throws at a level from 1 to 12: clerics, druids and monks
// improve every three levels, fighters, paladins and rangers every two,
// magic-users at levels 6 and 11, thieves at 5 and 9.
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
constexpr std::array<std::array<int, 8>, class_slot_count> experience_table_pool{
    {{1501, 3001, 6001, 13001, 27501},
     {},
     {2001, 4001, 8001, 18001, 35001, 70001, 125001},
     {},
     {},
     {2501, 5001, 10001, 22501, 40001},
     {1251, 2501, 5001, 10001, 20001, 42501, 70001, 110001},
     {}}};

// Curse's, for levels 2 to 13: its paladins and rangers, and higher levels.
constexpr std::array<std::array<int, 12>, class_slot_count> experience_table_curse{
    {{1501, 3001, 6001, 13001, 27501, 55001, 110001, 225001, 450001},
     {},
     {2001, 4001, 8001, 18001, 35001, 70001, 125001, 250001, 500001, 750001, 1000001},
     {2751, 5501, 12001, 24001, 45001, 95001, 175001, 350001, 700001, 1050001},
     {2251, 4501, 10001, 20001, 40001, 90001, 150001, 225001, 325001, 650001},
     {2501, 5001, 10001, 22501, 40001, 60001, 90001, 135001, 250001, 375001},
     {1251, 2501, 5001, 10001, 20001, 42501, 70001, 110001, 160001, 220001, 440001},
     {}}};

// Secret's, for levels 2 to 18: Curse's, carried on to its higher levels.
constexpr std::array<std::array<int, 17>, class_slot_count> experience_table_secret{
    {{1501, 3001, 6001, 13001, 27501, 55001, 110001, 225001, 450001, 675001, 900001, 1125001,
      1350001, 1575001},
     {},
     {2001, 4001, 8001, 18001, 35001, 70001, 125001, 250001, 500001, 750001, 1000001, 1250001,
      1500001, 1750001},
     {2751, 5501, 12001, 24001, 45001, 95001, 175001, 350001, 700001, 1050001, 1400001, 1750001,
      2100001, 2460001},
     {2251, 4501, 10001, 20001, 40001, 90001, 150001, 225001, 325001, 650001, 975001, 1300001,
      1625001, 1950001},
     {2501, 5001, 10001, 22501, 40001, 60001, 90001, 135001, 250001, 375001, 750001, 1125001,
      1500001, 1875001},
     {1251, 2501, 5001, 10001, 20001, 42501, 70001, 110001, 160001, 220001, 440001, 660001, 880001,
      1100001, 1320001, 1540001, 1760001},
     {}}};

// Pools of Darkness's experience from level 11 to 40 (its XpForLevel): level
// 11's, then so much more a level. Its paladins and rangers need 1 less than
// Secret's at 11.
constexpr std::array<int, class_slot_count> darkness_level_11{675001, 0,      750001, 1050000,
                                                              650000, 375001, 220001, 0};
constexpr std::array<int, class_slot_count> darkness_per_level{225000, 0,      250000, 350000,
                                                               325000, 375000, 220000, 0};
constexpr int darkness_highest_level = 40;

// Curse rolls a class's Hit Die at each level below this one, and adds a
// fixed amount from it on (curse_fixed_hit_points).
constexpr std::array<int, class_slot_count> curse_hit_dice_levels{10, 15, 10, 10, 11, 12, 11, 13};
constexpr std::array<int, class_slot_count> curse_fixed_hit_points{2, 0, 3, 3, 2, 1, 2, 0};

// Gateway to the Savage Frontier keeps Curse's tables, number for number.
[[nodiscard]] constexpr bool curse_tables(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::curse_of_the_azure_bonds ||
           game == GoldBoxGame::gateway_to_the_savage_frontier;
}

// Gateway caps every class by its own level, whatever the race (its
// levelCaps): clerics, druids and magic-users 6, rangers 7, the rest 8.
constexpr std::array<int, class_slot_count> gateway_level_caps{6, 6, 8, 8, 7, 6, 8, 8};

using SlotRow = std::array<int, 7>;
// Spell slots of levels 1 to 7, for class levels 2 to 15 (Secret's, its
// classProgression summed). Pool of Radiance's and Curse's are the same to
// level 12 at spell levels 1 to 5, and stop there (spell_slots).
constexpr std::array<SlotRow, 14> cleric_slots{{{2, 0, 0, 0, 0, 0, 0},
                                                {2, 1, 0, 0, 0, 0, 0},
                                                {3, 2, 0, 0, 0, 0, 0},
                                                {3, 3, 1, 0, 0, 0, 0},
                                                {3, 3, 2, 0, 0, 0, 0},
                                                {3, 3, 2, 1, 0, 0, 0},
                                                {3, 3, 3, 2, 0, 0, 0},
                                                {4, 4, 3, 2, 1, 0, 0},
                                                {4, 4, 3, 3, 2, 0, 0},
                                                {5, 4, 4, 3, 2, 1, 0},
                                                {6, 5, 5, 3, 2, 2, 0},
                                                {6, 6, 6, 4, 2, 2, 0},
                                                {6, 6, 6, 5, 3, 2, 0},
                                                {7, 7, 7, 5, 4, 2, 0}}};
constexpr std::array<SlotRow, 14> magic_user_slots{{{2, 0, 0, 0, 0, 0, 0},
                                                    {2, 1, 0, 0, 0, 0, 0},
                                                    {3, 2, 0, 0, 0, 0, 0},
                                                    {4, 2, 1, 0, 0, 0, 0},
                                                    {4, 2, 2, 0, 0, 0, 0},
                                                    {4, 3, 2, 1, 0, 0, 0},
                                                    {4, 3, 3, 2, 0, 0, 0},
                                                    {4, 3, 3, 2, 1, 0, 0},
                                                    {4, 4, 3, 2, 2, 0, 0},
                                                    {4, 4, 4, 3, 3, 0, 0},
                                                    {4, 4, 4, 4, 4, 1, 0},
                                                    {5, 5, 5, 4, 4, 2, 0},
                                                    {5, 5, 5, 4, 4, 2, 1},
                                                    {5, 5, 5, 5, 5, 2, 1}}};

// Pools of Darkness's slots of spell levels 1 to 7 for class levels 16 to 29
// (its spell table; to 15 they are Secret's). Its magic-users' eighth- and
// ninth-level slots are not kept yet.
constexpr std::array<SlotRow, 14> darkness_cleric_slots{{{7, 7, 7, 6, 5, 3, 1},
                                                         {8, 8, 8, 6, 5, 3, 1},
                                                         {8, 8, 8, 7, 6, 4, 1},
                                                         {9, 9, 9, 7, 6, 4, 2},
                                                         {9, 9, 9, 8, 7, 5, 2},
                                                         {9, 9, 9, 9, 8, 6, 2},
                                                         {9, 9, 9, 9, 9, 6, 3},
                                                         {9, 9, 9, 9, 9, 7, 3},
                                                         {9, 9, 9, 9, 9, 8, 3},
                                                         {9, 9, 9, 9, 9, 8, 4},
                                                         {9, 9, 9, 9, 9, 9, 4},
                                                         {9, 9, 9, 9, 9, 9, 5},
                                                         {9, 9, 9, 9, 9, 9, 6},
                                                         {9, 9, 9, 9, 9, 9, 7}}};
constexpr std::array<SlotRow, 14> darkness_magic_user_slots{{{5, 5, 5, 5, 5, 3, 2},
                                                             {5, 5, 5, 5, 5, 3, 3},
                                                             {5, 5, 5, 5, 5, 3, 3},
                                                             {5, 5, 5, 5, 5, 3, 3},
                                                             {5, 5, 5, 5, 5, 4, 3},
                                                             {5, 5, 5, 5, 5, 4, 4},
                                                             {5, 5, 5, 5, 5, 5, 4},
                                                             {5, 5, 5, 5, 5, 5, 5},
                                                             {5, 5, 5, 5, 5, 5, 5},
                                                             {5, 5, 5, 5, 5, 5, 5},
                                                             {6, 6, 6, 6, 5, 5, 5},
                                                             {6, 6, 6, 6, 6, 6, 6},
                                                             {6, 6, 6, 6, 6, 6, 6},
                                                             {7, 7, 7, 7, 6, 6, 6}}};

using SkillRow = std::array<int, 8>;
// A thief's chances by thief level 1 to 9.
// To level 9 in Pool of Radiance, 12 in Curse and 18 in Secret (its
// thiefBase), whose Read Languages at 11 and 12 are 5 below Curse's.
constexpr std::array<SkillRow, 18> thief_base{{{30, 25, 20, 15, 10, 10, 85, 0},
                                               {35, 29, 25, 21, 15, 10, 86, 0},
                                               {40, 33, 30, 27, 20, 15, 87, 0},
                                               {45, 37, 35, 33, 25, 15, 88, 20},
                                               {50, 42, 40, 40, 31, 20, 90, 25},
                                               {55, 47, 45, 47, 37, 20, 92, 30},
                                               {60, 52, 50, 55, 43, 25, 94, 35},
                                               {65, 57, 55, 62, 49, 25, 96, 40},
                                               {70, 62, 60, 70, 56, 30, 98, 45},
                                               {80, 67, 65, 78, 63, 30, 99, 50},
                                               {90, 72, 70, 86, 70, 35, 99, 55},
                                               {100, 77, 75, 94, 77, 35, 99, 60},
                                               {105, 82, 80, 99, 85, 40, 99, 65},
                                               {110, 87, 85, 99, 93, 40, 99, 70},
                                               {115, 92, 90, 99, 99, 50, 100, 75},
                                               {125, 97, 95, 99, 99, 50, 100, 80},
                                               {125, 99, 99, 99, 99, 55, 100, 80},
                                               {130, 99, 99, 99, 99, 55, 100, 85}}};
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

// Curse's roll of a class's Hit Die (ovr018 sub_509E0): the better of two.
[[nodiscard]] int better_of_two(Dice dice, RandomState &random)
{
    const int first = roll_dice(random, dice);
    return std::max(first, roll_dice(random, dice));
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

// Secret of the Silver Blades and Pools of Darkness divide a level's rolled
// Hit Points and its Constitution bonus among the classes apart, and count the
// bonus once for each die.
[[nodiscard]] constexpr bool secret_hit_points(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::secret_of_the_silver_blades ||
           game == GoldBoxGame::pools_of_darkness;
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

std::string_view class_slot_id(ClassSlot slot) noexcept
{
    constexpr std::array<std::string_view, class_slot_count> ids{
        "cleric", "druid", "fighter", "paladin", "ranger", "magic_user", "thief", "monk"};
    return ids[index(slot)];
}

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

std::span<const Profession> professions_for(Race race, GoldBoxGame game)
{
    // Secret offers Curse's classes (its raceClassTbl is Curse's).
    const bool curse = game != GoldBoxGame::pool_of_radiance;
    switch (race)
    {
    case Race::dwarf:
    case Race::gnome:
    case Race::halfling:
        return small_folk_professions;
    case Race::elf:
        return elf_professions;
    case Race::half_elf:
        return curse ? std::span<const Profession>(curse_half_elf_professions)
                     : std::span<const Profession>(half_elf_professions);
    case Race::half_orc:
        // Unlimited Adventures has no half-orcs (docs/games/ua.md).
        if (game == GoldBoxGame::unlimited_adventures)
        {
            return {};
        }
        return half_orc_professions;
    case Race::human:
        return curse ? std::span<const Profession>(curse_human_professions)
                     : std::span<const Profession>(human_professions);
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
    case P::ranger:
    case P::cleric_ranger:
        return ranger_alignments;
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

RolledCharacter roll_character(Race race, Profession profession, GoldBoxGame game,
                               RandomState &random)
{
    // Curse and Secret roll each 3d6 one higher (ovr018, createPlayer), and a
    // first level's Hit Dice the better of two.
    const bool curse = game != GoldBoxGame::pool_of_radiance;
    RolledCharacter rolled;
    rolled.starting_age = starting_age(race, profession, random, game);
    for (const auto ability : original_ability_order)
    {
        rolled.rolls[ability] =
            best_of_six(racial_adjustments(race)[ability] + (curse ? 1 : 0), random);
    }
    rolled.strength_percentile = roll_die(random, 100);
    const auto classes = classes_of(profession);
    // Curse's characters start with platinum (starting_platinum) instead; in
    // Secret only a thief brings gold, 2d6, and in Pools of Darkness no one.
    if (game == GoldBoxGame::pools_of_darkness)
    {
        rolled.gold = 0;
    }
    else if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        rolled.gold =
            std::ranges::find(classes, C::thief) != classes.end() ? roll_dice(random, {2, 6}) : 0;
    }
    else
    {
        rolled.gold = curse ? 0 : starting_gold(profession, random);
    }
    for (std::size_t n = 0; n < classes.size(); ++n)
    {
        // Secret's first-level cleric rolls two dice, as a ranger does.
        const auto dice =
            game == GoldBoxGame::secret_of_the_silver_blades && classes[n] == C::cleric
                ? Dice{2, 8}
                : hit_dice[index(classes[n])];
        rolled.first_level_hit_dice[n] =
            curse ? better_of_two(dice, random)
                  : std::max(roll_dice(random, dice), dice.sides * 2 / 3);
    }
    for (const auto slot : classes)
    {
        for (int level = 2; level <= starting_reach(slot, profession, game); ++level)
        {
            rolled.training_hit_dice.push_back(
                better_of_two({1, hit_dice[index(slot)].sides}, random));
        }
    }
    return rolled;
}

int starting_experience(Profession profession, GoldBoxGame game)
{
    const auto classes = static_cast<int>(classes_of(profession).size());
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return 25000 / classes;
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return 3000 / classes;
    case GoldBoxGame::secret_of_the_silver_blades:
        // Its creation rounds a three-class character's share up.
        return classes == 3 ? 66667 : 200000 / classes;
    case GoldBoxGame::unlimited_adventures:
        // The kit's game record starts a new character with 40,001
        // experience, shared among its classes (docs/games/ua.md).
        return 40001 / classes;
    case GoldBoxGame::pools_of_darkness:
        // Its creation gives each count its own amount (CreateCharacterMenu).
        return classes == 1 ? 1500001 : classes == 2 ? 750000 : 500000;
    default:
        return 0;
    }
}

int starting_reach(ClassSlot slot, Profession profession, GoldBoxGame game)
{
    const int experience = starting_experience(profession, game);
    int level = 1;
    while (true)
    {
        const auto needed = experience_for_level(slot, level + 1, game);
        if (!needed || *needed > experience)
        {
            return level;
        }
        ++level;
    }
}

int starting_platinum(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return 300;
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return 35;
    case GoldBoxGame::unlimited_adventures:
    case GoldBoxGame::pools_of_darkness:
        return 200;
    default:
        return 0;
    }
}

namespace
{
// The magic-user spells Secret's creation teaches at levels 6 to 9 (its
// TrainingMenu, newCharTraining), by canonical numbers. Its level 6 set also
// names Cure Light Wounds, a cleric's spell no magic-user can cast; it is
// left out (docs/ORIGINAL-RULES.md, Bugs).
[[nodiscard]] std::vector<int> secret_starting_spells(int magic_user_level)
{
    switch (magic_user_level)
    {
    case 6:
        return {12, 15, 30, 34, 47, 51, 54};
    case 7:
        return {14, 16, 29, 35, 45, 48, 181};
    case 8:
        return {10, 9, 32, 55, 185};
    case 9:
        return {50, 52, 182, 188, 194};
    default:
        return {};
    }
}

// The magic-user spells Pools of Darkness's creation teaches once trained
// (its InitKnownSpells), by its own numbers: every spell of levels 1 to 3,
// then a set for each higher level the magic-user has slots of, and every
// spell with ninth-level slots, which OpenGoldBox does not keep yet.
[[nodiscard]] std::vector<int> darkness_starting_spells(const SlotRow &slots)
{
    std::vector<int> learned;
    for (const int number : castable_spells(GoldBoxGame::pools_of_darkness))
    {
        const auto *known = spell(number);
        if (known->caster_class == SpellClass::magic_user && known->level <= 3)
        {
            learned.push_back(number);
        }
    }
    const std::array<std::vector<int>, 4> sets{
        {{81, 82, 83, 84, 85, 87, 88}, {91, 92, 118, 119}, {111, 112, 113, 114}, {115, 117}}};
    for (std::size_t level = 4; level <= 7; ++level)
    {
        if (slots[level - 1] > 0)
        {
            for (const int own : sets[level - 4])
            {
                learned.push_back(canonical_spell(own, GoldBoxGame::pools_of_darkness));
            }
        }
    }
    return learned;
}

// The magic-user spells Curse's creation teaches at each level it trains to
// (ovr018 train_player, silent training): Magic Missile at 2; Stinking Cloud
// and Protection from Evil at 3; Knock at 4; Fireball at 5.
[[nodiscard]] std::vector<int> curse_starting_spells(int magic_user_level)
{
    switch (magic_user_level)
    {
    case 2:
        return {15};
    case 3:
        return {34, 16};
    case 4:
        return {31};
    case 5:
        return {47};
    default:
        return {};
    }
}
} // namespace

StartingTraining starting_training(Race race, Profession profession,
                                   const AbilityArray<int> &scores, const RolledCharacter &rolled,
                                   GoldBoxGame game)
{
    StartingTraining trained;
    trained.levels = first_levels(profession);
    const int experience = starting_experience(profession, game);
    const auto classes = classes_of(profession);
    // Where each class's dice start in rolled.training_hit_dice.
    std::array<std::size_t, 3> first_die{};
    for (std::size_t n = 1; n < classes.size(); ++n)
    {
        first_die[n] = first_die[n - 1] + static_cast<std::size_t>(
                                              starting_reach(classes[n - 1], profession, game) - 1);
    }
    while (true)
    {
        const auto ready = classes_ready_to_train(race, trained.levels, scores, experience, game);
        if (ready.empty())
        {
            if (game == GoldBoxGame::pools_of_darkness && level_in(trained.levels, C::magic_user))
            {
                trained.learned_spells = darkness_starting_spells(
                    spell_slots(C::magic_user, level_in(trained.levels, C::magic_user),
                                scores[Ability::wisdom], scores[Ability::intelligence], game));
            }
            return trained;
        }
        int rolled_dice = 0;
        for (const auto slot : ready)
        {
            const int level = ++level_in(trained.levels, slot);
            const auto n =
                static_cast<std::size_t>(std::ranges::find(classes, slot) - classes.begin());
            rolled_dice +=
                rolled.training_hit_dice.at(first_die[n] + static_cast<std::size_t>(level - 2));
            if (slot == C::magic_user && game != GoldBoxGame::pools_of_darkness)
            {
                const auto learned = game == GoldBoxGame::secret_of_the_silver_blades
                                         ? secret_starting_spells(level)
                                         : curse_starting_spells(level);
                for (const int spell : learned)
                {
                    trained.learned_spells.push_back(spell);
                }
            }
        }
        trained.hit_points +=
            training_hit_points(profession, rolled_dice, scores[Ability::constitution], game);
        trained.rolled_hit_points += training_rolled_hit_points(profession, rolled_dice);
    }
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

int first_level_hit_points(Profession profession, const RolledCharacter &rolled, int constitution,
                           GoldBoxGame game)
{
    const auto class_count = classes_of(profession).size();
    int total = 0;
    for (std::size_t n = 0; n < class_count; ++n)
    {
        total += rolled.first_level_hit_dice[n];
    }
    const int bonus = constitution_hit_point_bonus(profession, constitution, game, true);
    const int classes = static_cast<int>(class_count);
    if (secret_hit_points(game))
    {
        return std::max(1, total / classes) + bonus / classes;
    }
    return divided_among_classes(total, bonus, classes);
}

int first_level_rolled_hit_points(Profession profession, const RolledCharacter &rolled)
{
    const auto class_count = classes_of(profession).size();
    int total = 0;
    for (std::size_t n = 0; n < class_count; ++n)
    {
        total += rolled.first_level_hit_dice[n];
    }
    return total / static_cast<int>(class_count);
}

int starting_age(Race race, Profession profession, RandomState &random, GoldBoxGame game)
{
    const auto &ages = starting_ages[index(race)];
    const auto classes = classes_of(profession);
    if (classes.size() == 1)
    {
        if (classes.front() == C::monk)
        {
            throw std::invalid_argument("The original has no starting age for a monk");
        }
        const bool darkness = game == GoldBoxGame::pools_of_darkness;
        // Gateway's table gives a half-elf ranger its own row, and Pools of
        // Darkness's one of 22 years and no dice; Curse's has none, and
        // starts one aged 0 (docs/ORIGINAL-RULES.md, Bugs): a warrior's row
        // stands in.
        constexpr AgeDice gateway_half_elf_ranger{26, 3, 8};
        if (game == GoldBoxGame::gateway_to_the_savage_frontier && race == Race::half_elf &&
            classes.front() == C::ranger)
        {
            return gateway_half_elf_ranger.base +
                   roll_dice(random,
                             {gateway_half_elf_ranger.count, gateway_half_elf_ranger.sides});
        }
        const auto &row = ages[index(classes.front())];
        const auto dice = row.base > 0 ? row
                          : darkness   ? AgeDice{22, 0, 0}
                                       : ages[index(C::fighter)];
        // Pools of Darkness makes a single class ten years older (its
        // CreateCharacterMenu).
        const int older = darkness ? 10 : 0;
        return dice.base + roll_dice(random, {dice.count, dice.sides}) + older;
    }
    // A multi-class character is as old as one of its classes allows at most:
    // its cleric's, else its magic-user's, else its fighter's.
    const auto oldest = has_class(profession, C::cleric)       ? C::cleric
                        : has_class(profession, C::magic_user) ? C::magic_user
                                                               : C::fighter;
    const auto dice = ages[index(oldest)];
    return dice.base + dice.count * dice.sides;
}

int constitution_hit_point_bonus(Profession profession, int constitution, GoldBoxGame game,
                                 bool first_level)
{
    const int score = std::clamp(constitution, 3, 25);
    const int per_class = score == 3 ? -2 : score <= 6 ? -1 : score <= 14 ? 0 : score == 15 ? 1 : 2;
    const auto classes = classes_of(profession);
    if (game == GoldBoxGame::pool_of_radiance)
    {
        int bonus = per_class * static_cast<int>(classes.size());
        if (profession == P::fighter)
        {
            bonus += (score > 16 ? 1 : 0) + (score > 17 ? 1 : 0);
        }
        return bonus;
    }
    const int warrior_extra = score < 17    ? 0
                              : score == 17 ? 1
                              : score == 18 ? 2
                              : score <= 20 ? 3
                              : score <= 23 ? 4
                                            : 5;
    const auto warrior = [](ClassSlot slot)
    {
        return slot == C::fighter || slot == C::paladin || slot == C::ranger;
    };
    int bonus = 0;
    for (const auto slot : classes)
    {
        if (secret_hit_points(game))
        {
            // Each class's own bonus, once for each of the level's dice: a
            // ranger's first level rolls two, and in Secret a cleric's.
            const bool two_dice =
                slot == C::ranger ||
                (slot == C::cleric && game == GoldBoxGame::secret_of_the_silver_blades);
            const int dice = first_level && two_dice ? 2 : 1;
            bonus += (per_class + (warrior(slot) ? warrior_extra : 0)) * dice;
            continue;
        }
        // Curse's extra goes to a single fighter, paladin or ranger; its
        // first-level ranger's two dice double the bonus so far.
        bonus += per_class + (classes.size() == 1 && warrior(slot) ? warrior_extra : 0);
        if (first_level && slot == C::ranger)
        {
            bonus *= 2;
        }
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

std::array<int, 8> thief_skills(Race race, int thief_level, int dexterity, GoldBoxGame game)
{
    std::array<int, 8> skills{};
    if (thief_level < 1)
    {
        return skills;
    }
    const int top = game == GoldBoxGame::pool_of_radiance ? 9 : curse_tables(game) ? 12 : 18;
    const auto level = std::min(thief_level, top);
    auto base = thief_base[static_cast<std::size_t>(level - 1)];
    if (curse_tables(game) && level >= 11)
    {
        base[7] += 5;
    }
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

int thac0(const ClassLevels &levels, GoldBoxGame game)
{
    const bool curse = curse_tables(game);
    // Only the classes held count; with none, 20.
    int best = 40;
    bool held = false;
    for (std::size_t slot = 0; slot < class_slot_count; ++slot)
    {
        if (levels[slot] <= 0)
        {
            continue;
        }
        best = held ? best : 0;
        held = true;
        if (game == GoldBoxGame::secret_of_the_silver_blades)
        {
            best = std::max(
                best,
                secret_stored_thac0[slot][static_cast<std::size_t>(std::min(levels[slot], 18))]);
            continue;
        }
        if (game == GoldBoxGame::pools_of_darkness)
        {
            best = std::max(
                best,
                darkness_stored_thac0[slot][static_cast<std::size_t>(std::min(levels[slot], 21))]);
            continue;
        }
        const auto level = static_cast<std::size_t>(std::min(levels[slot], curse ? 12 : 10));
        best = std::max(best, curse ? curse_stored_thac0[slot][level] : stored_thac0[slot][level]);
    }
    return 60 - best;
}

std::array<int, 5> saving_throws(const ClassLevels &levels, GoldBoxGame game)
{
    const bool curse = curse_tables(game);
    std::array<int, 5> best{20, 20, 20, 20, 20};
    for (std::size_t slot = 0; slot < class_slot_count; ++slot)
    {
        if (levels[slot] <= 0)
        {
            continue;
        }
        if (game == GoldBoxGame::pools_of_darkness && levels[slot] > 18)
        {
            const auto &row =
                darkness_high_saves[slot]
                                   [static_cast<std::size_t>(std::min(levels[slot], 21) - 19)];
            for (std::size_t save = 0; save < best.size(); ++save)
            {
                best[save] = std::min(best[save], row[save]);
            }
            continue;
        }
        if (game == GoldBoxGame::secret_of_the_silver_blades ||
            game == GoldBoxGame::pools_of_darkness)
        {
            const auto &row =
                secret_saves[slot][static_cast<std::size_t>(std::min(levels[slot], 18) - 1)];
            for (std::size_t save = 0; save < best.size(); ++save)
            {
                best[save] = std::min(best[save], row[save]);
            }
            continue;
        }
        const auto class_slot = static_cast<ClassSlot>(slot);
        const auto row = class_saves(class_slot, std::min(levels[slot], curse ? 12 : 9));
        // Curse's paladin saves 2 better than a fighter of its level.
        const int paladin_bonus = curse && class_slot == C::paladin ? 2 : 0;
        for (std::size_t save = 0; save < best.size(); ++save)
        {
            best[save] = std::min(best[save], row[save] - paladin_bonus);
        }
    }
    return best;
}

std::optional<int> experience_for_level(ClassSlot slot, int level, GoldBoxGame game)
{
    if (level == 1)
    {
        return 0;
    }
    if (game == GoldBoxGame::pools_of_darkness && level >= 11)
    {
        const auto at = index(slot);
        if (level > darkness_highest_level || darkness_level_11[at] == 0)
        {
            return std::nullopt;
        }
        return darkness_level_11[at] + (level - 11) * darkness_per_level[at];
    }
    if (game == GoldBoxGame::secret_of_the_silver_blades || game == GoldBoxGame::pools_of_darkness)
    {
        if (level < 2 || level > 18)
        {
            return std::nullopt;
        }
        const int needed =
            experience_table_secret[index(slot)][static_cast<std::size_t>(level - 2)];
        return needed == 0 ? std::nullopt : std::optional<int>{needed};
    }
    const bool curse = curse_tables(game);
    if (level < 2 || level > (curse ? 13 : 9))
    {
        return std::nullopt;
    }
    const auto at = static_cast<std::size_t>(level - 2);
    const int needed =
        curse ? experience_table_curse[index(slot)][at] : experience_table_pool[index(slot)][at];
    return needed == 0 ? std::nullopt : std::optional<int>{needed};
}

std::vector<int> known_spells(const ClassLevels &levels, int wisdom, std::span<const int> learned,
                              GoldBoxGame game)
{
    // Seventh-level slots need Wisdom 18 whatever the Intelligence.
    const auto slots = caster_slots(levels, wisdom, 9, game);
    // A ranger's own slots, which Secret's ranger knows every spell of.
    ClassLevels ranger_alone{};
    ranger_alone[static_cast<std::size_t>(ClassSlot::ranger)] = level_in(levels, ClassSlot::ranger);
    const auto ranger_slots = caster_slots(ranger_alone, wisdom, 9, game);
    const bool secret = game == GoldBoxGame::secret_of_the_silver_blades;
    // Detect Magic, Read Magic, Sleep, and Shield (Pool of Radiance) or
    // Enlarge (Curse).
    // Pools of Darkness's too knows Enlarge.
    const bool enlarge = curse_tables(game) || game == GoldBoxGame::pools_of_darkness;
    const std::array<int, 4> first_magic_user_spells{11, enlarge ? 12 : 19, 18, 21};
    const bool magic_user = level_in(levels, ClassSlot::magic_user) > 0;
    std::vector<int> known;
    for (const int number : castable_spells(game))
    {
        const auto *known_spell = spell(number);
        const auto at = static_cast<std::size_t>(std::clamp(known_spell->level, 1, 7) - 1);
        const bool cleric_spell = known_spell->caster_class == SpellClass::cleric &&
                                  known_spell->level <= 7 && slots.cleric[at] > 0;
        const bool druid_spell = known_spell->caster_class == SpellClass::druid &&
                                 known_spell->level <= 7 && slots.druid[at] > 0;
        const bool magic_user_spell =
            known_spell->caster_class == SpellClass::magic_user &&
            ((magic_user && (std::ranges::find(first_magic_user_spells, number) !=
                                 first_magic_user_spells.end() ||
                             std::ranges::find(learned, number) != learned.end())) ||
             (secret && known_spell->level <= 7 && ranger_slots.magic_user[at] > 0));
        if (cleric_spell || druid_spell || magic_user_spell)
        {
            known.push_back(number);
        }
    }
    return known;
}

std::vector<ClassSlot> classes_ready_to_train(Race race, const ClassLevels &levels,
                                              const AbilityArray<int> &scores, int experience,
                                              GoldBoxGame game)
{
    std::vector<ClassSlot> ready;
    for (std::size_t at = 0; at < class_slot_count; ++at)
    {
        const auto slot = static_cast<ClassSlot>(at);
        const int level = levels[at];
        if (level == 0)
        {
            continue;
        }
        const auto limit = racial_level_limit(race, slot, scores, game);
        const auto needed = experience_for_level(slot, level + 1, game);
        const bool capped =
            game == GoldBoxGame::gateway_to_the_savage_frontier && level >= gateway_level_caps[at];
        if (!capped && (!limit || level < *limit) && needed && *needed <= experience)
        {
            ready.push_back(slot);
        }
    }
    return ready;
}

int experience_after_training(const ClassLevels &levels, std::span<const ClassSlot> trained,
                              int experience, GoldBoxGame game)
{
    // The original keeps the most of what the trained classes allow.
    int cap = 0;
    for (const auto slot : trained)
    {
        const auto after_next = experience_for_level(slot, level_in(levels, slot) + 2, game);
        if (after_next && experience >= *after_next)
        {
            cap = std::max(cap, *after_next - 1);
        }
    }
    return cap > 0 ? cap : experience;
}

int roll_training_hit_dice(const ClassLevels &trained_levels, std::span<const ClassSlot> trained,
                           GoldBoxGame game, RandomState &random)
{
    int total = 0;
    for (const auto slot : trained)
    {
        if (game == GoldBoxGame::pool_of_radiance)
        {
            total += roll_dice(random, hit_dice[index(slot)]);
            continue;
        }
        // Curse (and Secret, whose code is the same) adds the fixed amount to the other classes'
        // dice; its own code put it in place of them (docs/ORIGINAL-RULES.md, Bugs).
        if (level_in(trained_levels, slot) >= curse_hit_dice_levels[index(slot)])
        {
            total += curse_fixed_hit_points[index(slot)];
            continue;
        }
        total += better_of_two({1, hit_dice[index(slot)].sides}, random);
    }
    return total;
}

int training_hit_points(Profession profession, int rolled, int constitution, GoldBoxGame game)
{
    const int classes = static_cast<int>(classes_of(profession).size());
    const int bonus = constitution_hit_point_bonus(profession, constitution, game);
    if (secret_hit_points(game))
    {
        return std::max(1, rolled / classes) + bonus / classes;
    }
    return std::max(1, (rolled + bonus) / classes);
}

int training_rolled_hit_points(Profession profession, int rolled)
{
    return std::max(1, rolled / static_cast<int>(classes_of(profession).size()));
}

// The original sums a bonus a level that is not the one training adds: a
// fighter's level counts the Constitution above 14, any other class's 1 at
// 14 or 15 and 2 above; the division is kept as the original reckons it.
int raised_hit_point_loss(const ClassLevels &levels, int constitution, int hit_points,
                          int rolled_hit_points)
{
    if (constitution < 14)
    {
        return 0;
    }
    int loss = std::max(0, hit_points - rolled_hit_points);
    int bonus = 0;
    for (std::size_t at = 0; at < levels.size(); ++at)
    {
        const int level = levels[at];
        if (level <= 0)
        {
            continue;
        }
        if (static_cast<ClassSlot>(at) == C::fighter)
        {
            bonus += (constitution - 14) * level;
        }
        else
        {
            bonus += (constitution > 15 ? 2 : 1) * level;
        }
    }
    if (bonus > 0)
    {
        loss /= bonus;
    }
    const bool fighter = level_in(levels, C::fighter) > 0;
    return constitution < 17 || fighter ? loss : 0;
}

int experience_share(Profession profession, const AbilityArray<int> &scores, int award,
                     GoldBoxGame game)
{
    const auto bonus_above_15 = [&](Ability prime)
    {
        return scores[prime] > 15 ? award + award / 10 : award;
    };
    // A paladin's and a ranger's bonus needs every one of its prime scores
    // (postcbt.pas, AwardExperience).
    const auto bonus_for_all = [&](std::initializer_list<Ability> primes)
    {
        const bool all = std::ranges::all_of(primes,
                                             [&](Ability prime)
                                             {
                                                 return scores[prime] > 15;
                                             });
        return all ? award + award / 10 : award;
    };
    switch (profession)
    {
    case P::cleric:
        // Secret's and Pools of Darkness's cleric gains it from Wisdom 15.
        return (game == GoldBoxGame::secret_of_the_silver_blades ||
                game == GoldBoxGame::pools_of_darkness) &&
                       scores[Ability::wisdom] == 15
                   ? award + award / 10
                   : bonus_above_15(Ability::wisdom);
    case P::paladin:
        return bonus_for_all({Ability::strength, Ability::wisdom});
    case P::ranger:
        return bonus_for_all({Ability::strength, Ability::intelligence, Ability::wisdom});
    case P::fighter:
        return bonus_above_15(Ability::strength);
    case P::magic_user:
        return bonus_above_15(Ability::intelligence);
    case P::thief:
        return bonus_above_15(Ability::dexterity);
    default:
        return award / static_cast<int>(classes_of(profession).size());
    }
}

std::optional<int> racial_level_limit(Race race, ClassSlot slot, const AbilityArray<int> &scores,
                                      GoldBoxGame game)
{
    const int strength = scores[Ability::strength];
    const auto by_score = [](int score, int below_17, int at_17, int above)
    {
        return score < 17 ? below_17 : score == 17 ? at_17 : above;
    };
    const auto by_strength = [&](int below_17, int at_17, int above)
    {
        return by_score(strength, below_17, at_17, above);
    };
    // Secret's limits are Curse's.
    const bool curse = game != GoldBoxGame::pool_of_radiance;
    if (race == Race::half_elf && slot == C::cleric)
    {
        return 5;
    }
    // Curse's and Secret's magic-users: an elf's limit and a half-elf's
    // follow Intelligence.
    if (curse && slot == C::magic_user)
    {
        const int intelligence = scores[Ability::intelligence];
        switch (race)
        {
        case Race::elf:
            return by_score(intelligence, 9, 10, 11);
        case Race::half_elf:
            return by_score(intelligence, 6, 7, 8);
        default:
            return std::nullopt;
        }
    }
    if (curse && race == Race::half_elf && slot == C::ranger)
    {
        return by_strength(6, 7, 8);
    }
    if (slot != C::fighter)
    {
        return std::nullopt;
    }
    switch (race)
    {
    case Race::dwarf:
        // In Pool of Radiance a dwarf of 18 Strength is limited only by the
        // experience table; in Curse it reaches level 9.
        if (strength >= 18)
        {
            return curse ? std::optional<int>{9} : std::nullopt;
        }
        return strength < 17 ? 7 : 8;
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

SpellSlots spell_slots(ClassSlot slot, int level, int wisdom, int intelligence, GoldBoxGame game)
{
    if ((slot != C::cleric && slot != C::magic_user) || level < 1)
    {
        return {};
    }
    const bool darkness = game == GoldBoxGame::pools_of_darkness;
    const bool secret = game == GoldBoxGame::secret_of_the_silver_blades || darkness;
    const auto &table = slot == C::cleric ? cleric_slots : magic_user_slots;
    const auto &darkness_table =
        slot == C::cleric ? darkness_cleric_slots : darkness_magic_user_slots;
    SlotRow slots = level == 1 ? SlotRow{1, 0, 0, 0, 0, 0, 0}
                    : darkness && level > 15
                        ? darkness_table[static_cast<std::size_t>(std::min(level, 29) - 16)]
                        : table[static_cast<std::size_t>(std::min(level, secret ? 15 : 12) - 2)];
    if (!secret)
    {
        slots[5] = slots[6] = 0;
    }
    else
    {
        // Sixth- and seventh-level slots need the wit for them: Wisdom 17
        // and 18 a cleric, Intelligence 12 and 14 a magic-user (classcal.pas).
        const auto [sixth, seventh] = slot == C::cleric
                                          ? std::pair{wisdom >= 17, wisdom >= 18}
                                          : std::pair{intelligence >= 12, intelligence >= 14};
        slots[5] = sixth ? slots[5] : 0;
        slots[6] = seventh ? slots[6] : 0;
    }
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
        add(3, wisdom > 17);
    }
    return slots;
}

namespace
{
// The cleric slots a paladin gains at levels 9 to 15, of spell levels 1 to 4
// (Secret's classProgression; Curse's classExperience to level 12).
constexpr std::array<std::array<int, 4>, 7> paladin_slot_gains{{{1, 0, 0, 0},
                                                                {1, 0, 0, 0},
                                                                {0, 1, 0, 0},
                                                                {0, 1, 0, 0},
                                                                {0, 0, 1, 0},
                                                                {1, 0, 0, 0},
                                                                {0, 0, 0, 1}}};
// The slots a ranger gains at levels 8 to 15: druid spell levels 1 to 3,
// then magic-user spell levels 1 and 2.
constexpr std::array<std::array<int, 5>, 8> ranger_slot_gains{{{1, 0, 0, 0, 0},
                                                               {0, 0, 0, 1, 0},
                                                               {1, 0, 0, 0, 0},
                                                               {0, 0, 0, 1, 0},
                                                               {0, 1, 0, 0, 0},
                                                               {0, 0, 0, 0, 1},
                                                               {0, 1, 0, 0, 0},
                                                               {0, 0, 0, 0, 1}}};
} // namespace

CasterSlots caster_slots(const ClassLevels &levels, int wisdom, int intelligence, GoldBoxGame game)
{
    CasterSlots slots;
    slots.cleric = spell_slots(C::cleric, level_in(levels, C::cleric), wisdom, intelligence, game);
    slots.magic_user =
        spell_slots(C::magic_user, level_in(levels, C::magic_user), wisdom, intelligence, game);
    // Their tables end with the game's levels: Curse's at 12, Secret's at 15.
    const int top = game == GoldBoxGame::secret_of_the_silver_blades ? 15 : 12;
    for (int level = 9; level <= std::min(level_in(levels, C::paladin), top); ++level)
    {
        const auto &gains = paladin_slot_gains[static_cast<std::size_t>(level - 9)];
        for (std::size_t at = 0; at < gains.size(); ++at)
        {
            slots.cleric[at] += gains[at];
        }
    }
    for (int level = 8; level <= std::min(level_in(levels, C::ranger), top); ++level)
    {
        const auto &gains = ranger_slot_gains[static_cast<std::size_t>(level - 8)];
        for (std::size_t at = 0; at < 3; ++at)
        {
            slots.druid[at] += gains[at];
        }
        slots.magic_user[0] += gains[3];
        slots.magic_user[1] += gains[4];
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
