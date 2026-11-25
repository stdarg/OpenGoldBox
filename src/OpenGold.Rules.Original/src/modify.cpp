#include "dark_queen_rules.h"
#include "opengold/original_rules.h"
#include "opengold/buck_rogers_rules.h"
#include <algorithm>
#include <array>
#include <initializer_list>

// Modify Character as each game's chargen.pas (ModifyCharacterMenu) and the
// kit's modify.c work it out (docs/ORIGINAL-RULES.md, Modify Character). The
// rules are the originals'; the code is OpenGoldBox's own.
namespace opengold::original
{
namespace
{
using rules::Ability;
using rules::ModifyRow;
using rules::ModifyStep;
using C = ClassSlot;

[[nodiscard]] constexpr std::size_t at(ClassSlot slot) noexcept
{
    return static_cast<std::size_t>(slot);
}

// How each game works out Modify's Hit Points.
enum class HitPointRules
{
    // Pool of Radiance: the dice's lowest and highest, the Constitution's
    // bonus for one level added once.
    pool,
    // Curse and Gateway: the bonus once at the lowest, each class's bonus at
    // every level at the highest, a fixed amount past its Hit Dice.
    curse,
    // Secret: Curse's, each class with its own bonus.
    secret,
    // Pools of Darkness, Treasures and the kit: the bonus at every level at
    // the lowest too, where it adds only when it is above 0.
    darkness
};

[[nodiscard]] constexpr HitPointRules hit_point_rules(GoldBoxGame game) noexcept
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return HitPointRules::pool;
    case GoldBoxGame::curse_of_the_azure_bonds:
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return HitPointRules::curse;
    case GoldBoxGame::secret_of_the_silver_blades:
        return HitPointRules::secret;
    default:
        return HitPointRules::darkness;
    }
}

// The level at which each class stops rolling Hit Dice (Curse's
// constitutionBonusLevelCaps, the later games' firstFixedHpLevel, the kit's
// hit_dice_level_limit). Only Curse and Gateway have monks, at 19.
[[nodiscard]] constexpr std::array<int, class_slot_count> hit_dice_caps(GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::unlimited_adventures)
    {
        return {10, 10, 10, 10, 11, 12, 11, 19};
    }
    return {10, 15, 10, 10, 11, 12, 11, 19};
}

constexpr std::array<int, class_slot_count> hit_die_sides{8, 8, 10, 10, 8, 4, 6, 4};

[[nodiscard]] constexpr bool warrior(ClassSlot slot) noexcept
{
    return slot == C::fighter || slot == C::paladin || slot == C::ranger;
}

// The Hit Points a level gets for its Constitution (conBonusTbl).
[[nodiscard]] constexpr int table_bonus(int constitution) noexcept
{
    return constitution <= 3    ? -2
           : constitution <= 6  ? -1
           : constitution <= 14 ? 0
           : constitution == 15 ? 1
                                : 2;
}

// What a fighter, paladin or ranger gets more from 17 up (ConHpBonus).
[[nodiscard]] constexpr int warrior_extra(int constitution) noexcept
{
    return constitution < 17    ? 0
           : constitution == 17 ? 1
           : constitution == 18 ? 2
           : constitution <= 20 ? 3
           : constitution <= 23 ? 4
                                : 5;
}

// A class's Hit Points a level for its Constitution, besides the die
// (ClassConAdj): a warrior's 17 to 19 less 14, anyone else's 2. The original
// reads nothing above 19, which no race reaches; it counts as 19.
[[nodiscard]] constexpr int class_constitution_adjustment(ClassSlot slot, int constitution) noexcept
{
    if (constitution <= 16)
    {
        return table_bonus(constitution);
    }
    return warrior(slot) ? std::min(constitution, 19) - 14 : 2;
}

// A class's own bonus, its warrior's extra by the class (Secret's, Pools of
// Darkness's and the kit's ConHpBonus).
[[nodiscard]] constexpr int class_bonus(ClassSlot slot, int constitution) noexcept
{
    return table_bonus(constitution) + (warrior(slot) ? warrior_extra(constitution) : 0);
}

// Each class the character has levels in, with its level.
template <class Visit> void each_class(const ClassLevels &levels, const Visit &visit)
{
    for (std::size_t n = 0; n < class_slot_count; ++n)
    {
        if (levels[n] > 0)
        {
            visit(static_cast<ClassSlot>(n), levels[n]);
        }
    }
}

[[nodiscard]] int class_count(const ClassLevels &levels)
{
    return static_cast<int>(std::ranges::count_if(levels,
                                                  [](int level)
                                                  {
                                                      return level > 0;
                                                  }));
}

// A total and a Constitution bonus divided among the classes, 1 when a
// negative bonus would leave too little.
[[nodiscard]] int divided(int total, int bonus, int classes)
{
    if (bonus < 0 && total <= -bonus + classes)
    {
        return 1;
    }
    return (total + bonus) / classes;
}

// The Constitution bonus Pool of Radiance's and Curse's limits add once
// (ConHpBonus): Pool of Radiance's each class's table bonus, a single
// fighter's 1 more at 17 and 2 at 18; Curse's for each class below its Hit
// Dice's end, a single fighter's, paladin's or ranger's extra, and a
// first-level ranger's bonus so far counted twice.
[[nodiscard]] int level_bonus(const ModifiedCharacter &character, int constitution,
                              GoldBoxGame game)
{
    const auto profession = character.profession;
    if (hit_point_rules(game) == HitPointRules::pool)
    {
        const int fighter_extra =
            profession == Profession::fighter ? (constitution > 16) + (constitution > 17) : 0;
        return table_bonus(constitution) * class_count(character.levels) + fighter_extra;
    }
    if (hit_point_rules(game) == HitPointRules::secret)
    {
        int bonus = 0;
        each_class(character.levels,
                   [&](ClassSlot slot, int)
                   {
                       bonus += class_bonus(slot, constitution);
                   });
        return bonus;
    }
    const bool single_warrior = profession == Profession::fighter ||
                                profession == Profession::paladin ||
                                profession == Profession::ranger;
    const auto caps = hit_dice_caps(game);
    int bonus = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   if (level >= caps[at(slot)])
                   {
                       return;
                   }
                   bonus += table_bonus(constitution) +
                            (single_warrior ? warrior_extra(constitution) : 0);
                   if (slot == C::ranger && level == 1)
                   {
                       bonus *= 2;
                   }
               });
    return bonus;
}

// The levels the lowest Hit Points count, one a level and a ranger's or
// monk's two first-level dice.
[[nodiscard]] int levels_counted(const ClassLevels &levels, bool monks)
{
    int total = 0;
    each_class(levels,
               [&](ClassSlot slot, int level)
               {
                   total += level + ((slot == C::ranger || (monks && slot == C::monk)) ? 1 : 0);
               });
    return total;
}

// Pools of Darkness's, Treasures' and the kit's lowest (HpMaximum,
// hp_minimum): a point a level below a class's Hit Dice's end and fixed
// amounts past it, with the classes' bonuses at each level when they come
// to more than 0.
[[nodiscard]] int darkness_minimum(const ModifiedCharacter &character, int constitution,
                                   GoldBoxGame game)
{
    const auto caps = hit_dice_caps(game);
    int total = 0, bonus = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   const int cap = caps[at(slot)];
                   if (level < cap)
                   {
                       total += level + (slot == C::ranger ? 1 : 0);
                       bonus += class_bonus(slot, constitution) * level;
                       return;
                   }
                   const int over = level - cap + 1;
                   switch (slot)
                   {
                   case C::cleric:
                       total += over * 2 + 9;
                       break;
                   case C::fighter:
                   case C::paladin:
                       total += over * 3 + 9;
                       break;
                   case C::ranger:
                       total += over * 2 + 11;
                       break;
                   case C::magic_user:
                       total += over + 11;
                       break;
                   case C::thief:
                       total += over * 2 + 10;
                       break;
                   default:
                       break;
                   }
                   bonus += class_bonus(slot, constitution) * (cap - 1);
               });
    return (total + std::max(bonus, 0)) / class_count(character.levels);
}

// Curse's, Gateway's and Secret's highest (HpMaximum): each class's die and
// Constitution at each level below its Hit Dice's end, and from there a fixed
// amount, which the original put in place of the other classes' (Bugs).
[[nodiscard]] int curse_maximum(const ModifiedCharacter &character, int constitution,
                                GoldBoxGame game)
{
    const bool secret = game == GoldBoxGame::secret_of_the_silver_blades;
    const auto caps = hit_dice_caps(game);
    int total = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   const int cap = caps[at(slot)];
                   if (level < cap)
                   {
                       const int dice = level + ((slot == C::ranger || slot == C::monk) ? 1 : 0);
                       total += (hit_die_sides[at(slot)] +
                                 class_constitution_adjustment(slot, constitution)) *
                                dice;
                       return;
                   }
                   const int over = level - cap + 1;
                   switch (slot)
                   {
                   case C::cleric:
                       total += over * 2 + 72;
                       break;
                   case C::druid:
                       total += 112;
                       break;
                   case C::fighter:
                   case C::paladin:
                       total += secret ? over * 2 + 88 : over * 3 + 90;
                       break;
                   case C::ranger:
                       total += over * 2 + 88;
                       break;
                   case C::magic_user:
                       total += over + 44;
                       break;
                   case C::thief:
                       total += over * 2 + 60;
                       break;
                   case C::monk:
                       total += 72;
                       break;
                   }
               });
    return total / class_count(character.levels);
}

// Pools of Darkness's, Treasures' and the kit's highest (HitPointCap,
// hp_floor): each class's die and Constitution at each level below its Hit
// Dice's end; past it those levels, a ranger's extra die, and 2 a level (a
// fighter's or paladin's 3, a magic-user's 1).
[[nodiscard]] int darkness_maximum(const ModifiedCharacter &character, int constitution,
                                   GoldBoxGame game)
{
    const auto caps = hit_dice_caps(game);
    int total = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   const int cap = caps[at(slot)];
                   const int die =
                       hit_die_sides[at(slot)] + class_constitution_adjustment(slot, constitution);
                   if (level < cap)
                   {
                       total += die * (level + (slot == C::ranger ? 1 : 0));
                       return;
                   }
                   const int over = level - cap + 1;
                   const int dice = die * (cap - 1) + (slot == C::ranger ? die : 0);
                   switch (slot)
                   {
                   case C::cleric:
                   case C::ranger:
                   case C::thief:
                       total += dice + over * 2;
                       break;
                   case C::fighter:
                   case C::paladin:
                       total += dice + over * 3;
                       break;
                   case C::magic_user:
                       total += dice + over;
                       break;
                   default:
                       break;
                   }
               });
    return total / class_count(character.levels);
}

[[nodiscard]] bool one_of(int experience, std::initializer_list<int> values)
{
    return std::ranges::find(values, experience) != values.end();
}

[[nodiscard]] bool strength_warrior(const ClassLevels &levels, GoldBoxGame game)
{
    const auto has = [&](ClassSlot slot)
    {
        return levels[at(slot)] > 0;
    };
    if (game == GoldBoxGame::pool_of_radiance)
    {
        return has(C::fighter);
    }
    return has(C::fighter) || has(C::paladin) || has(C::ranger) ||
           (krynn_game(game) && has(C::monk));
}

int champions_constitution(ClassSlot slot, int constitution)
{
    const bool warrior =
        slot == C::fighter || slot == C::ranger || slot == C::monk || slot == C::paladin;
    return table_bonus(constitution) + (warrior ? warrior_extra(constitution) : 0);
}

int champions_maximum(const ModifiedCharacter &character, int constitution)
{
    static constexpr std::array<int, 8> caps{10, 15, 10, 10, 11, 12, 11, 10};
    int total = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   const auto at_slot = at(slot);
                   const bool extra = slot == C::ranger || slot == C::monk ||
                                      (slot == C::cleric && character.orders.deity <= 4);
                   const int dice = std::min(level, caps[at_slot] - 1) + (extra ? 1 : 0);
                   const int sides = slot == C::monk ? 10 : hit_die_sides[at_slot];
                   const int fixed = slot == C::fighter || slot == C::monk || slot == C::paladin ? 3
                                     : slot == C::magic_user ? 1
                                                             : 2;
                   total += dice * (sides + champions_constitution(slot, constitution)) +
                            std::max(0, level - caps[at_slot] + 1) * fixed;
               });
    return std::max(1, total / class_count(character.levels));
}

[[nodiscard]] ModifiedNumbers step_hit_points(const ModifiedNumbers &numbers,
                                              const ModifiedCharacter &character, ModifyStep step,
                                              GoldBoxGame game)
{
    auto next = numbers;
    const int constitution = numbers.scores[Ability::constitution];
    // A step never moves the other way: Hit Points creation left above the
    // highest (or below the lowest) stay where they are (Bugs).
    if (step == ModifyStep::lower)
    {
        next.hit_points = std::min(
            next.hit_points,
            std::max(next.hit_points - 1, modify_hit_point_minimum(character, constitution, game)));
    }
    else
    {
        next.hit_points = std::max(
            next.hit_points,
            std::min(next.hit_points + 1, modify_hit_point_maximum(character, constitution, game)));
    }
    return next;
}
} // namespace

bool modify_allowed(int experience, GoldBoxGame game, int design_start)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return experience == 0;
    case GoldBoxGame::curse_of_the_azure_bonds:
        return one_of(experience, {0, 8333, 12500, 25000});
    case GoldBoxGame::secret_of_the_silver_blades:
        return one_of(experience, {0, 66667, 100000, 200000});
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return one_of(experience, {0, 1000, 1500, 3000});
    case GoldBoxGame::pools_of_darkness:
        return one_of(experience, {500000, 750000, 1500001});
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return one_of(experience, {7533, 11300, 22600});
    case GoldBoxGame::unlimited_adventures:
        return one_of(experience, {design_start, design_start / 2, design_start / 3});
    case GoldBoxGame::champions_of_krynn:
        // CharGen.ModifyCharacter's creation/import experience values.
        return one_of(experience, {1500, 1251, 2000, 2001, 2251, 2500, 5000});
    case GoldBoxGame::death_knights_of_krynn:
        return one_of(experience, {210003, 105002, 70001});
    case GoldBoxGame::countdown_to_doomsday:
    case GoldBoxGame::matrix_cubed:
        // A career's starting experience; buck_rogers_modifiable checks the
        // character's own career, as chargen.pas ModifyCharacterMenu does.
        return std::ranges::any_of(
            std::array{Profession::rocket_jock, Profession::medic, Profession::warrior,
                       Profession::engineer, Profession::rogue},
            [&](Profession career)
            {
                return career_experience_for_level(career, buck_rogers_starting_level(game),
                                                   game) == static_cast<std::uint32_t>(experience);
            });
    case GoldBoxGame::dark_queen_of_krynn:
        return one_of(experience, {1000001, 500000, 333333});
    }
    return false;
}

int modify_hit_point_minimum(const ModifiedCharacter &character, int constitution, GoldBoxGame game)
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return dark_queen_hit_point_minimum(character, constitution);
    }
    if (krynn_game(game))
    {
        int dice = 0, bonus = 0;
        each_class(character.levels,
                   [&](ClassSlot slot, int level)
                   {
                       dice += level + (slot == C::ranger || slot == C::monk ? 1 : 0);
                       bonus += champions_constitution(slot, constitution);
                   });
        return divided(dice, bonus, class_count(character.levels));
    }
    if (hit_point_rules(game) == HitPointRules::darkness)
    {
        return darkness_minimum(character, constitution, game);
    }
    const bool monks = hit_point_rules(game) != HitPointRules::secret;
    return divided(levels_counted(character.levels, monks),
                   level_bonus(character, constitution, game), class_count(character.levels));
}

int modify_hit_point_maximum(const ModifiedCharacter &character, int constitution, GoldBoxGame game)
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return dark_queen_hit_point_maximum(character, constitution);
    }
    if (krynn_game(game))
    {
        return champions_maximum(character, constitution);
    }
    switch (hit_point_rules(game))
    {
    case HitPointRules::pool:
    {
        int dice = 0;
        each_class(character.levels,
                   [&](ClassSlot slot, int level)
                   {
                       const int count = level + ((slot == C::ranger || slot == C::monk) ? 1 : 0);
                       dice += hit_die_sides[at(slot)] * count;
                   });
        return divided(dice, level_bonus(character, constitution, game),
                       class_count(character.levels));
    }
    case HitPointRules::curse:
    case HitPointRules::secret:
        return curse_maximum(character, constitution, game);
    case HitPointRules::darkness:
        return darkness_maximum(character, constitution, game);
    }
    return 0;
}

ModifiedNumbers modify_step(const ModifiedNumbers &numbers, const ModifiedCharacter &character,
                            ModifyRow row, ModifyStep step, GoldBoxGame game)
{
    if (row == ModifyRow::hit_points)
    {
        return step_hit_points(numbers, character, step, game);
    }
    const auto ability = static_cast<Ability>(row);
    const auto limits = ability_limits(character.race, character.gender, game);
    const auto minimums = class_minimums(character.profession, game);
    auto next = numbers;
    auto &score = next.scores[ability];
    if (step == ModifyStep::lower)
    {
        if (ability == Ability::strength && next.exceptional_strength > 0)
        {
            --next.exceptional_strength;
            return next;
        }
        // Pool of Radiance sent a score below its class's minimum to its
        // race's, lower still (Bugs).
        score = std::max({score - 1, limits.minimum[ability], minimums[ability]});
    }
    else if (score + 1 > limits.maximum[ability])
    {
        if (ability == Ability::strength && score + 1 > 18 &&
            strength_warrior(character.levels, game) &&
            limits.exceptional_strength_maximum > next.exceptional_strength)
        {
            ++next.exceptional_strength;
        }
        score = limits.maximum[ability];
    }
    else
    {
        ++score;
    }
    // A lower Constitution brings the Hit Points down to its highest, a
    // higher one up to its lowest, as the original does.
    if (ability == Ability::constitution && step == ModifyStep::lower)
    {
        next.hit_points =
            std::min(next.hit_points, modify_hit_point_maximum(character, score, game));
    }
    if (ability == Ability::constitution && step == ModifyStep::raise)
    {
        next.hit_points =
            std::max(next.hit_points, modify_hit_point_minimum(character, score, game));
    }
    return next;
}

int modified_rolled_hit_points(const ModifiedCharacter &character, int constitution, int hit_points,
                               int rolled_before, GoldBoxGame game)
{
    if (krynn_game(game))
    {
        int bonus = 0;
        each_class(character.levels,
                   [&](ClassSlot slot, int level)
                   {
                       const bool extra = slot == C::ranger || slot == C::monk ||
                                          (slot == C::cleric && character.orders.deity <= 4);
                       bonus +=
                           (level + (extra ? 1 : 0)) * champions_constitution(slot, constitution);
                   });
        return std::max(0, hit_points - bonus / class_count(character.levels));
    }
    if (game == GoldBoxGame::pool_of_radiance)
    {
        return rolled_before;
    }
    const auto caps = hit_dice_caps(game);
    const bool kit = game == GoldBoxGame::unlimited_adventures;
    int share = 0;
    each_class(character.levels,
               [&](ClassSlot slot, int level)
               {
                   const int cap = caps[at(slot)];
                   const int adjustment = class_constitution_adjustment(slot, constitution);
                   if (level < cap)
                   {
                       share += adjustment * (level + (slot == C::ranger ? 1 : 0));
                   }
                   else
                   {
                       // The kit counts a ranger's extra die past the end too.
                       share += adjustment * (cap - 1 + (kit && slot == C::ranger ? 1 : 0));
                   }
               });
    return std::max(0, hit_points - share / class_count(character.levels));
}
} // namespace opengold::original
