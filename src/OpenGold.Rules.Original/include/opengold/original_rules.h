#ifndef OPENGOLD_ORIGINAL_RULES_H
#define OPENGOLD_ORIGINAL_RULES_H
#include "opengold/gold_box_game.h"
#include "opengold/random_state.h"
#include "opengold/rules.h"
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The original Gold Box rules: AD&D 1st edition as Pool of Radiance applied
// them, and Curse of the Azure Bonds after it with its own tables where the
// two differ, each game named by the GoldBoxGame it is (docs/ORIGINAL-RULES.md).
// The tables are the originals'; the code is OpenGoldBox's own.
namespace opengold::original
{
// In the original's order, which its scripts read as race codes 1 to 7.
enum class Race : unsigned
{
    dwarf,
    elf,
    gnome,
    half_elf,
    halfling,
    half_orc,
    human
};

enum class Gender : unsigned
{
    male,
    female
};

// The classes a character can hold levels in, in the original's order.
enum class ClassSlot : unsigned
{
    cleric,
    druid,
    fighter,
    paladin,
    ranger,
    magic_user,
    thief,
    monk
};

inline constexpr std::size_t class_slot_count = 8;

// What a character is at creation: a single class or a multi-class, the
// original's seventeen in its order.
enum class Profession : unsigned
{
    cleric,
    druid,
    fighter,
    paladin,
    ranger,
    magic_user,
    thief,
    monk,
    cleric_fighter,
    cleric_fighter_magic_user,
    cleric_ranger,
    cleric_magic_user,
    cleric_thief,
    fighter_magic_user,
    fighter_thief,
    fighter_magic_user_thief,
    magic_user_thief
};

enum class Alignment : unsigned
{
    lawful_good,
    lawful_neutral,
    lawful_evil,
    neutral_good,
    true_neutral,
    neutral_evil,
    chaotic_good,
    chaotic_neutral,
    chaotic_evil
};

// The saving throw categories, in AD&D order.
enum class Save : unsigned
{
    paralysis_poison_death,
    petrification_polymorph,
    rod_staff_wand,
    breath_weapon,
    spell
};

// A character's level in each class slot; 0 where it has none.
using ClassLevels = std::array<int, class_slot_count>;

[[nodiscard]] constexpr int &level_in(ClassLevels &levels, ClassSlot slot) noexcept
{
    return levels[static_cast<std::size_t>(slot)];
}

[[nodiscard]] constexpr int level_in(const ClassLevels &levels, ClassSlot slot) noexcept
{
    return levels[static_cast<std::size_t>(slot)];
}

// The classes a profession holds levels in.
[[nodiscard]] std::span<const ClassSlot> classes_of(Profession profession);
// A class's stable ID ("magic_user"), as sheets list the classes of their levels.
[[nodiscard]] std::string_view class_slot_id(ClassSlot slot) noexcept;
// A new character of the profession: level 1 in each of its classes.
[[nodiscard]] ClassLevels first_levels(Profession profession);
// The professions creation offers a race in the game, in its menu order:
// Curse adds the paladin and ranger.
[[nodiscard]] std::span<const Profession> professions_for(Race race, GoldBoxGame game);
[[nodiscard]] std::span<const Alignment> alignments_for(Profession profession);

// The lowest and highest score a race may have. Strength, and the exceptional
// Strength an 18 may carry, depend on gender.
struct AbilityLimits
{
    rules::AbilityArray<int> minimum, maximum;
    int exceptional_strength_maximum{};
};

[[nodiscard]] AbilityLimits ability_limits(Race race, Gender gender);
// What the race adds to each 3d6 roll. The original's creation never applied
// these (a bug, docs/ORIGINAL-RULES.md); OpenGoldBox does.
[[nodiscard]] rules::AbilityArray<int> racial_adjustments(Race race);
[[nodiscard]] rules::AbilityArray<int> class_minimums(Profession profession);

// How age changes a character's ability scores, chosen for each game.
enum class Aging : unsigned
{
    // Age changes nothing.
    off,
    // As the original: the starting age changes the scores once, at creation.
    original,
    // AD&D's age table, with the young adult's +1 Constitution the original
    // left out; the scores change again as the character grows older.
    add
};

inline constexpr Aging default_aging = Aging::original;

// The aging rule's name as saves keep it ("off", "original" or "add"), and the
// rule a name stands for.
[[nodiscard]] std::string_view aging_name(Aging aging) noexcept;
[[nodiscard]] std::optional<Aging> aging_named(std::string_view name) noexcept;

enum class AgeCategory : unsigned
{
    young_adult,
    mature,
    middle_aged,
    old,
    venerable
};

[[nodiscard]] AgeCategory age_category(Race race, int age);
// What reaching this age has done to each ability under the aging rule; each
// category's changes add to those before it.
[[nodiscard]] rules::AbilityArray<int> age_changes(Aging aging, Race race, int age);

// A character as rolled at creation, before it is named. Its scores are
// worked out from these rolls for the game's aging rule (ability_scores).
struct RolledCharacter
{
    // Each ability's best of six 3d6 rolls with the racial adjustment, before
    // age and the race's and class's limits.
    rules::AbilityArray<int> rolls;
    // The d100 that becomes exceptional Strength on a fighter's 18.
    int strength_percentile{};
    int starting_age{};
    // Each class's first-level Hit Die, in classes_of order: in Pool of
    // Radiance at least two thirds of the die, in Curse the better of two rolls.
    std::array<int, 3> first_level_hit_dice{};
    int gold{};
    // Curse's character starts trained up to its starting experience: the
    // Hit Die of each class (classes_of order) at each level from 2 to the
    // highest that experience reaches (starting_reach), rolled at creation.
    std::vector<int> training_hit_dice;
};

// Rolls a new character as the game's creation does, the racial adjustments
// included: its abilities, its age, its Hit Dice and its starting gold.
[[nodiscard]] RolledCharacter roll_character(Race race, Profession profession, GoldBoxGame game,
                                             RandomState &random);

// The experience a new character starts with: none in Pool of Radiance;
// 25,000 in Curse, shared among a multi-class character's classes.
[[nodiscard]] int starting_experience(Profession profession, GoldBoxGame game);
// The highest level of a class the starting experience reaches by the table.
[[nodiscard]] int starting_reach(ClassSlot slot, Profession profession, GoldBoxGame game);
// The platinum a new character starts with besides its gold: Curse's 300.
[[nodiscard]] int starting_platinum(GoldBoxGame game);

// A new character trained to its starting experience at creation (Curse's
// SilentTrainPlayer): every class that can rises, training after training,
// each training's Hit Points rolled at creation, and a magic-user learns the
// game's spell of each level it reaches.
struct StartingTraining
{
    ClassLevels levels{};
    int hit_points{}, rolled_hit_points{};
    std::vector<int> learned_spells;
};

[[nodiscard]] StartingTraining starting_training(Race race, Profession profession,
                                                 const rules::AbilityArray<int> &scores,
                                                 const RolledCharacter &rolled, GoldBoxGame game);

// The character's scores at its current age. At its starting age each
// ability is changed for age, kept within the race's limits and raised to the
// class's minimums, as the original's creation does. Under AD&D aging, the
// changes of each older age group follow, keeping the score between 3 and the
// race's maximum.
[[nodiscard]] rules::AbilityArray<int> ability_scores(Race race, Gender gender,
                                                      Profession profession,
                                                      const RolledCharacter &rolled, Aging aging,
                                                      int current_age);
// 1 to 100 (18/00) for a character with fighter levels and a Strength of 18,
// up to the race's limit; otherwise 0.
[[nodiscard]] int exceptional_strength(Race race, Gender gender, Profession profession,
                                       const RolledCharacter &rolled, int strength);
// First-level Hit Points: the class dice with the Constitution bonus, divided
// among a multi-class character's classes (Secret divides the two apart).
[[nodiscard]] int first_level_hit_points(Profession profession, const RolledCharacter &rolled,
                                         int constitution,
                                         GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// What first level adds to the Hit Points rolled (the original's hpRolled):
// the class dice divided among the classes, with no Constitution bonus.
[[nodiscard]] int first_level_rolled_hit_points(Profession profession,
                                                const RolledCharacter &rolled);

[[nodiscard]] int starting_age(Race race, Profession profession, RandomState &random);
// The Hit Points every level of every class adds for this Constitution, before
// a multi-class character's total is divided among its classes (ConHpBonus).
// Curse and Secret give fighters, paladins and rangers more from 17 up;
// Curse doubles a first-level ranger's, and Secret counts each class's first
// level dice.
[[nodiscard]] int constitution_hit_point_bonus(Profession profession, int constitution,
                                               GoldBoxGame game = GoldBoxGame::pool_of_radiance,
                                               bool first_level = false);

// How many points better (or, below zero, worse) Dexterity makes Armor Class.
// It is lost against an attack from behind.
[[nodiscard]] int dexterity_armor_class_bonus(int dexterity) noexcept;
// What Dexterity adds to a missile's to-hit roll and to initiative.
[[nodiscard]] int dexterity_missile_bonus(int dexterity) noexcept;

enum class ThiefSkill : unsigned
{
    pick_pockets,
    open_locks,
    find_remove_traps,
    move_silently,
    hide_in_shadows,
    hear_noise,
    climb_walls,
    read_languages
};

// A thief's chance in percent for each skill, by its thief level, race and
// Dexterity. Dexterity below 9 counts as 9 and above 19 as 19, the ends of
// the original's table; no chance falls below 0.
[[nodiscard]] std::array<int, 8> thief_skills(Race race, int thief_level, int dexterity,
                                              GoldBoxGame game = GoldBoxGame::pool_of_radiance);

[[nodiscard]] constexpr int skill_chance(const std::array<int, 8> &skills,
                                         ThiefSkill skill) noexcept
{
    return skills[static_cast<std::size_t>(skill)];
}

// The best THAC0 of the character's classes at their levels in the game.
[[nodiscard]] int thac0(const ClassLevels &levels, GoldBoxGame game);
// The roll each saving throw needs: the best of the character's classes.
[[nodiscard]] std::array<int, 5> saving_throws(const ClassLevels &levels, GoldBoxGame game);

[[nodiscard]] constexpr int saving_throw(const std::array<int, 5> &throws, Save save) noexcept
{
    return throws[static_cast<std::size_t>(save)];
}

// The experience a class needs to reach a level, or nothing when the class
// cannot reach it in this game.
[[nodiscard]] std::optional<int> experience_for_level(ClassSlot slot, int level, GoldBoxGame game);
// The highest level a race may reach in a class for its Strength (and in
// Curse a magic-user's for its Intelligence), or nothing when only the
// experience table limits it.
[[nodiscard]] std::optional<int> racial_level_limit(Race race, ClassSlot slot,
                                                    const rules::AbilityArray<int> &scores,
                                                    GoldBoxGame game);
// Training (chargen.pas, TrainingMenu): the classes a character with this
// much experience trains in at once, each whose next level's experience it
// has and that its race's limit and the experience table let rise; in class
// slot order, empty when none can.
[[nodiscard]] std::vector<ClassSlot> classes_ready_to_train(Race race, const ClassLevels &levels,
                                                            const rules::AbilityArray<int> &scores,
                                                            int experience, GoldBoxGame game);
// The experience a character keeps after training from these levels: just
// short of the level after next of a class it trained in, when it had that
// much (the most such class allows). Training never takes two levels at once.
[[nodiscard]] int experience_after_training(const ClassLevels &levels,
                                            std::span<const ClassSlot> trained, int experience,
                                            GoldBoxGame game);
// The trained classes' Hit Dice, rolled, for these levels after training: in
// Pool of Radiance each class's dice; in Curse the better of two rolls of one
// die, or a fixed amount at the levels past a class's Hit Dice.
[[nodiscard]] int roll_training_hit_dice(const ClassLevels &trained_levels,
                                         std::span<const ClassSlot> trained, GoldBoxGame game,
                                         RandomState &random);
// What training adds to the Hit Points: the roll and the Constitution bonus
// of all the profession's classes, divided among them, at least 1.
[[nodiscard]] int training_hit_points(Profession profession, int rolled, int constitution,
                                      GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// What training adds to the Hit Points rolled: the roll divided among the
// classes, at least 1 (chargen.pas, TrainingMenu).
[[nodiscard]] int training_rolled_hit_points(Profession profession, int rolled);
// The maximum Hit Points a character a temple raised loses (temple.pas,
// RaiseDead), reckoned with the Constitution it is left with: what its
// maximum holds above its rolled Hit Points, divided by its Constitution's
// bonus summed over its class levels when that is not 0; nothing below a
// Constitution of 14, nor at 17 or more without fighter levels.
[[nodiscard]] int raised_hit_point_loss(const ClassLevels &levels, int constitution, int hit_points,
                                        int rolled_hit_points);
// A character's share of an award of experience (postcbt.pas,
// AwardExperience): a single-class cleric, fighter, magic-user or thief
// with its prime ability above 15 earns a tenth more; a two-class character
// half and a three-class character a third.
[[nodiscard]] int experience_share(Profession profession, const rules::AbilityArray<int> &scores,
                                   int award, GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// The spells a character knows (chargen.pas, CreateCharacterMenu): a cleric
// every cleric spell of a level it has slots for; a magic-user the four it
// starts with (Detect Magic, Read Magic, Sleep, and Shield in Pool of
// Radiance or Enlarge in Curse) and those it learned since (`learned`), in
// the spell table's order.
[[nodiscard]] std::vector<int> known_spells(const ClassLevels &levels, int wisdom,
                                            std::span<const int> learned, GoldBoxGame game);

using SpellSlots = std::array<int, 7>;

// Spell slots of levels 1 to 7 for a cleric or magic-user level, with the
// cleric's Wisdom bonus; zeros for other classes. Only Secret reaches sixth-
// and seventh-level spells, and only with the Wisdom or Intelligence for them.
[[nodiscard]] SpellSlots spell_slots(ClassSlot slot, int level, int wisdom, int intelligence = 9,
                                     GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// Combat (docs/ORIGINAL-RULES.md, plan step 4).

// Strength on the original's 0 to 30 scale its tables use: up to 17 as it is,
// 18 by its exceptional percentage as 18 (none) to 23 (18/00), and 19 to 25
// as 24 to 30.
[[nodiscard]] int strength_scale(int strength, int exceptional_strength) noexcept;
// What Strength adds to a weapon's to-hit roll and damage, for a weapon that
// takes Strength.
[[nodiscard]] int strength_to_hit_bonus(int strength, int exceptional_strength) noexcept;
[[nodiscard]] int strength_damage_bonus(int strength, int exceptional_strength) noexcept;
// How many coins' weight Strength lets a creature carry before it slows
// (charhlp.pas, WeightAllowance); below zero for a weak one.
[[nodiscard]] int weight_allowance(int strength, int exceptional_strength) noexcept;
// A creature's movement under this load, in coins' weight: up to 512 over its
// allowance changes nothing, then 9, 6 to 1,024 over and 3 beyond. A load only
// slows (charhlp.pas, EncumbranceAdjust); armor slows only by its weight in
// the load (the user, 2026-11-12, docs/DEVIATIONS.md).
[[nodiscard]] int encumbered_movement(int movement, int load, int allowance) noexcept;
// The attacks a rate per two rounds gives this round: an odd round, as the
// original counts them, rounds a half attack up and an even round down.
[[nodiscard]] int attacks_this_round(int rate_per_two_rounds, int round) noexcept;
// Whether a d20 roll hits: 1 always misses, 20 always hits, and otherwise the
// roll with its modifier must reach THAC0 minus the target's Armor Class.
[[nodiscard]] bool attack_hits(int natural_roll, int thac0, int armor_class, int modifier) noexcept;
// A hit's damage from its dice and modifier. The original summed them in a
// byte, so a total below zero wrapped to more than 250; it is 0 here
// (docs/DEVIATIONS.md, the user, 2026-11-12).
[[nodiscard]] int hit_damage(int dice_total, int modifier) noexcept;
} // namespace opengold::original
#endif
