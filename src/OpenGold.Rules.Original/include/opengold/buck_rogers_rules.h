#ifndef OPENGOLD_BUCK_ROGERS_RULES_H
#define OPENGOLD_BUCK_ROGERS_RULES_H
#include "opengold/buck_rogers_records.h"
#include "opengold/original_rules.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The characters of the two Buck Rogers games, Countdown to Doomsday and
// Matrix Cubed, under their original rules: careers and races, the seven
// abilities, Hit Points, experience, saving throws, the 84 skills and their
// checks, creation, training and Modify (docs/games/countdown.md and
// matrix-cubed.md, "Rules"). A character is the game's own record,
// buck_rogers::Creature, so what these rules make saves as the original
// saves it. Matrix Cubed shares Countdown's tables, which run on to its
// level 20, except where its source differs.
namespace opengold::original
{
[[nodiscard]] constexpr bool buck_rogers_race(Race race) noexcept
{
    return race >= Race::monster && race <= Race::lowlander;
}

[[nodiscard]] constexpr bool buck_rogers_career(Profession career) noexcept
{
    return career >= Profession::rocket_jock && career <= Profession::scout;
}

// A career's number in a record (1 to 6), and the career a number names.
[[nodiscard]] std::uint8_t record_career(Profession career);
[[nodiscard]] std::optional<Profession> career_from_record(unsigned code) noexcept;

// The six races creation offers, in its menu's order (chargen.pas
// CreateCharacterMenu); professions_for(race, game) gives each one's careers.
[[nodiscard]] std::span<const Race> buck_rogers_creation_races() noexcept;
// The careers creation offers a race, in its menu's order (gdconsts.pas
// raceCareers): five for the human races, a Tinker's Medic and Engineer, a
// Desert Runner's Rocketjock, Warrior and Engineer; none for the others.
// professions_for(race, game) answers with these in the Buck Rogers games.
[[nodiscard]] std::span<const Profession> buck_rogers_careers(Race race) noexcept;

// The seven abilities, in the record's order.
enum class BuckRogersAbility : unsigned
{
    strength,
    dexterity,
    constitution,
    intelligence,
    wisdom,
    charisma,
    tech
};

inline constexpr std::size_t buck_rogers_ability_count = 7;
using BuckRogersScores = std::array<int, buck_rogers_ability_count>;

// What a race adds to each rolled score (gdconsts.pas raceStatAdjust).
[[nodiscard]] BuckRogersScores race_ability_adjustments(Race race);
// The least of each score a career takes (careerStatMinimum).
[[nodiscard]] BuckRogersScores career_ability_minimums(Profession career);
// Each score the best of six 3d6, with the race's adjustment, at least 1 and
// raised to the career's minimum (CreateCharacterMenu).
[[nodiscard]] BuckRogersScores roll_buck_rogers_abilities(Race race, Profession career,
                                                          RandomState &random);

// The level a new character starts at (2 in Countdown, 7 in Matrix Cubed)
// and the highest training reaches (8 and 20; C_Career.MAX_LEVEL).
[[nodiscard]] int buck_rogers_starting_level(GoldBoxGame game);
[[nodiscard]] int buck_rogers_level_limit(GoldBoxGame game);
// The experience a career needs for a level (experienceNeeded): none for
// level 1, nothing past the game's table (level 9 in Countdown, 20 in Matrix
// Cubed).
[[nodiscard]] std::optional<std::uint32_t> career_experience_for_level(Profession career, int level,
                                                                       GoldBoxGame game);
// The combat value a career has at a level (careerBaseCombat).
[[nodiscard]] int career_base_combat(Profession career, int level, GoldBoxGame game);
// What Constitution adds to each level's Hit Points (constitutionLevelBonus),
// before a career other than the Warrior's stops it at 2.
[[nodiscard]] int constitution_level_bonus(int constitution);
// The Hit Points a level adds (chargen.pas AddLevelHitPoints): the better of
// two of the career's die (d10 Warrior, d8 Engineer, else d6) with the
// Constitution bonus, at least 1; from the level the game fixes them (9 in
// Countdown, 10 in Matrix Cubed), 3 for a Warrior and 2 for the rest.
[[nodiscard]] int roll_level_hit_points(Profession career, int new_level, int constitution,
                                        GoldBoxGame game, RandomState &random);
// Each of the eight saving throws a new character of the race needs
// (baseSavingThrows less raceSaveAdjust), and at a level: one better at every
// third level (TrainingMenu).
[[nodiscard]] std::array<int, 8> starting_saving_throws(Race race);
[[nodiscard]] std::array<int, 8> buck_rogers_saving_throws(Race race, int level);
// Movement: 12, a Tinker's 9 and a Desert Runner's 15.
[[nodiscard]] int buck_rogers_movement(Race race);
// What the scores of 1 to 22 give in a fight (gdconsts.pas): Dexterity's
// Armor Class (-5 to +5, raising it), Strength's to hit and damage in melee
// and unarmed (-5 to +4, -4 to +10), and the weight Strength allows, past
// half of which a character moves half as far (charhlp.pas RecalcMovement).
// A score past the table reads its last row. Dexterity's missile adjustment
// is ship combat's (buck_rogers_missile_adjustment, buck_rogers_ship_combat.h).
[[nodiscard]] int buck_rogers_armor_adjustment(int dexterity);
[[nodiscard]] int buck_rogers_strength_to_hit(int strength);
[[nodiscard]] int buck_rogers_strength_damage(int strength);
[[nodiscard]] int buck_rogers_weight_allowance(int strength);

// The 84 skills, by their numbers in the record and the scripts.
enum class Skill : std::uint8_t
{
    repair_electrical = 1,
    repair_mechanical,
    repair_nuclear_engine,
    repair_life_support,
    repair_rocket_hull,
    jury_rig,
    bypass_security,
    open_lock,
    commo_operation,
    sensor_operation,
    demolitions,
    cook,
    first_aid,
    repair_weapon,
    repair_computer,
    life_suspension_tech,
    treat_light_wounds,
    treat_serious_wounds,
    treat_critical_wounds,
    treat_poisoning,
    treat_stun,
    treat_disease,
    diagnose,
    general_knowledge,
    ship_lore,
    composition,
    mathematics,
    physics,
    geology,
    planetology,
    battle_tactics,
    chemistry,
    biology,
    design_engineering,
    history,
    astronomy,
    botany,
    literature,
    economics,
    metallurgy,
    speak_read_language,
    law,
    programming,
    cryptography,
    library_search,
    astrogation,
    navigation,
    mimic,
    memorize,
    disguise,
    pilot_rocket,
    pilot_fixed_wing,
    drive_ground_car,
    pilot_rotor_wing,
    drive_motorcycle,
    use_jetpack,
    pilot_ship_sub,
    drive_heavy_ground_vehicle,
    drive_jetcar,
    animal_riding,
    hide_in_shadows,
    move_silently,
    pick_pockets,
    acrobatics,
    climb,
    swimming,
    maneuver_in_zero_g,
    play_instrument,
    paint_draw,
    hypnosis,
    act,
    intimidate,
    leadership,
    befriend_animal,
    animal_training,
    fast_talk,
    singing,
    distract,
    etiquette,
    tracking,
    shadowing,
    read_lips,
    notice,
    planetary_survival
};

inline constexpr std::size_t skill_count = 84;
// Each skill's level, skill 1 at [0].
using SkillLevels = std::array<std::uint8_t, skill_count>;

// The skill screen's groups. A group past the career's is its ability's: the
// group's number is the ability's place in the record. No skill is in the
// Constitution group.
enum class SkillGroup : unsigned
{
    career,
    dexterity,
    constitution,
    intelligence,
    wisdom,
    charisma,
    technical
};

struct SkillDefinition
{
    std::string_view name;
    // Whether the general lists offer it; a skill they do not offer is never
    // learned (gdconsts.pas skillDefinitions).
    bool offered{};
    SkillGroup group{};
};

[[nodiscard]] const SkillDefinition &skill_definition(Skill skill);
[[nodiscard]] Skill skill_numbered(unsigned number);
[[nodiscard]] std::uint8_t &level_of(SkillLevels &levels, Skill skill);
[[nodiscard]] int level_of(const SkillLevels &levels, Skill skill);
// A career's eight career skills (careerSkills), in the screen's order.
[[nodiscard]] std::span<const Skill> career_skills(Profession career);
[[nodiscard]] bool is_career_skill(Profession career, Skill skill);
// Life Suspension Tech to Diagnose: a Medic's only, never in a general list.
[[nodiscard]] bool is_medical_skill(Skill skill) noexcept;
// The skills the general lists offer a career, in skill order: offered,
// not its career skills, not medical.
[[nodiscard]] std::vector<Skill> general_skills(Profession career);

// The levels in other skills a skill needs (charhlp.pas
// MeetsSkillPrerequisite), as the skill screen lists them: all of them, or
// for Composition any one.
struct SkillNeed
{
    Skill skill{};
    int level{};
};

struct SkillPrerequisites
{
    std::vector<SkillNeed> needs;
    bool any{};
};

[[nodiscard]] SkillPrerequisites skill_prerequisites(Skill skill);
// Whether the character has the levels in other skills a skill needs
// (charhlp.pas MeetsSkillPrerequisite); medical skills need a Medic.
[[nodiscard]] bool meets_skill_prerequisite(const SkillLevels &levels, Profession career,
                                            Skill skill);
// A skill's level before any points (skills.pas StartingSkillLevel): 10 for
// a Rocketjock's piloting and driving skills and a Rogue's favored ones,
// when their prerequisites are met; else 0.
[[nodiscard]] int starting_skill_level(const SkillLevels &levels, Profession career, Skill skill);

// The skills a new Matrix Cubed character starts with (careerInitialSkills);
// none in Countdown, where the player spends the points.
struct PresetSkill
{
    Skill skill{};
    int level{};
};

[[nodiscard]] std::span<const PresetSkill> preset_skills(Profession career, GoldBoxGame game);
// The group ability's score a skill adds, none while its prerequisites are
// unmet (SkillAbilityScore), and a skill's value: its level and that score,
// for a character who can act and has the skill (SkillValue). The original
// kept the value in a signed byte in Countdown (a byte in Matrix Cubed), so a
// value past 127 turned negative; here it does not (docs/games/countdown.md,
// Bugs).
[[nodiscard]] int skill_ability_score(const buck_rogers::Creature &character, Skill skill);
[[nodiscard]] int skill_value(const buck_rogers::Creature &character, Skill skill);

// A skill check's result, as the original numbers it.
enum class SkillCheckGrade : unsigned
{
    bad_failure,
    failure,
    success,
    great_success
};

// The value a check rolls against: difficulty 0 four times the skill, 1
// twice, 2 as it is, 3 a half and 4 a quarter, rounded down.
[[nodiscard]] int scaled_skill_value(int value, int difficulty) noexcept;
// The grade of a d100 against a scaled value: at most the value succeeds, by
// half the value or more greatly; missing by more than the value fails badly.
[[nodiscard]] SkillCheckGrade skill_check_grade(int scaled_value, int d100) noexcept;

struct SkillCheck
{
    // Which of the values rolled best: it is the one who acts.
    std::size_t roller{};
    SkillCheckGrade grade{};
};

// A check (charhlp.pas SelectSkillCharacter): each value in party order rolls
// a d100, the best margin (the first of equals) is the roller, and the grade
// is the roller's. The original graded by the last member's value instead
// (docs/games/countdown.md, Bugs). One value is a single character's check.
[[nodiscard]] SkillCheck check_skill(std::span<const int> values, int difficulty,
                                     RandomState &random);

// The skill a script's SKILL CHECK asks for. The hail menu's three checks
// name skills two numbers below the ones its choices mean (74, 70 and 69 for
// Fast Talk/Convince, Intimidate and Act); they are corrected where the
// script and operand are the ones the game ships (the user, 2026-11-20;
// docs/games/countdown.md, Bugs). Every other check asks what it names.
[[nodiscard]] unsigned script_check_skill(GoldBoxGame game, std::string_view archive,
                                          unsigned script, std::uint16_t address,
                                          unsigned skill) noexcept;

// Why the skill screen refuses a point.
enum class SkillRefusal
{
    none,
    not_listed,
    no_points,
    prerequisites_unmet,
    at_level_maximum,
    at_general_maximum,
    no_new_skills
};

// The skill screen (skills.pas EditSkills): career points go to the career
// skills and general points to the general ones, each skill at most
// max_per_skill above its floor and a general skill at most 80. A general
// skill is new while it is at its starting level, and the number known is
// limited: 7 at Countdown's level 2, 12 at Matrix Cubed's level 7, else one
// more than were known on entry. Lowering a skill below another's
// prerequisite refunds what was spent on that one.
class SkillSpending
{
  public:
    // Creation's: Countdown's 80 career and 40 general points, at most 30 a
    // skill; Matrix Cubed's preset skills, which may be moved about, at most
    // 105 above each skill's starting level.
    [[nodiscard]] static SkillSpending at_creation(const buck_rogers::Creature &character,
                                                   GoldBoxGame game);
    // Training's: 40 career and 20 general points, at most 15 a skill.
    [[nodiscard]] static SkillSpending at_training(const buck_rogers::Creature &character,
                                                   GoldBoxGame game);

    [[nodiscard]] SkillRefusal add(Skill skill);
    // False when the skill is at its floor.
    bool subtract(Skill skill);

    [[nodiscard]] const SkillLevels &levels() const noexcept
    {
        return levels_;
    }

    [[nodiscard]] int career_points() const noexcept
    {
        return career_points_;
    }

    [[nodiscard]] int general_points() const noexcept
    {
        return general_points_;
    }

    [[nodiscard]] int max_per_skill() const noexcept
    {
        return max_per_skill_;
    }

    [[nodiscard]] int known_general_skills() const noexcept
    {
        return known_general_;
    }

    // How many general skills the character may know on this screen.
    [[nodiscard]] int general_skills_allowed() const noexcept
    {
        return max_general_;
    }

    // The character with these skill levels.
    void apply_to(buck_rogers::Creature &character) const;

  private:
    SkillSpending(const buck_rogers::Creature &character, GoldBoxGame game, int career_points,
                  int general_points, int max_per_skill);
    [[nodiscard]] int starting_level(Skill skill) const;
    void count_known_general();
    void adjust_dependents(Skill changed, bool raised, bool career_pool);
    void reset_dependent(Skill changed, int required, Skill dependent, bool raised,
                         bool career_pool);

    GoldBoxGame game_;
    Profession career_;
    int level_{};
    SkillLevels levels_{}, floors_{};
    int career_points_{}, general_points_{}, max_per_skill_{};
    int known_general_{}, max_general_{};
    std::vector<Skill> general_;
};

// A Warrior's weapon specializations (viewchar.pas SpecializationMenu): each
// a weapon type of item templates 1 to 15, at most three in one type.
inline constexpr int max_specialization = 3;
[[nodiscard]] int specialization_level(const buck_rogers::Creature &character,
                                       std::uint8_t item_type);
// The item types the menu offers, in its order (template 15 first).
[[nodiscard]] std::vector<std::uint8_t>
specializable_weapon_types(std::span<const buck_rogers::Item> templates);
// One more specialization in the type, in the first empty slot; false when
// the character has three in it or no slot is left.
[[nodiscard]] bool specialize(buck_rogers::Creature &character, std::uint8_t item_type);
// One specialization in the type taken back, from the last slot that holds
// it, as creation's page lets a choice be taken back until it is left; false
// when the character has none in it.
bool unspecialize(buck_rogers::Creature &character, std::uint8_t item_type);
// How many a new Warrior chooses: 1 in Countdown, 3 in Matrix Cubed; and
// whether a Warrior trained to this level chooses one (every even level).
[[nodiscard]] int creation_specializations(Profession career, GoldBoxGame game);
[[nodiscard]] bool specializes_at(Profession career, int new_level) noexcept;

// Creation (chargen.pas CreateCharacterMenu): the race, gender and career
// chosen, 1,000 credits, age 1d6 + 18, Armor Class 50, the starting
// level's experience, the abilities rolled and the character trained free
// from level 0 to it. Throws std::invalid_argument for a race and career
// the menus do not pair.
[[nodiscard]] buck_rogers::Creature new_buck_rogers_character(Race race, Gender gender,
                                                              Profession career, GoldBoxGame game,
                                                              RandomState &random);
// Rerolls the abilities and the free training's Hit Points. Matrix Cubed
// lowered the saving throws again at each reroll; here they start over
// (docs/games/matrix-cubed.md, Bugs).
void reroll_buck_rogers_character(buck_rogers::Creature &character, GoldBoxGame game,
                                  RandomState &random);
// The body icon a new character starts with (SetDefaultIcon): a Desert
// Runner's is one of four at random.
[[nodiscard]] std::uint8_t default_icon(Race race, Gender gender, Profession career,
                                        RandomState &random);
// The two items every new character carries, readied (shop.pas
// BuildAmmoItems): item template 7 with 100 rounds and template 20, in the
// list's order (template 20 first).
[[nodiscard]] std::vector<buck_rogers::Item>
buck_rogers_starting_items(std::span<const buck_rogers::Item> templates);

// Why training refuses a character (chargen.pas TrainingMenu).
enum class TrainingRefusal
{
    none,
    not_conscious,
    at_level_limit,
    not_enough_experience
};

[[nodiscard]] TrainingRefusal training_refusal(const buck_rogers::Creature &character,
                                               GoldBoxGame game);
// One level, free: the career's combat value and Hit Points for it, the
// experience cut to one short of the next level, and every third level one
// better on each saving throw. A Warrior then specializes at an even level
// and every character spends SkillSpending::at_training. At Matrix Cubed's
// level 20 the original read past its table and cut the experience to the
// next career's first figure; here a character at the limit keeps it
// (docs/games/matrix-cubed.md, Bugs).
void train_buck_rogers_character(buck_rogers::Creature &character, GoldBoxGame game,
                                 RandomState &random);

// Modify Character (chargen.pas ModifyCharacterMenu): only a character with
// its career's starting experience; each score between the race's
// adjustment + 3 and + 18 and at least the career's minimum; the Hit Points
// between HpMinimum and HpMaximum for its level and Constitution.
[[nodiscard]] bool buck_rogers_modifiable(const buck_rogers::Creature &character, GoldBoxGame game);

struct ScoreRange
{
    int lowest{}, highest{};
};

[[nodiscard]] ScoreRange buck_rogers_modify_range(Race race, Profession career,
                                                  BuckRogersAbility ability);
[[nodiscard]] int buck_rogers_hit_point_minimum(const buck_rogers::Creature &character);
[[nodiscard]] int buck_rogers_hit_point_maximum(const buck_rogers::Creature &character);
// One step of a score, or of the Hit Points; a Constitution step keeps the
// Hit Points within their new limits. Keep sets the current scores.
void modify_buck_rogers_ability(buck_rogers::Creature &character, BuckRogersAbility ability,
                                rules::ModifyStep step);
void modify_buck_rogers_hit_points(buck_rogers::Creature &character, rules::ModifyStep step);
void keep_buck_rogers_modify(buck_rogers::Creature &character);
} // namespace opengold::original
#endif
