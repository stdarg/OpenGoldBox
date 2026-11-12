#ifndef OPENGOLD_ORIGINAL_RULES_H
#define OPENGOLD_ORIGINAL_RULES_H
#include "opengold/random_state.h"
#include "opengold/rules.h"
#include <array>
#include <optional>
#include <span>

// The original Pool of Radiance rules: AD&D 1st edition as the game applied
// them (docs/ORIGINAL-RULES.md). The tables are the original's; the code is
// OpenGoldBox's own.
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
// A new character of the profession: level 1 in each of its classes.
[[nodiscard]] ClassLevels first_levels(Profession profession);
// The professions creation offers a race, in the original's menu order.
[[nodiscard]] std::span<const Profession> professions_for(Race race);
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

// A character as rolled at creation, before it is named.
struct RolledCharacter
{
    rules::AbilityArray<int> scores;
    // 1 to 100 (18/00) on a Strength of 18 for a character with fighter levels;
    // otherwise 0.
    int exceptional_strength{};
    int age{};
    int hit_points{};
    int gold{};
};

// Rolls a new character as the original's creation does: each ability the
// best of six 3d6 rolls with the racial adjustment, changed for its age, kept
// within the race's limits and raised to the profession's minimums; then its
// age, first-level Hit Points and starting gold.
[[nodiscard]] RolledCharacter roll_character(Race race, Gender gender, Profession profession,
                                             RandomState &random);

[[nodiscard]] int starting_age(Race race, Profession profession, RandomState &random);
// The Hit Points every level of every class adds for this Constitution, before
// a multi-class character's total is divided among its classes.
[[nodiscard]] int constitution_hit_point_bonus(Profession profession, int constitution);

// How many points better (or, below zero, worse) Dexterity makes Armor Class.
// It is lost against an attack from behind.
[[nodiscard]] int dexterity_armor_class_bonus(int dexterity) noexcept;
// What Dexterity adds to a missile's to-hit roll and to initiative.
[[nodiscard]] int dexterity_missile_bonus(int dexterity) noexcept;

// The best THAC0 of the character's classes at their levels.
[[nodiscard]] int thac0(const ClassLevels &levels);
// The roll each saving throw needs: the best of the character's classes.
[[nodiscard]] std::array<int, 5> saving_throws(const ClassLevels &levels);

[[nodiscard]] constexpr int saving_throw(const std::array<int, 5> &throws, Save save) noexcept
{
    return throws[static_cast<std::size_t>(save)];
}

// The experience a class needs to reach a level, or nothing when the class
// cannot reach it in this game.
[[nodiscard]] std::optional<int> experience_for_level(ClassSlot slot, int level);
// The highest level a race may reach in a class for its Strength, or nothing
// when only the experience table limits it.
[[nodiscard]] std::optional<int> racial_level_limit(Race race, ClassSlot slot, int strength);
// Spell slots of levels 1 to 3 for a cleric or magic-user level, with the
// cleric's Wisdom bonus; zeros for other classes.
[[nodiscard]] std::array<int, 3> spell_slots(ClassSlot slot, int level, int wisdom);
} // namespace opengold::original
#endif
