#include "opengold/buck_rogers_rules.h"
#include "opengold/dice.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace opengold::original
{
namespace
{
using buck_rogers::Creature;
using buck_rogers::Item;
using P = Profession;
using R = Race;
using S = Skill;

constexpr std::size_t career_count = 6;
// The record's status of a character who can act (types.pas StOkay).
constexpr std::uint8_t status_okay = 1;
// What a general skill can reach (skills.pas MAX_GENERAL_LEVEL).
constexpr int general_skill_limit = 80;

[[nodiscard]] std::size_t career_index(Profession career)
{
    if (!buck_rogers_career(career))
    {
        throw std::invalid_argument("Not a Buck Rogers career");
    }
    return static_cast<std::size_t>(career) - static_cast<std::size_t>(P::rocket_jock);
}

// Terran to Lowlander as 0 to 7; the monsters' race has no row.
[[nodiscard]] std::size_t race_index(Race race)
{
    if (!buck_rogers_race(race) || race == R::monster)
    {
        throw std::invalid_argument("Not a Buck Rogers character race");
    }
    return static_cast<std::size_t>(race) - static_cast<std::size_t>(R::terran);
}

[[nodiscard]] Profession career_of(const Creature &character)
{
    const auto career = career_from_record(character.career);
    if (!career)
    {
        throw std::invalid_argument("The character has no career");
    }
    return *career;
}

[[nodiscard]] Race race_of(const Creature &character)
{
    return race_from_record(character.race, GoldBoxGame::countdown_to_doomsday);
}

[[nodiscard]] int ability_of(const Creature &character, BuckRogersAbility ability)
{
    return character.abilities[static_cast<std::size_t>(ability)];
}

[[nodiscard]] bool matrix(GoldBoxGame game)
{
    if (!is_buck_rogers(game))
    {
        throw std::invalid_argument("Not a Buck Rogers game");
    }
    return game == GoldBoxGame::matrix_cubed;
}

// Experience for levels 2 to 20 by career: Countdown's table is the first
// eight columns of Matrix Cubed's (gdconsts.pas experienceNeeded).
constexpr std::array<std::array<std::uint32_t, 19>, career_count> experience_table{{
    {1250, 2500, 5000, 10000, 20000, 40000, 70000, 110000, 160000, 220000, 440000, 880000, 1760000,
     3520000, 7040000, 14080000, 28160000, 56320000, 112640000},
    {1500, 3000, 6000, 12000, 24000, 48000, 96000, 192000, 384000, 768000, 1536000, 3072000,
     6144000, 12288000, 24576000, 49152000, 98304000, 196608000, 393216000},
    {2000, 4000, 8000, 16000, 32000, 64000, 125000, 250000, 500000, 750000, 1000000, 2000000,
     4000000, 8000000, 16000000, 32000000, 64000000, 128000000, 256000000},
    {1250, 2500, 5000, 10000, 20000, 40000, 70000, 110000, 160000, 220000, 440000, 880000, 1760000,
     3520000, 7040000, 14080000, 28160000, 56320000, 112640000},
    {1250, 2500, 5000, 10000, 20000, 40000, 70000, 110000, 160000, 220000, 440000, 880000, 1760000,
     3520000, 7040000, 14080000, 28160000, 56320000, 112640000},
    {2250, 4500, 9000, 18000, 36000, 75000, 150000, 300000, 600000, 900000, 1500000, 3000000,
     6000000, 12000000, 24000000, 48000000, 96000000, 192000000, 384000000},
}};

// Combat value at levels 0 to 20 by career; Countdown's runs to level 9
// (careerBaseCombat).
constexpr std::array<std::array<int, 21>, career_count> base_combat_table{{
    {40, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49},
    {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46, 48, 48, 48, 50, 50, 50, 52, 52},
    {40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59},
    {40, 40, 40, 40, 42, 42, 42, 44, 44, 44, 46, 46, 46, 48, 48, 48, 50, 50, 50, 52, 52},
    {40, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49},
    {40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59},
}};

constexpr std::array<BuckRogersScores, 8> race_adjustments{{
    {0, 0, 1, 0, 1, 0, 0},
    {-1, 1, -1, 0, -1, 1, 0},
    {0, -1, 1, 0, 1, -1, 0},
    {-1, 1, 1, 0, 0, 0, 0},
    {-2, 3, -2, 0, 0, 0, 3},
    {2, 2, 1, 0, 0, -1, 0},
    {-2, 1, -1, 2, 0, 0, 0},
    {3, 0, 1, 0, 0, -3, 1},
}};

constexpr std::array<BuckRogersScores, career_count> career_minimums{{
    {1, 13, 1, 11, 1, 12, 1},
    {1, 12, 1, 12, 12, 1, 1},
    {10, 8, 10, 8, 1, 1, 1},
    {10, 1, 12, 8, 1, 1, 13},
    {1, 13, 1, 8, 9, 13, 1},
    {1, 1, 8, 8, 9, 8, 1},
}};

constexpr std::array<int, 8> base_saves{15, 14, 14, 13, 13, 16, 13, 13};
// Subtracted from the base: a positive entry makes the save easier.
constexpr std::array<std::array<int, 8>, 8> race_save_adjustments{{
    {1, 0, 0, 0, 0, 0, 0, 0},
    {-1, 0, 0, 0, 1, 1, -1, 1},
    {1, 3, 0, 1, 1, -2, 3, -2},
    {1, 0, 0, 0, 3, 1, 4, -3},
    {0, -1, -3, -2, 0, 0, 0, 0},
    {2, 0, 1, 1, 2, 2, -3, 4},
    {0, -1, 0, 0, 2, 2, -1, -1},
    {2, 4, 2, 3, 1, -4, 6, -6},
}};

// By Constitution 1 to 22.
constexpr std::array<int, 22> constitution_bonuses{-3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0,
                                                   0,  0,  0,  1,  2,  3,  4, 5, 5, 6, 6};

constexpr std::array<Race, 6> creation_races{R::terran,    R::martian, R::venusian,
                                             R::mercurian, R::tinker,  R::desert_runner};

constexpr std::array<Profession, 5> five_careers{P::rocket_jock, P::medic, P::warrior, P::engineer,
                                                 P::rogue};
constexpr std::array<Profession, 2> tinker_careers{P::medic, P::engineer};
constexpr std::array<Profession, 3> desert_runner_careers{P::rocket_jock, P::warrior, P::engineer};

constexpr SkillGroup tech = SkillGroup::technical, intel = SkillGroup::intelligence,
                     dex = SkillGroup::dexterity, cha = SkillGroup::charisma,
                     wis = SkillGroup::wisdom;

constexpr std::array<SkillDefinition, skill_count> skills{{
    {"Repair Electrical", true, tech},
    {"Repair Mechanical", true, tech},
    {"Repair Nuclear Engine", true, tech},
    {"Repair Life Support", true, tech},
    {"Repair Rocket Hull", true, tech},
    {"Jury Rig", true, tech},
    {"Bypass Security", true, tech},
    {"Open Lock", true, tech},
    {"Commo Operation", true, tech},
    {"Sensor Operation", true, tech},
    {"Demolitions", true, tech},
    {"Cook", false, tech},
    {"First Aid", true, tech},
    {"Repair Weapon", true, tech},
    {"Repair Computer", false, tech},
    {"Life Suspension Tech", true, tech},
    {"Treat Light Wounds", true, tech},
    {"Treat Serious Wounds", true, tech},
    {"Treat Critical Wounds", true, tech},
    {"Treat Poisoning", true, tech},
    {"Treat Stun/Paralysis", true, tech},
    {"Treat Disease", true, tech},
    {"Diagnose", true, tech},
    {"General Knowledge", false, intel},
    {"Ship Lore", false, intel},
    {"Composition", false, intel},
    {"Mathematics", true, intel},
    {"Physics", false, intel},
    {"Geology", false, intel},
    {"Planetology", true, intel},
    {"Battle Tactics", true, intel},
    {"Chemistry", false, intel},
    {"Biology", false, intel},
    {"Design Engineering", false, intel},
    {"History", false, intel},
    {"Astronomy", true, intel},
    {"Botany", false, intel},
    {"Literature", false, intel},
    {"Economics", false, intel},
    {"Metallurgy", false, intel},
    {"Speak/Read Language", false, intel},
    {"Law", false, intel},
    {"Programming", true, intel},
    {"Cryptography", false, intel},
    {"Library Search", true, intel},
    {"Astrogation", true, intel},
    {"Navigation", true, intel},
    {"Mimic", false, intel},
    {"Memorize", false, intel},
    {"Disguise", true, intel},
    {"Pilot Rocket", true, dex},
    {"Pilot Fixed Wing", true, dex},
    {"Drive Ground Car", true, dex},
    {"Pilot Rotor Wing", true, dex},
    {"Drive Motorcycle", false, dex},
    {"Use Jetpack", true, dex},
    {"Pilot Ship/Sub", false, dex},
    {"Drive Heavy Ground Vehicle", false, dex},
    {"Drive Jetcar", true, dex},
    {"Animal Riding", false, dex},
    {"Hide in Shadows", true, dex},
    {"Move Silently", true, dex},
    {"Pick Pockets", true, dex},
    {"Acrobatics", true, dex},
    {"Climb", true, dex},
    {"Swimming", false, dex},
    {"Maneuver in Zero-G", true, dex},
    {"Play Instrument", false, dex},
    {"Paint/Draw", false, dex},
    {"Hypnosis", false, cha},
    {"Act", true, cha},
    {"Intimidate", true, cha},
    {"Leadership", true, cha},
    {"Befriend Animal", true, cha},
    {"Animal Training", false, cha},
    {"Fast Talk/Convince", true, cha},
    {"Singing", true, cha},
    {"Distract", true, cha},
    {"Etiquette", true, cha},
    {"Tracking", true, wis},
    {"Shadowing", true, wis},
    {"Read Lips", false, wis},
    {"Notice", true, wis},
    {"Planetary Survival", true, wis},
}};

constexpr std::array<std::array<Skill, 8>, career_count> career_skill_table{{
    {S::notice, S::maneuver_in_zero_g, S::use_jetpack, S::pilot_rocket, S::pilot_fixed_wing,
     S::drive_ground_car, S::pilot_rotor_wing, S::drive_jetcar},
    {S::treat_light_wounds, S::treat_serious_wounds, S::treat_critical_wounds, S::diagnose,
     S::treat_poisoning, S::treat_stun, S::treat_disease, S::life_suspension_tech},
    {S::notice, S::leadership, S::battle_tactics, S::maneuver_in_zero_g, S::use_jetpack,
     S::move_silently, S::demolitions, S::repair_weapon},
    {S::notice, S::repair_electrical, S::repair_mechanical, S::repair_nuclear_engine,
     S::repair_life_support, S::repair_rocket_hull, S::jury_rig, S::maneuver_in_zero_g},
    {S::notice, S::bypass_security, S::open_lock, S::hide_in_shadows, S::move_silently,
     S::pick_pockets, S::climb, S::fast_talk},
    {S::notice, S::animal_riding, S::befriend_animal, S::move_silently, S::climb, S::tracking,
     S::planetary_survival, S::planetology},
}};

// Matrix Cubed's new characters' skills (gdconsts.pas careerInitialSkills).
constexpr std::array<PresetSkill, 9> rocket_jock_presets{{{S::drive_ground_car, 40},
                                                          {S::maneuver_in_zero_g, 85},
                                                          {S::notice, 40},
                                                          {S::pilot_rocket, 85},
                                                          {S::use_jetpack, 50},
                                                          {S::first_aid, 25},
                                                          {S::mathematics, 25},
                                                          {S::astronomy, 20},
                                                          {S::astrogation, 70}}};
constexpr std::array<PresetSkill, 9> medic_presets{{{S::diagnose, 15},
                                                    {S::treat_critical_wounds, 75},
                                                    {S::treat_light_wounds, 50},
                                                    {S::treat_poisoning, 40},
                                                    {S::treat_serious_wounds, 40},
                                                    {S::treat_stun, 60},
                                                    {S::maneuver_in_zero_g, 80},
                                                    {S::befriend_animal, 55},
                                                    {S::battle_tactics, 5}}};
constexpr std::array<PresetSkill, 12> warrior_presets{{{S::battle_tactics, 50},
                                                       {S::demolitions, 25},
                                                       {S::leadership, 50},
                                                       {S::maneuver_in_zero_g, 85},
                                                       {S::move_silently, 20},
                                                       {S::notice, 20},
                                                       {S::repair_weapon, 10},
                                                       {S::use_jetpack, 20},
                                                       {S::first_aid, 70},
                                                       {S::jury_rig, 50},
                                                       {S::repair_electrical, 10},
                                                       {S::repair_mechanical, 10}}};
constexpr std::array<PresetSkill, 10> engineer_presets{{{S::jury_rig, 20},
                                                        {S::maneuver_in_zero_g, 85},
                                                        {S::notice, 25},
                                                        {S::repair_electrical, 30},
                                                        {S::repair_life_support, 30},
                                                        {S::repair_mechanical, 30},
                                                        {S::repair_nuclear_engine, 30},
                                                        {S::repair_rocket_hull, 30},
                                                        {S::first_aid, 70},
                                                        {S::repair_weapon, 70}}};
constexpr std::array<PresetSkill, 12> rogue_presets{{{S::bypass_security, 60},
                                                     {S::climb, 30},
                                                     {S::fast_talk, 40},
                                                     {S::hide_in_shadows, 40},
                                                     {S::move_silently, 50},
                                                     {S::notice, 40},
                                                     {S::open_lock, 60},
                                                     {S::pick_pockets, 40},
                                                     {S::first_aid, 10},
                                                     {S::maneuver_in_zero_g, 70},
                                                     {S::mathematics, 10},
                                                     {S::programming, 50}}};
constexpr std::array<PresetSkill, 10> scout_presets{{{S::planetology, 10},
                                                     {S::animal_riding, 10},
                                                     {S::move_silently, 75},
                                                     {S::climb, 40},
                                                     {S::befriend_animal, 30},
                                                     {S::tracking, 50},
                                                     {S::notice, 45},
                                                     {S::planetary_survival, 45},
                                                     {S::maneuver_in_zero_g, 70},
                                                     {S::battle_tactics, 70}}};

// A skill that another needs at a level (the C_Skill _NEEDS_ constants).
struct Dependent
{
    Skill prerequisite;
    int required;
    Skill dependent;
};

// Each prerequisite and what depends on it, in the order the skill screen
// resets them (skills.pas AdjustPrerequisites). Composition is left out: no
// list offers it, nor the arts it needs, so its resets change nothing.
// Lowering Treat Light Wounds below Treat Serious Wounds' need resets Treat
// Critical Wounds too, as Matrix Cubed does; Countdown left its points
// stranded (docs/games/countdown.md, Bugs).
constexpr std::array<Dependent, 19> dependents{{
    {S::repair_electrical, 10, S::jury_rig},
    {S::repair_electrical, 30, S::repair_life_support},
    {S::repair_electrical, 40, S::repair_computer},
    {S::repair_mechanical, 10, S::jury_rig},
    {S::repair_mechanical, 10, S::repair_rocket_hull},
    {S::repair_mechanical, 25, S::repair_life_support},
    {S::repair_mechanical, 10, S::repair_nuclear_engine},
    {S::treat_light_wounds, 30, S::treat_serious_wounds},
    {S::treat_light_wounds, 30, S::treat_critical_wounds},
    {S::treat_serious_wounds, 40, S::treat_critical_wounds},
    {S::diagnose, 25, S::treat_disease},
    {S::diagnose, 15, S::treat_poisoning},
    {S::mathematics, 25, S::navigation},
    {S::mathematics, 25, S::astrogation},
    {S::mathematics, 10, S::programming},
    {S::planetology, 10, S::planetary_survival},
    {S::astronomy, 15, S::navigation},
    {S::astronomy, 20, S::astrogation},
    {S::befriend_animal, 20, S::animal_training},
}};

[[nodiscard]] bool has(const SkillLevels &levels, Skill skill, int required)
{
    return level_of(levels, skill) >= required;
}

// The hail menu's three checks (Countdown's ECL2 script 34, Matrix Cubed's
// ECL1 script 19): bluff, intimidate and ask for help.
struct ScriptSkillFix
{
    GoldBoxGame game;
    std::string_view archive;
    unsigned script;
    std::uint16_t address;
    unsigned shipped, meant;
};

constexpr std::array<ScriptSkillFix, 6> script_skill_fixes{{
    {GoldBoxGame::countdown_to_doomsday, "ECL2.DAX", 34, 0x8DE8, 74, 76},
    {GoldBoxGame::countdown_to_doomsday, "ECL2.DAX", 34, 0x8E67, 70, 72},
    {GoldBoxGame::countdown_to_doomsday, "ECL2.DAX", 34, 0x8ED1, 69, 71},
    {GoldBoxGame::matrix_cubed, "ECL1.DAX", 19, 0x932E, 74, 76},
    {GoldBoxGame::matrix_cubed, "ECL1.DAX", 19, 0x93B7, 70, 72},
    {GoldBoxGame::matrix_cubed, "ECL1.DAX", 19, 0x9422, 69, 71},
}};

void gain_level(Creature &character, Profession career, GoldBoxGame game, RandomState &random)
{
    ++character.level;
    character.base_combat =
        static_cast<std::uint8_t>(career_base_combat(career, character.level, game));
    const int gain =
        roll_level_hit_points(career, character.level,
                              ability_of(character, BuckRogersAbility::constitution), game, random);
    character.hit_point_maximum = static_cast<std::uint16_t>(character.hit_point_maximum + gain);
    character.hit_points = static_cast<std::uint16_t>(character.hit_points + gain);
    if (character.level % 3 == 0)
    {
        for (auto &save : character.saves)
        {
            --save;
        }
    }
}

// The rolls and the free training from level 0 (CreateCharacterMenu, which
// TrainingMenu serves with isFreeTraining set).
void roll_and_train(Creature &character, Race race, Profession career, GoldBoxGame game,
                    RandomState &random)
{
    const auto scores = roll_buck_rogers_abilities(race, career, random);
    for (std::size_t ability = 0; ability < buck_rogers_ability_count; ++ability)
    {
        character.abilities[ability] = static_cast<std::uint8_t>(scores[ability]);
    }
    character.current_abilities = character.abilities;
    const bool warrior = career == P::warrior;
    const bool desert_runner = race == R::desert_runner;
    character.unarmed_dice_count[0] = 1;
    character.unarmed_dice_sides[0] = warrior || desert_runner ? 3 : 2;
    character.unarmed_damage_bonus[0] = desert_runner ? 1 : 0;
    character.unarmed_attacks_doubled = warrior ? 4 : 2;
    character.base_movement = static_cast<std::uint8_t>(buck_rogers_movement(race));

    character.level = 0;
    character.hit_point_maximum = 0;
    character.hit_points = 0;
    while (true)
    {
        const auto next = career_experience_for_level(career, character.level + 1, game);
        if (character.level >= 1 && (!next || *next > character.experience))
        {
            break;
        }
        gain_level(character, career, game, random);
    }
    for (const auto skill : career_skills(career))
    {
        level_of(character.skills, skill) =
            static_cast<std::uint8_t>(starting_skill_level(character.skills, career, skill));
    }
}

void set_saves(Creature &character, const std::array<int, 8> &saves)
{
    for (std::size_t save = 0; save < saves.size(); ++save)
    {
        character.saves[save] = static_cast<std::uint8_t>(saves[save]);
    }
}

} // namespace

std::uint8_t record_career(Profession career)
{
    return static_cast<std::uint8_t>(career_index(career) + 1);
}

std::optional<Profession> career_from_record(unsigned code) noexcept
{
    if (code < 1 || code > career_count)
    {
        return std::nullopt;
    }
    return static_cast<Profession>(static_cast<unsigned>(P::rocket_jock) + code - 1);
}

std::span<const Race> buck_rogers_creation_races() noexcept
{
    return creation_races;
}

std::span<const Profession> buck_rogers_careers(Race race) noexcept
{
    switch (race)
    {
    case R::terran:
    case R::martian:
    case R::venusian:
    case R::mercurian:
        return five_careers;
    case R::tinker:
        return tinker_careers;
    case R::desert_runner:
        return desert_runner_careers;
    default:
        return {};
    }
}

BuckRogersScores race_ability_adjustments(Race race)
{
    return race_adjustments[race_index(race)];
}

BuckRogersScores career_ability_minimums(Profession career)
{
    return career_minimums[career_index(career)];
}

BuckRogersScores roll_buck_rogers_abilities(Race race, Profession career, RandomState &random)
{
    const auto adjustments = race_ability_adjustments(race);
    const auto minimums = career_ability_minimums(career);
    BuckRogersScores scores{};
    for (std::size_t ability = 0; ability < buck_rogers_ability_count; ++ability)
    {
        int best = 0;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const int rolled = roll_die(random, 6) + roll_die(random, 6) + roll_die(random, 6);
            best = std::max(best, rolled);
        }
        scores[ability] = std::max({best + adjustments[ability], 1, minimums[ability]});
    }
    return scores;
}

int buck_rogers_starting_level(GoldBoxGame game)
{
    return matrix(game) ? 7 : 2;
}

int buck_rogers_level_limit(GoldBoxGame game)
{
    return matrix(game) ? 20 : 8;
}

std::optional<std::uint32_t> career_experience_for_level(Profession career, int level,
                                                         GoldBoxGame game)
{
    const int last_level = matrix(game) ? 20 : 9;
    if (level <= 1)
    {
        return 0U;
    }
    if (level > last_level)
    {
        return std::nullopt;
    }
    return experience_table[career_index(career)][static_cast<std::size_t>(level - 2)];
}

int career_base_combat(Profession career, int level, GoldBoxGame game)
{
    const int last_level = matrix(game) ? 20 : 9;
    if (level < 0 || level > last_level)
    {
        throw std::out_of_range("No combat value for that level");
    }
    return base_combat_table[career_index(career)][static_cast<std::size_t>(level)];
}

int constitution_level_bonus(int constitution)
{
    const int row = std::clamp(constitution, 1, 22);
    return constitution_bonuses[static_cast<std::size_t>(row - 1)];
}

int roll_level_hit_points(Profession career, int new_level, int constitution, GoldBoxGame game,
                          RandomState &random)
{
    const bool warrior = career == P::warrior;
    const int fixed_from = matrix(game) ? 10 : 9;
    if (new_level >= fixed_from)
    {
        return warrior ? 3 : 2;
    }
    const int sides = warrior ? 10 : career == P::engineer ? 8 : 6;
    const int first = roll_die(random, sides);
    const int better = std::max(first, roll_die(random, sides));
    int bonus = constitution_level_bonus(constitution);
    if (!warrior)
    {
        bonus = std::min(bonus, 2);
    }
    return std::max(better + bonus, 1);
}

std::array<int, 8> starting_saving_throws(Race race)
{
    const auto &adjustments = race_save_adjustments[race_index(race)];
    std::array<int, 8> saves{};
    for (std::size_t save = 0; save < saves.size(); ++save)
    {
        saves[save] = base_saves[save] - adjustments[save];
    }
    return saves;
}

std::array<int, 8> buck_rogers_saving_throws(Race race, int level)
{
    auto saves = starting_saving_throws(race);
    for (auto &save : saves)
    {
        save -= std::max(level, 0) / 3;
    }
    return saves;
}

int buck_rogers_movement(Race race)
{
    switch (race)
    {
    case R::tinker:
        return 9;
    case R::desert_runner:
        return 15;
    default:
        return 12;
    }
}

namespace
{
// A score's row of a table for scores 1 to 22.
template <typename T> [[nodiscard]] int by_score(const std::array<T, 22> &table, int score)
{
    return static_cast<int>(table[static_cast<std::size_t>(std::clamp(score, 1, 22) - 1)]);
}
} // namespace

int buck_rogers_armor_adjustment(int dexterity)
{
    static constexpr std::array<int, 22> table{-5, -5, -4, -3, -2, -1, 0, 0, 0, 0, 0,
                                               0,  0,  0,  1,  2,  3,  4, 4, 4, 5, 5};
    return by_score(table, dexterity);
}

int buck_rogers_strength_to_hit(int strength)
{
    static constexpr std::array<int, 22> table{-5, -4, -3, -2, -2, -1, -1, 0, 0, 0, 0,
                                               0,  0,  0,  0,  0,  1,  2,  3, 3, 4, 4};
    return by_score(table, strength);
}

int buck_rogers_strength_damage(int strength)
{
    static constexpr std::array<int, 22> table{-4, -3, -2, -1, -1, 0, 0, 0, 0, 0, 0,
                                               0,  0,  0,  0,  1,  1, 2, 4, 6, 8, 10};
    return by_score(table, strength);
}

int buck_rogers_weight_allowance(int strength)
{
    static constexpr std::array<int, 22> table{2,   4,   10,  20,  30,  40,  50,  60,
                                               70,  80,  90,  100, 110, 120, 130, 140,
                                               170, 220, 400, 600, 900, 1200};
    return by_score(table, strength);
}

const SkillDefinition &skill_definition(Skill skill)
{
    return skills.at(static_cast<std::size_t>(skill) - 1);
}

Skill skill_numbered(unsigned number)
{
    if (number < 1 || number > skill_count)
    {
        throw std::out_of_range("Skills are numbered 1 to 84");
    }
    return static_cast<Skill>(number);
}

std::uint8_t &level_of(SkillLevels &levels, Skill skill)
{
    return levels.at(static_cast<std::size_t>(skill) - 1);
}

int level_of(const SkillLevels &levels, Skill skill)
{
    return levels.at(static_cast<std::size_t>(skill) - 1);
}

std::span<const Skill> career_skills(Profession career)
{
    return career_skill_table[career_index(career)];
}

bool is_career_skill(Profession career, Skill skill)
{
    return std::ranges::find(career_skills(career), skill) != career_skills(career).end();
}

bool is_medical_skill(Skill skill) noexcept
{
    return skill >= S::life_suspension_tech && skill <= S::diagnose;
}

std::vector<Skill> general_skills(Profession career)
{
    std::vector<Skill> listed;
    for (unsigned number = 1; number <= skill_count; ++number)
    {
        const auto skill = skill_numbered(number);
        if (skill_definition(skill).offered && !is_career_skill(career, skill) &&
            !is_medical_skill(skill))
        {
            listed.push_back(skill);
        }
    }
    return listed;
}

SkillPrerequisites skill_prerequisites(Skill skill)
{
    switch (skill)
    {
    case S::repair_nuclear_engine:
    case S::repair_rocket_hull:
        return {{{S::repair_mechanical, 10}}};
    case S::repair_life_support:
        return {{{S::repair_electrical, 30}, {S::repair_mechanical, 25}}};
    case S::jury_rig:
        return {{{S::repair_electrical, 10}, {S::repair_mechanical, 10}}};
    case S::repair_computer:
        return {{{S::repair_electrical, 40}}};
    case S::treat_serious_wounds:
        return {{{S::treat_light_wounds, 30}}};
    case S::treat_critical_wounds:
        return {{{S::treat_serious_wounds, 40}}};
    case S::treat_poisoning:
        return {{{S::diagnose, 15}}};
    case S::treat_disease:
        return {{{S::diagnose, 25}}};
    case S::composition:
        return {{{S::singing, 20}, {S::play_instrument, 20}, {S::literature, 20}}, true};
    case S::programming:
        return {{{S::mathematics, 10}}};
    case S::astrogation:
        return {{{S::astronomy, 20}, {S::mathematics, 25}}};
    case S::navigation:
        return {{{S::astronomy, 15}, {S::mathematics, 25}}};
    case S::animal_training:
        return {{{S::befriend_animal, 20}}};
    case S::planetary_survival:
        return {{{S::planetology, 10}}};
    default:
        return {};
    }
}

namespace
{
// Authored creatures may have no character career. Their ordinary skills
// still use their governing ability; medical skills require a Medic.
bool skill_prerequisites(const SkillLevels &levels, bool medic, Skill skill)
{
    if (is_medical_skill(skill) && !medic)
    {
        return false;
    }
    const auto prerequisites = skill_prerequisites(skill);
    const auto met = [&](const SkillNeed &need)
    {
        return has(levels, need.skill, need.level);
    };
    return prerequisites.any ? std::ranges::any_of(prerequisites.needs, met)
                             : std::ranges::all_of(prerequisites.needs, met);
}

} // namespace

bool meets_skill_prerequisite(const SkillLevels &levels, Profession career, Skill skill)
{
    return skill_prerequisites(levels, career == P::medic, skill);
}

int starting_skill_level(const SkillLevels &levels, Profession career, Skill skill)
{
    constexpr int favored_start = 10;
    static constexpr std::array<Skill, 10> rogue_favored{
        S::bypass_security, S::open_lock, S::move_silently,   S::pick_pockets,
        S::hide_in_shadows, S::climb,     S::animal_training, S::read_lips,
        S::notice,          S::fast_talk};
    if (!meets_skill_prerequisite(levels, career, skill))
    {
        return 0;
    }
    const bool rocket_jock_favored =
        career == P::rocket_jock && skill >= S::pilot_rocket && skill <= S::drive_jetcar;
    const bool rogue =
        career == P::rogue && std::ranges::find(rogue_favored, skill) != rogue_favored.end();
    return rocket_jock_favored || rogue ? favored_start : 0;
}

std::span<const PresetSkill> preset_skills(Profession career, GoldBoxGame game)
{
    if (!matrix(game))
    {
        return {};
    }
    switch (career)
    {
    case P::rocket_jock:
        return rocket_jock_presets;
    case P::medic:
        return medic_presets;
    case P::warrior:
        return warrior_presets;
    case P::engineer:
        return engineer_presets;
    case P::rogue:
        return rogue_presets;
    case P::scout:
        return scout_presets;
    default:
        throw std::invalid_argument("Not a Buck Rogers career");
    }
}

int skill_ability_score(const Creature &character, Skill skill)
{
    if (!skill_prerequisites(character.skills, career_from_record(character.career) == P::medic,
                             skill))
    {
        return 0;
    }
    return character.abilities.at(static_cast<std::size_t>(skill_definition(skill).group));
}

int skill_value(const Creature &character, Skill skill)
{
    const int level = level_of(character.skills, skill);
    if (level == 0 || character.enabled == 0)
    {
        return 0;
    }
    return level + skill_ability_score(character, skill);
}

int scaled_skill_value(int value, int difficulty) noexcept
{
    switch (difficulty)
    {
    case 0:
        return value * 4;
    case 1:
        return value * 2;
    case 3:
        return value / 2;
    case 4:
        return value / 4;
    default:
        return value;
    }
}

SkillCheckGrade skill_check_grade(int scaled_value, int d100) noexcept
{
    const int margin = scaled_value - d100;
    if (margin >= 0)
    {
        return margin < scaled_value / 2 ? SkillCheckGrade::success
                                         : SkillCheckGrade::great_success;
    }
    return -margin > scaled_value ? SkillCheckGrade::bad_failure : SkillCheckGrade::failure;
}

SkillCheck check_skill(std::span<const int> values, int difficulty, RandomState &random)
{
    if (values.empty())
    {
        throw std::invalid_argument("A skill check needs someone to roll it");
    }
    std::size_t roller = 0;
    int best_margin = 0, roller_value = 0, roller_roll = 0;
    for (std::size_t member = 0; member < values.size(); ++member)
    {
        const int scaled = scaled_skill_value(values[member], difficulty);
        const int roll = roll_die(random, 100);
        if (member == 0 || scaled - roll > best_margin)
        {
            roller = member;
            best_margin = scaled - roll;
            roller_value = scaled;
            roller_roll = roll;
        }
    }
    return {roller, skill_check_grade(roller_value, roller_roll)};
}

unsigned script_check_skill(GoldBoxGame game, std::string_view archive, unsigned script,
                            std::uint16_t address, unsigned skill) noexcept
{
    for (const auto &fix : script_skill_fixes)
    {
        if (fix.game == game && fix.archive == archive && fix.script == script &&
            fix.address == address && fix.shipped == skill)
        {
            return fix.meant;
        }
    }
    return skill;
}

SkillSpending::SkillSpending(const Creature &character, GoldBoxGame game, int career_points,
                             int general_points, int max_per_skill)
    : game_(game), career_(career_of(character)), level_(character.level),
      levels_(character.skills), floors_(character.skills), career_points_(career_points),
      general_points_(general_points), max_per_skill_(max_per_skill),
      general_(general_skills(career_))
{
    count_known_general();
    const bool new_character = level_ == buck_rogers_starting_level(game_);
    max_general_ = new_character ? (matrix(game_) ? 12 : 7) : known_general_ + 1;
}

SkillSpending SkillSpending::at_creation(const Creature &character, GoldBoxGame game)
{
    auto made = character;
    const auto career = career_of(made);
    for (unsigned number = 1; number <= skill_count; ++number)
    {
        const auto skill = skill_numbered(number);
        level_of(made.skills, skill) =
            static_cast<std::uint8_t>(starting_skill_level(made.skills, career, skill));
    }
    if (!matrix(game))
    {
        return SkillSpending(made, game, 80, 40, 30);
    }
    for (const auto &preset : preset_skills(career, game))
    {
        level_of(made.skills, preset.skill) = static_cast<std::uint8_t>(preset.level);
    }
    SkillSpending spending(made, game, 0, 0, 105);
    for (unsigned number = 1; number <= skill_count; ++number)
    {
        const auto skill = skill_numbered(number);
        level_of(spending.floors_, skill) =
            static_cast<std::uint8_t>(spending.starting_level(skill));
    }
    return spending;
}

SkillSpending SkillSpending::at_training(const Creature &character, GoldBoxGame game)
{
    return SkillSpending(character, game, 40, 20, 15);
}

int SkillSpending::starting_level(Skill skill) const
{
    return starting_skill_level(levels_, career_, skill);
}

void SkillSpending::count_known_general()
{
    known_general_ = static_cast<int>(std::ranges::count_if(general_,
                                                            [this](Skill skill)
                                                            {
                                                                return level_of(levels_, skill) > 0;
                                                            }));
}

SkillRefusal SkillSpending::add(Skill skill)
{
    auto &level = level_of(levels_, skill);
    const int ceiling = level_of(floors_, skill) + max_per_skill_;
    if (is_career_skill(career_, skill))
    {
        if (career_points_ <= 0)
        {
            return SkillRefusal::no_points;
        }
        if (!meets_skill_prerequisite(levels_, career_, skill))
        {
            return SkillRefusal::prerequisites_unmet;
        }
        if (level >= ceiling)
        {
            return SkillRefusal::at_level_maximum;
        }
        --career_points_;
        ++level;
        adjust_dependents(skill, true, true);
        return SkillRefusal::none;
    }
    if (std::ranges::find(general_, skill) == general_.end())
    {
        return SkillRefusal::not_listed;
    }
    if (general_points_ <= 0)
    {
        return SkillRefusal::no_points;
    }
    if (level >= general_skill_limit)
    {
        return SkillRefusal::at_general_maximum;
    }
    if (level >= ceiling)
    {
        return SkillRefusal::at_level_maximum;
    }
    if (!meets_skill_prerequisite(levels_, career_, skill))
    {
        return SkillRefusal::prerequisites_unmet;
    }
    const bool learning = level == starting_level(skill);
    if (learning && known_general_ >= max_general_)
    {
        return SkillRefusal::no_new_skills;
    }
    if (learning)
    {
        ++known_general_;
    }
    --general_points_;
    ++level;
    adjust_dependents(skill, true, false);
    return SkillRefusal::none;
}

bool SkillSpending::subtract(Skill skill)
{
    const bool career_pool = is_career_skill(career_, skill);
    if (!career_pool && std::ranges::find(general_, skill) == general_.end())
    {
        return false;
    }
    auto &level = level_of(levels_, skill);
    if (level <= level_of(floors_, skill))
    {
        return false;
    }
    ++(career_pool ? career_points_ : general_points_);
    --level;
    // The original also counted a career skill back at its start as a
    // general skill forgotten, so a player could learn general skills past
    // the limit (docs/games/countdown.md, Bugs).
    if (!career_pool && level == starting_level(skill))
    {
        --known_general_;
    }
    adjust_dependents(skill, false, career_pool);
    return true;
}

void SkillSpending::adjust_dependents(Skill changed, bool raised, bool career_pool)
{
    for (const auto &rule : dependents)
    {
        if (rule.prerequisite == changed)
        {
            reset_dependent(changed, rule.required, rule.dependent, raised, career_pool);
        }
    }
}

void SkillSpending::reset_dependent(Skill changed, int required, Skill dependent, bool raised,
                                    bool career_pool)
{
    const int changed_level = level_of(levels_, changed);
    const int base = starting_level(dependent);
    auto &level = level_of(levels_, dependent);
    auto &floor = level_of(floors_, dependent);
    if (raised)
    {
        if (changed_level != required)
        {
            return;
        }
        level = static_cast<std::uint8_t>(base);
        floor = static_cast<std::uint8_t>(base);
    }
    else
    {
        if (changed_level != required - 1)
        {
            return;
        }
        // What was spent on the dependent: the original refunded it down to
        // its new start, 0, so a Rogue's free Animal Training paid out 10
        // points each time Befriend Animal went back below 20
        // (docs/games/countdown.md, Bugs).
        const int refund = level - floor;
        if (refund > 0)
        {
            (career_pool ? career_points_ : general_points_) += refund;
        }
        floor = static_cast<std::uint8_t>(base);
        level = 0;
    }
    count_known_general();
}

void SkillSpending::apply_to(Creature &character) const
{
    character.skills = levels_;
}

int specialization_level(const Creature &character, std::uint8_t item_type)
{
    return static_cast<int>(std::ranges::count(character.specializations, item_type));
}

std::vector<std::uint8_t> specializable_weapon_types(std::span<const Item> templates)
{
    constexpr std::size_t last_specializable_template = 15;
    std::vector<std::uint8_t> types;
    const auto offered = std::min(templates.size(), last_specializable_template);
    for (std::size_t index = offered; index-- > 0;)
    {
        if (templates[index].type != Item::no_type)
        {
            types.push_back(templates[index].type);
        }
    }
    return types;
}

bool specialize(Creature &character, std::uint8_t item_type)
{
    if (item_type == 0 || specialization_level(character, item_type) >= max_specialization)
    {
        return false;
    }
    const auto slot = std::ranges::find(character.specializations, std::uint8_t{0});
    if (slot == character.specializations.end())
    {
        return false;
    }
    *slot = item_type;
    return true;
}

bool unspecialize(Creature &character, std::uint8_t item_type)
{
    auto &slots = character.specializations;
    for (std::size_t at = slots.size(); item_type != 0 && at-- > 0;)
    {
        if (slots[at] == item_type)
        {
            // The slots stay filled from the first, as specialize fills them.
            std::shift_left(slots.begin() + static_cast<std::ptrdiff_t>(at), slots.end(), 1);
            slots.back() = 0;
            return true;
        }
    }
    return false;
}

int creation_specializations(Profession career, GoldBoxGame game)
{
    if (career != P::warrior)
    {
        return 0;
    }
    return matrix(game) ? 3 : 1;
}

bool specializes_at(Profession career, int new_level) noexcept
{
    return career == P::warrior && new_level % 2 == 0;
}

Creature new_buck_rogers_character(Race race, Gender gender, Profession career, GoldBoxGame game,
                                   RandomState &random)
{
    const auto careers = professions_for(race, game);
    if (std::ranges::find(careers, career) == careers.end())
    {
        throw std::invalid_argument("Creation does not offer that race and career");
    }
    Creature character;
    for (std::size_t slot = 0; slot < character.icon_sequence.size(); ++slot)
    {
        character.icon_sequence[slot] = static_cast<std::uint8_t>(slot);
    }
    character.base_armor_class = 50;
    character.armor_class[0] = 50;
    character.status = status_okay;
    character.enabled = 1;
    character.icon_dimensions = 1;
    character.order_number = 10;
    character.credits = 1000;
    character.race = static_cast<std::uint8_t>(record_race(race, game));
    character.gender = static_cast<std::uint8_t>(gender);
    character.career = record_career(career);
    const int start = buck_rogers_starting_level(game);
    character.experience = *career_experience_for_level(career, start, game);
    set_saves(character, starting_saving_throws(race));
    character.age = static_cast<std::uint8_t>(roll_die(random, 6) + 18);
    roll_and_train(character, race, career, game, random);
    return character;
}

void reroll_buck_rogers_character(Creature &character, GoldBoxGame game, RandomState &random)
{
    const auto race = race_of(character);
    set_saves(character, starting_saving_throws(race));
    roll_and_train(character, race, career_of(character), game, random);
}

std::uint8_t default_icon(Race race, Gender gender, Profession career, RandomState &random)
{
    static constexpr std::array<std::array<std::uint8_t, 5>, 2> career_icons{
        {{32, 0, 3, 7, 21}, {33, 1, 6, 15, 25}}};
    static constexpr std::array<std::uint8_t, 4> desert_runner_icons{16, 23, 24, 31};
    if (race == R::tinker)
    {
        return 17;
    }
    if (race == R::desert_runner)
    {
        return desert_runner_icons[static_cast<std::size_t>(roll_die(random, 4) - 1)];
    }
    const auto column = career_index(career);
    if (column >= career_icons[0].size())
    {
        throw std::invalid_argument("No icon for that career");
    }
    return career_icons[static_cast<std::size_t>(gender)][column];
}

std::vector<Item> buck_rogers_starting_items(std::span<const Item> templates)
{
    struct FreeItem
    {
        std::size_t template_number;
        std::uint8_t quantity;
    };

    static constexpr std::array<FreeItem, 2> free_items{{{7, 100}, {20, 0}}};
    std::vector<Item> items;
    for (const auto &free : free_items)
    {
        if (templates.size() < free.template_number)
        {
            throw std::invalid_argument("ITEM0 holds too few templates");
        }
        auto item = templates[free.template_number - 1];
        item.quantity = free.quantity;
        item.flags = static_cast<std::uint8_t>(item.flags | Item::readied);
        // Each goes to the front of the list.
        items.insert(items.begin(), item);
    }
    return items;
}

TrainingRefusal training_refusal(const Creature &character, GoldBoxGame game)
{
    if (character.status != status_okay)
    {
        return TrainingRefusal::not_conscious;
    }
    if (character.level >= buck_rogers_level_limit(game))
    {
        return TrainingRefusal::at_level_limit;
    }
    const auto next = career_experience_for_level(career_of(character), character.level + 1, game);
    if (!next || *next > character.experience)
    {
        return TrainingRefusal::not_enough_experience;
    }
    return TrainingRefusal::none;
}

void train_buck_rogers_character(Creature &character, GoldBoxGame game, RandomState &random)
{
    if (training_refusal(character, game) != TrainingRefusal::none)
    {
        throw std::logic_error("The character cannot train");
    }
    const auto career = career_of(character);
    gain_level(character, career, game, random);
    const auto next = career_experience_for_level(career, character.level + 1, game);
    if (next && *next <= character.experience)
    {
        character.experience = *next - 1;
    }
}

BuckRogersTrainingChoices::BuckRogersTrainingChoices(Creature trained, GoldBoxGame game)
    : character_(std::move(trained)), specializations_before_(character_.specializations),
      spending_(SkillSpending::at_training(character_, game))
{
    const bool slot_free = std::ranges::find(character_.specializations, std::uint8_t{0}) !=
                           character_.specializations.end();
    specializations_owed_ = specializes_at(career_of(character_), character_.level) && slot_free;
}

SkillRefusal BuckRogersTrainingChoices::add_skill(Skill skill)
{
    const auto refusal = spending_.add(skill);
    spending_.apply_to(character_);
    return refusal;
}

bool BuckRogersTrainingChoices::subtract_skill(Skill skill)
{
    const bool subtracted = spending_.subtract(skill);
    spending_.apply_to(character_);
    return subtracted;
}

bool BuckRogersTrainingChoices::specialize(std::uint8_t item_type)
{
    if (specializations_chosen_ >= specializations_owed_ ||
        !opengold::original::specialize(character_, item_type))
    {
        return false;
    }
    ++specializations_chosen_;
    return true;
}

bool BuckRogersTrainingChoices::unspecialize(std::uint8_t item_type)
{
    const auto before = std::ranges::count(specializations_before_, item_type);
    if (specialization_level(character_, item_type) <= before ||
        !opengold::original::unspecialize(character_, item_type))
    {
        return false;
    }
    --specializations_chosen_;
    return true;
}

bool buck_rogers_modifiable(const Creature &character, GoldBoxGame game)
{
    const auto career = career_from_record(character.career);
    return career && character.experience == career_experience_for_level(
                                                 *career, buck_rogers_starting_level(game), game);
}

ScoreRange buck_rogers_modify_range(Race race, Profession career, BuckRogersAbility ability)
{
    const auto index = static_cast<std::size_t>(ability);
    const int adjustment = race_ability_adjustments(race)[index];
    const int minimum = career_ability_minimums(career)[index];
    return {std::max(adjustment + 3, minimum), std::max(adjustment + 18, minimum)};
}

int buck_rogers_hit_point_minimum(const Creature &character)
{
    int each_level = std::max(
        constitution_level_bonus(ability_of(character, BuckRogersAbility::constitution)) + 1, 1);
    if (career_of(character) != P::warrior)
    {
        each_level = std::min(each_level, 3);
    }
    return each_level * character.level;
}

int buck_rogers_hit_point_maximum(const Creature &character)
{
    const auto career = career_of(character);
    int each_level =
        constitution_level_bonus(ability_of(character, BuckRogersAbility::constitution));
    if (career != P::warrior)
    {
        each_level = std::min(each_level, 2);
    }
    each_level += career == P::warrior ? 10 : career == P::scout || career == P::engineer ? 8 : 6;
    return each_level * character.level;
}

void modify_buck_rogers_ability(Creature &character, BuckRogersAbility ability,
                                rules::ModifyStep step)
{
    const auto index = static_cast<std::size_t>(ability);
    const auto range = buck_rogers_modify_range(race_of(character), career_of(character), ability);
    const int stepped = character.abilities[index] + (step == rules::ModifyStep::raise ? 1 : -1);
    character.abilities[index] =
        static_cast<std::uint8_t>(std::clamp(stepped, range.lowest, range.highest));
    if (ability != BuckRogersAbility::constitution)
    {
        return;
    }
    const int lowest = buck_rogers_hit_point_minimum(character);
    const int highest = buck_rogers_hit_point_maximum(character);
    if (lowest > character.hit_point_maximum)
    {
        character.hit_point_maximum = static_cast<std::uint16_t>(lowest);
    }
    else if (highest < character.hit_point_maximum)
    {
        character.hit_point_maximum = static_cast<std::uint16_t>(highest);
    }
    character.hit_points = character.hit_point_maximum;
}

void modify_buck_rogers_hit_points(Creature &character, rules::ModifyStep step)
{
    const int stepped = character.hit_point_maximum + (step == rules::ModifyStep::raise ? 1 : -1);
    const int clamped = std::max(std::min(stepped, buck_rogers_hit_point_maximum(character)),
                                 buck_rogers_hit_point_minimum(character));
    character.hit_point_maximum = static_cast<std::uint16_t>(clamped);
    character.hit_points = character.hit_point_maximum;
}

void keep_buck_rogers_modify(Creature &character)
{
    character.current_abilities = character.abilities;
}

} // namespace opengold::original
