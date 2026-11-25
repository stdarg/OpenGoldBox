#ifndef OPENGOLD_ORIGINAL_RULES_H
#define OPENGOLD_ORIGINAL_RULES_H
#include "opengold/character_rules.h"
#include "opengold/gold_box_game.h"
#include "opengold/random_state.h"
#include "opengold/rules.h"
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
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
    human,
    silvanesti_elf,
    qualinesti_elf,
    krynn_half_elf,
    mountain_dwarf,
    hill_dwarf,
    kender,
    krynn_human,
    // The Buck Rogers games' races, in their record order 0 to 8: race 0 is
    // the monsters', and the last two are NPCs' and monsters' only.
    monster,
    terran,
    martian,
    venusian,
    mercurian,
    tinker,
    desert_runner,
    lunarian,
    lowlander
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
    magic_user_thief,
    knight,
    // The Buck Rogers games' careers, in their record order 1 to 6. No race
    // is offered the Scout.
    rocket_jock,
    medic,
    warrior,
    engineer,
    rogue,
    scout
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
[[nodiscard]] std::string_view class_slot_id(ClassSlot slot, GoldBoxGame game) noexcept;
[[nodiscard]] unsigned record_race(Race race, GoldBoxGame game) noexcept;
[[nodiscard]] Race race_from_record(unsigned code, GoldBoxGame game) noexcept;

// Early Krynn's class suborders select genuinely different progressions.
struct KrynnOrders
{
    int deity{}, robe{}, knight{};
};

[[nodiscard]] std::optional<int> champions_experience_for_level(ClassSlot slot, int level,
                                                                KrynnOrders orders = {});
[[nodiscard]] std::array<int, 9> champions_spell_slots(ClassSlot slot, int level, int wisdom,
                                                       KrynnOrders orders);
void recalculate_champions_numbers(rules::CharacterSheet::Original &numbers,
                                   const rules::AbilityArray<int> &scores);
[[nodiscard]] std::vector<int> champions_learning_spells(int robe, int highest_spell_level);
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
[[nodiscard]] AbilityLimits ability_limits(Race race, Gender gender, GoldBoxGame game);
// What the race adds to each 3d6 roll. The original's creation never applied
// these (a bug, docs/ORIGINAL-RULES.md); OpenGoldBox does.
[[nodiscard]] rules::AbilityArray<int> racial_adjustments(Race race);
// Pools of Darkness asks a Charisma of 6 of every class besides.
[[nodiscard]] rules::AbilityArray<int>
class_minimums(Profession profession, GoldBoxGame game = GoldBoxGame::pool_of_radiance);

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
// The same with `experience` for each class, as a game that sets it does (an
// Unlimited Adventures design's game record).
[[nodiscard]] RolledCharacter roll_character(Race race, Profession profession, GoldBoxGame game,
                                             int experience, RandomState &random,
                                             KrynnOrders orders = {});

// The experience a new character starts with: none in Pool of Radiance;
// 25,000 in Curse, shared among a multi-class character's classes.
[[nodiscard]] int starting_experience(Profession profession, GoldBoxGame game);
// The highest level of a class the starting experience reaches by the table.
[[nodiscard]] int starting_reach(ClassSlot slot, Profession profession, GoldBoxGame game);
// The same with `experience` for each class.
[[nodiscard]] int starting_reach(ClassSlot slot, int experience, GoldBoxGame game);
// The platinum a new character starts with besides its gold: Curse's 300.
[[nodiscard]] int starting_platinum(GoldBoxGame game);

// One item of a new character's starting equipment: a definition of the
// game's (its ITEM0, numbered from 1), how many of it (none
// keeps the definition's count), and whether the character readies it.
struct StartingItem
{
    unsigned definition{};
    std::optional<int> count;
    bool readied{};

    bool operator==(const StartingItem &) const = default;
};

// The items a new character of these levels starts with, by its class
// family (Pools of Darkness's and Treasures' CreateStartingItems): fighters,
// paladins and rangers first, then thieves, then clerics, then the rest.
// None in the games that give none at creation.
[[nodiscard]] std::vector<StartingItem> starting_items(const ClassLevels &levels, GoldBoxGame game);
// Native Dark Queen equipment also depends on race and profession.
[[nodiscard]] std::vector<StartingItem> starting_items(const rules::CharacterSheet &sheet);
// A native Dark Queen spell byte that fits a white/red magic-user scroll.
[[nodiscard]] bool dark_queen_scroll_spell(int native, unsigned robe) noexcept;

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
// The same with `experience` for each class.
[[nodiscard]] StartingTraining starting_training(Race race, Profession profession,
                                                 const rules::AbilityArray<int> &scores,
                                                 const RolledCharacter &rolled, GoldBoxGame game,
                                                 int experience, KrynnOrders orders = {});

// The character's scores at its current age. At its starting age each
// ability is changed for age, kept within the race's limits and raised to the
// class's minimums, as the original's creation does. Under AD&D aging, the
// changes of each older age group follow, keeping the score between 3 and the
// race's maximum.
[[nodiscard]] rules::AbilityArray<int>
ability_scores(Race race, Gender gender, Profession profession, const RolledCharacter &rolled,
               Aging aging, int current_age, GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// 1 to 100 (18/00) for a character with fighter levels and a Strength of 18,
// up to the race's limit; otherwise 0. In Pools of Darkness a paladin or a
// ranger rolls it too.
[[nodiscard]] int exceptional_strength(Race race, Gender gender, Profession profession,
                                       const RolledCharacter &rolled, int strength,
                                       GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// First-level Hit Points: the class dice with the Constitution bonus, divided
// among a multi-class character's classes (Secret divides the two apart).
[[nodiscard]] int first_level_hit_points(Profession profession, const RolledCharacter &rolled,
                                         int constitution,
                                         GoldBoxGame game = GoldBoxGame::pool_of_radiance,
                                         KrynnOrders orders = {});
// What first level adds to the Hit Points rolled (the original's hpRolled):
// the class dice divided among the classes, with no Constitution bonus.
[[nodiscard]] int first_level_rolled_hit_points(Profession profession,
                                                const RolledCharacter &rolled);

// A new character's age (chargen.pas): its race's and class's dice, a
// multi-class one the most its cleric's, magic-user's or fighter's allow.
// Gateway gives a half-elf ranger 26+3d8 and Pools of Darkness 22; Curse and
// Secret have no row for one, and a half-elf fighter's stands in (docs/ORIGINAL-RULES.md, Bugs).
[[nodiscard]] int starting_age(Race race, Profession profession, RandomState &random,
                               GoldBoxGame game = GoldBoxGame::pool_of_radiance);
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
// of all the profession's classes, divided among them, at least 1. From
// Secret on, the bonus is only the trained classes' (at these levels after
// training), none past a class's Hit Dice, divided apart from the roll.
[[nodiscard]] int training_hit_points(Profession profession, int rolled, int constitution,
                                      GoldBoxGame game, const ClassLevels &trained_levels,
                                      std::span<const ClassSlot> trained);
// What training adds to the Hit Points rolled: the roll divided among the
// classes, at least 1 (chargen.pas, TrainingMenu).
[[nodiscard]] int training_rolled_hit_points(Profession profession, int rolled);
// What Restoration adds to the Hit Points rolled with a level of a class
// (magic.pas, SpellRestoration): the Hit Points it gives back, less the
// class's Constitution bonus divided among the classes ever drained while the
// level is one whose Hit Die is rolled.
[[nodiscard]] int restored_rolled_hit_points(ClassSlot slot, int restored_level, int hit_points,
                                             int constitution, int drained_classes,
                                             GoldBoxGame game);
// The maximum Hit Points a character a temple raised loses (temple.pas,
// RaiseDead), reckoned with the Constitution it is left with: what its
// maximum holds above its rolled Hit Points, divided by its Constitution's
// bonus summed over its class levels when that is not 0; nothing below a
// Constitution of 14, nor at 17 or more without fighter levels.
[[nodiscard]] int raised_hit_point_loss(const ClassLevels &levels, int constitution, int hit_points,
                                        int rolled_hit_points);
// Modify Character (each game's chargen.pas, ModifyCharacterMenu; the kit's
// modify.c): a new character's scores and Hit Points, changed a step at a
// time within its race's, class's and Hit Dice's limits
// (docs/ORIGINAL-RULES.md, Modify Character).

// What Modify changes: the six scores, exceptional Strength (1 to 100, on a
// Strength of 18) and the maximum Hit Points.
struct ModifiedNumbers
{
    rules::AbilityArray<int> scores{};
    int exceptional_strength{};
    int hit_points{};

    bool operator==(const ModifiedNumbers &) const = default;
};

// The character whose numbers Modify changes.
struct ModifiedCharacter
{
    Race race{};
    Gender gender{};
    Profession profession{};
    ClassLevels levels{};
    KrynnOrders orders{};
};

// Whether the game's Modify still takes a character with this much
// experience: one its game's creation gave it, for one, two or three classes
// (none in Pool of Radiance; from Curse to Gateway none as well). An
// Unlimited Adventures design starts its characters with `design_start`.
[[nodiscard]] bool modify_allowed(int experience, GoldBoxGame game, int design_start = 0);
// The lowest and highest Hit Points Modify gives a character of these levels
// and this Constitution (HpMinimum and HpMaximum; Pools of Darkness's,
// Treasures' and the kit's HpMaximum and HitPointCap).
[[nodiscard]] int modify_hit_point_minimum(const ModifiedCharacter &character, int constitution,
                                           GoldBoxGame game);
[[nodiscard]] int modify_hit_point_maximum(const ModifiedCharacter &character, int constitution,
                                           GoldBoxGame game);
// The numbers with one row lowered or raised a step: a score within its
// race's limits and, lowered, its class's minimum; Strength's exceptional
// percentage first, a point at a time, for a fighter (from Curse on a
// paladin or ranger too) at its race's 18; a Constitution's change keeping
// the Hit Points within their new limits.
[[nodiscard]] ModifiedNumbers modify_step(const ModifiedNumbers &numbers,
                                          const ModifiedCharacter &character, rules::ModifyRow row,
                                          rules::ModifyStep step, GoldBoxGame game);
// The Hit Points rolled (hpRolled) a modified character keeps: in Pool of
// Radiance its dice's, `rolled_before`; from Curse on its maximum less its
// Constitution's share of it, as Keep works it out.
[[nodiscard]] int modified_rolled_hit_points(const ModifiedCharacter &character, int constitution,
                                             int hit_points, int rolled_before, GoldBoxGame game);

// A character's share of an award of experience (postcbt.pas,
// AwardExperience): a single-class cleric, fighter, magic-user or thief
// with its prime ability above 15 earns a tenth more; a two-class character
// half and a three-class character a third.
[[nodiscard]] int experience_share(Profession profession, const rules::AbilityArray<int> &scores,
                                   int award, GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// The spells a character knows (chargen.pas, CreateCharacterMenu): a cleric
// every cleric spell of a level it has slots for, and so a paladin from level
// 9; a ranger from level 8 every druid spell of a level it has slots for, and
// in Secret every magic-user spell too (Curse's ranger learns those only from
// scrolls); a magic-user the four it starts with (Detect Magic, Read Magic,
// Sleep, and Shield in Pool of Radiance or Enlarge in Curse) and those it
// learned since (`learned`), in the spell table's order.
[[nodiscard]] std::vector<int> known_spells(const ClassLevels &levels, int wisdom,
                                            std::span<const int> learned, GoldBoxGame game);

using SpellSlots = std::array<int, 9>;

// Spell slots of levels 1 to 9 for a cleric or magic-user level, with the
// cleric's Wisdom bonus; zeros for other classes. Only Secret reaches sixth-
// and seventh-level spells, and Pools of Darkness's magic-users eighth- and
// ninth-level ones, only with the Wisdom or Intelligence for them.
[[nodiscard]] SpellSlots spell_slots(ClassSlot slot, int level, int wisdom, int intelligence = 9,
                                     GoldBoxGame game = GoldBoxGame::pool_of_radiance);

// A character's slots by the list its spells come from (classcal.pas,
// RecalcSpellSlots): a cleric's and a magic-user's, with a paladin's cleric
// slots from level 9 and a ranger's druid slots (spell levels 1 to 3) and
// magic-user slots (1 and 2) from level 8, in Curse and Secret.
struct CasterSlots
{
    SpellSlots cleric{}, druid{}, magic_user{};
};

[[nodiscard]] CasterSlots caster_slots(const ClassLevels &levels, int wisdom, int intelligence,
                                       GoldBoxGame game);

// Combat (docs/ORIGINAL-RULES.md, plan step 4).

// Strength on the original's 0 to 30 scale its tables use: up to 17 as it is,
// 18 by its exceptional percentage as 18 (none) to 23 (18/00), and 19 to 25
// as 24 to 30.
[[nodiscard]] int strength_scale(int strength, int exceptional_strength) noexcept;

// Strength as an effect keeps it (fxengine.pas, StrengthModifier): 18 with its
// exceptional Strength as 1 to 101, any other Strength as 100 more; and back.
[[nodiscard]] int strength_code(int strength, int exceptional_strength) noexcept;
// Whether one Strength is greater than another, 18/01 to 18/00 between 18 and 19.
[[nodiscard]] bool stronger_than(int strength, int exceptional_strength, int other_strength,
                                 int other_exceptional_strength) noexcept;
[[nodiscard]] std::pair<int, int> strength_of_code(int code) noexcept;
// What Enlarge gives at a caster level (magic.pas, SpellEnlarge): 18/00,
// 18/01, 18/51, 18/76, 18/91 and 18/00 at levels 1 to 6; Curse and the games
// after it go on to 19, 20, 21 and 22 at levels 7 to 10.
[[nodiscard]] std::pair<int, int> enlarged_strength(int caster_level, GoldBoxGame game) noexcept;
// The die the Strength spell adds (SpellStrength): a d8 for a fighter (and,
// from Curse on, a paladin or ranger), a d6 for a cleric or thief, else a d4.
[[nodiscard]] int strength_spell_die(const ClassLevels &levels, GoldBoxGame game) noexcept;
// The Strength the Strength spell gives for the roll: above 18 a warrior's
// turns into 10 points of exceptional Strength a point, to 18/00, and anyone
// else's stops at 18.
[[nodiscard]] std::pair<int, int> strengthened(int strength, int exceptional_strength,
                                               const ClassLevels &levels, GoldBoxGame game,
                                               int rolled) noexcept;
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

// The game's difficulty levels, easiest first, as its settings order them.
enum class DifficultyLevel : unsigned
{
    easy,
    normal,
    hard,
    nightmare,
    infernal
};

// Whether a game's LOAD MONSTER scales its hostile monsters' Hit Points by its
// level setting (Secret's, Gateway's, Pools of Darkness's and Treasures' eclvm.pas).
[[nodiscard]] bool scales_monster_hit_points(GoldBoxGame game) noexcept;

// The percent of their Hit Points those games give their monsters at a level:
// a quarter for each of the original's levels above Novice and two more
// (its (level + 2) / 4, the original's Squire to Champion as Easy to
// Nightmare), and Infernal twice the Hit Points, two levels past Champion.
[[nodiscard]] unsigned game_hit_points_percent(DifficultyLevel level) noexcept;
} // namespace opengold::original
#endif
