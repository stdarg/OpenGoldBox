// Buck Rogers: Countdown to Doomsday, plan steps B1 and B2 (docs/BUCK-ROGERS.md):
// its script commands, its records, what its scripts ask of the engine and
// the rules of its characters (careers, skills, creation, training), first
// on made-up bytes, then on the game's own files when its folder is found
// (installed_games.h, docs/games/countdown.md). The whole census
// of the real files also runs in tests/game_census.cmake.
#include "opengold/buck_rogers_battlefield.h"
#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_scripts.h"
#include "opengold/campaign_encounter.h"
#include "opengold/difficulty.h"
#include "opengold/dice.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "installed_games.h"
#include "test_support.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;

constexpr auto countdown = GoldBoxGame::countdown_to_doomsday;

// A script record: two bytes the game skips, five entry jumps to the body at
// 8014h, then the body.
Bytes script_with_body(const Bytes &body)
{
    Bytes record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x02, 0x14, 0x80}); // GOTO 8014h
    }
    record.insert(record.end(), body.begin(), body.end());
    return record;
}

void the_commands_follow_the_buck_rogers_table()
{
    using opengold::por::ecl_opcode;
    check(ecl_opcode(0x1F, countdown).name == "SHIP COMBAT" &&
              ecl_opcode(0x1F, countdown).operands == 4,
          "1Fh starts a ship combat with four operands");
    check(ecl_opcode(0x22, countdown).name == "SKILL CHECK" &&
              ecl_opcode(0x22, countdown).operands == 3 &&
              ecl_opcode(0x49, countdown).name == "SKILL CHECK WITH DAMAGE" &&
              ecl_opcode(0x49, countdown).operands == 6,
          "The skill checks take three operands, the one with damage six");
    check(ecl_opcode(0x27, countdown).operands == 2 && ecl_opcode(0x27, countdown).variable_list,
          "TREASURE gives credits, then a list of item templates");
    check(ecl_opcode(0x4B, countdown).name == "SHOP" &&
              ecl_opcode(0x4C, countdown).name == "PICTURE MODE",
          "4Bh opens a shop and 4Ch sets the picture mode");
    check(opengold::test::throws(
              []
              {
                  static_cast<void>(ecl_opcode(0x4D, countdown));
              }),
          "The table ends at 4Ch");
    check(ecl_opcode(0x0C, GoldBoxGame::secret_of_the_silver_blades).operands == 3,
          "Secret of the Silver Blades keeps its own SETUP MONSTER");
    const auto memory = opengold::por::ecl_memory(countdown);
    check(memory.origin == 0x8000 && memory.limit == 0xA401,
          "Scripts load at 8000h and may run to A400h");
}

void four_byte_operands_decode()
{
    // SAVE #12345678h [4CE6h] with a four-byte cell; then EXIT.
    const auto record =
        script_with_body({0x09, 0x04, 0x78, 0x56, 0x34, 0x12, 0x05, 0xE6, 0x4C, 0x00});
    const auto program = opengold::por::EclProgram::decode(record, "made", countdown);
    const auto save = program.instruction(0x8014);
    check(save.opcode == 0x09 && save.operands[0].tag == 4 && save.operands[0].value == 0x5678 &&
              save.operands[0].high_word == 0x1234,
          "Tag 4 is a four-byte number");
    check(save.operands[1].tag == 5 && save.operands[1].value == 0x4CE6,
          "Tag 5 names a four-byte cell");
    check(program.instruction(save.next).opcode == 0x00, "The next command follows the operands");
    check(opengold::test::throws(
              [&]
              {
                  static_cast<void>(opengold::por::EclProgram::decode(
                                        record, "made", GoldBoxGame::secret_of_the_silver_blades)
                                        .instruction(0x8014));
              }),
          "The earlier games have no tag 4");
}

void the_empty_commands_never_go_on()
{
    // 3Bh, then a command the walk must never reach.
    const auto record = script_with_body({0x3B, 0x4D});
    const auto program = opengold::por::EclProgram::decode(record, "made", countdown);
    const auto walk = opengold::por::walk_reachable(program);
    check(walk.errors.empty() && walk.instructions.size() == 1 &&
              walk.instructions.back().opcode == 0x3B,
          "3Bh stays where it is: the walk stops on it");
}

Bytes made_creature()
{
    Bytes record(opengold::buck_rogers::Creature::size + 4);
    const std::string name = "HEXADILLO";
    record[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), record.begin() + 1);
    for (std::uint8_t ability = 0; ability < 7; ++ability)
    {
        record[16 + ability] = static_cast<std::uint8_t>(10 + ability);
    }
    record[39] = 0;  // A monster's race.
    record[40] = 3;  // Warrior.
    record[41] = 6;  // Level.
    record[55] = 59; // Armor Class.
    record[69] = 36; // Hit Points.
    record[77 + 9] = 40;
    record[178] = 0xC2; // 450 experience for defeating it.
    record[179] = 0x01;
    record[212] = 1; // Okay.
    record[227] = 36;
    return record;
}

void creatures_are_read_from_their_layout()
{
    const auto record = made_creature();
    const auto creature = opengold::buck_rogers::decode_creature(record);
    check(creature && creature->name == "HEXADILLO", "A 263-byte entry holds a creature");
    check(creature->abilities[0] == 10 && creature->abilities[6] == 16,
          "Seven abilities follow the name, Tech last");
    check(creature->career == 3 && creature->level == 6 && creature->base_armor_class == 59 &&
              creature->hit_point_maximum == 36 && creature->hit_points == 36,
          "Career, level, Armor Class and Hit Points sit at their places");
    check(creature->skills[9] == 40 && creature->experience_award == 450,
          "Skill 10's level and the experience it is worth");
    check(creature->raw.size() == opengold::buck_rogers::Creature::size,
          "The game reads 259 of the entry's bytes");
    auto wrong = record;
    wrong[40] = 7;
    check(!opengold::buck_rogers::decode_creature(wrong), "There are six careers");
    check(!opengold::buck_rogers::decode_creature(Bytes(300)), "Other sizes are not creatures");
}

void items_ships_and_types_are_read()
{
    Bytes templates(32);
    templates[0] = 7;
    templates[2] = 7;
    templates[3] = 8;
    templates[10] = 10;
    templates[11] = 210;
    templates[16] = opengold::buck_rogers::Item::no_type;
    const auto read = opengold::buck_rogers::decode_item_templates(templates);
    check(read && read->size() == 2 && read->front().type == 7 &&
              read->front().name_parts[2] == 8 && read->front().quantity == 10 &&
              read->front().value == 210,
          "A template is 16 bytes: type, name parts, then quantity and value");
    templates[0] = 129;
    check(!opengold::buck_rogers::decode_item_templates(templates), "ITEMS has 129 rows");

    Bytes items(opengold::buck_rogers::Item::size);
    items[0] = 200; // The name's leftover bytes are never read.
    items[46] = 13;
    const auto carried = opengold::buck_rogers::decode_items(items);
    check(carried && carried->size() == 1 && carried->front().type == 13,
          "An item's template is its last 16 bytes");

    const auto types = opengold::buck_rogers::decode_item_types(Bytes(18, 1));
    check(types && types->size() == 2 && types->back().dice_sides == 1, "ITEMS rows are 9 bytes");

    Bytes ship(opengold::buck_rogers::Ship::size);
    const std::string name = "Adder";
    ship[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), ship.begin() + 1);
    ship[22 + 8] = 0xC2; // Fuel 450.
    ship[22 + 9] = 0x01;
    ship[57] = 6;
    ship[60] = 0x81;
    const auto decoded = opengold::buck_rogers::decode_ship(ship);
    check(decoded && decoded->name == "Adder" && decoded->system_status[4] == 450 &&
              decoded->type == 6 && decoded->weapons[0].kind == 0x81,
          "A ship holds six systems, its type and eight weapon mounts");
}

// The rules of step B2 (docs/games/countdown.md, "Rules"). Each test names
// the routine of the original it follows.

namespace br = opengold::buck_rogers;
namespace og = opengold::original;
using og::BuckRogersAbility;
using og::Profession;
using og::Race;
using og::Skill;
using og::SkillRefusal;

int score(const br::Creature &character, BuckRogersAbility ability)
{
    return character.abilities[static_cast<std::size_t>(ability)];
}

// A made character of a career at a level, with these skill levels.
br::Creature made_character(Profession career, int level, Race race = Race::terran)
{
    br::Creature character;
    character.race = static_cast<std::uint8_t>(og::record_race(race, countdown));
    character.career = og::record_career(career);
    character.level = level;
    character.enabled = 1;
    character.status = 1;
    character.abilities = {12, 14, 12, 13, 11, 10, 15};
    return character;
}

void the_menus_pair_races_and_careers()
{
    // chargen.pas CreateCharacterMenu; gdconsts.pas raceCareers.
    const auto races = og::buck_rogers_creation_races();
    check(std::vector<Race>(races.begin(), races.end()) ==
              std::vector<Race>{Race::terran, Race::martian, Race::venusian, Race::mercurian,
                                Race::tinker, Race::desert_runner},
          "Creation offers Terran, Martian, Venusian, Mercurian, Tinker and Desert Runner");
    const auto tinker = og::professions_for(Race::tinker, countdown);
    check(std::vector<Profession>(tinker.begin(), tinker.end()) ==
              std::vector<Profession>{Profession::medic, Profession::engineer},
          "A Tinker may be a Medic or an Engineer");
    const auto runner = og::professions_for(Race::desert_runner, countdown);
    check(std::vector<Profession>(runner.begin(), runner.end()) ==
              std::vector<Profession>{Profession::rocket_jock, Profession::warrior,
                                      Profession::engineer},
          "A Desert Runner may be a Rocketjock, a Warrior or an Engineer");
    for (const auto race : {Race::terran, Race::martian, Race::venusian, Race::mercurian})
    {
        const auto careers = og::professions_for(race, countdown);
        check(careers.size() == 5 && std::ranges::find(careers, Profession::scout) == careers.end(),
              "The human races take five careers, never the Scout");
    }
    check(og::professions_for(Race::lunarian, countdown).empty() &&
              og::professions_for(Race::human, countdown).empty() &&
              og::professions_for(Race::terran, GoldBoxGame::pool_of_radiance).empty(),
          "No other race is offered, and the other games offer no Buck Rogers race");
    check(og::record_race(Race::terran, countdown) == 1 &&
              og::record_race(Race::lowlander, countdown) == 8 &&
              og::race_from_record(6, countdown) == Race::desert_runner &&
              og::race_from_record(0, countdown) == Race::monster,
          "Races keep the record's numbers, 0 the monsters'");
    check(og::record_career(Profession::rocket_jock) == 1 &&
              og::career_from_record(3) == Profession::warrior && !og::career_from_record(0) &&
              !og::career_from_record(7),
          "Careers are numbered 1 to 6, Rocketjock first");
    check(og::record_race(Race::human, GoldBoxGame::pool_of_radiance) == 7 &&
              og::race_from_record(1, GoldBoxGame::curse_of_the_azure_bonds) == Race::dwarf,
          "The other games keep their race numbers");
}

void abilities_are_rolled_and_raised()
{
    // chargen.pas CreateCharacterMenu; gdconsts.pas raceStatAdjust, careerStatMinimum.
    check(og::race_ability_adjustments(Race::tinker) == og::BuckRogersScores{-2, 3, -2, 0, 0, 0, 3},
          "A Tinker is weaker and tougher-handed: -2 Strength, +3 Dexterity, +3 Tech");
    check(og::career_ability_minimums(Profession::engineer) ==
              og::BuckRogersScores{10, 1, 12, 8, 1, 1, 13},
          "An Engineer needs 10 Strength, 12 Constitution, 8 Intelligence and 13 Tech");
    opengold::RandomState random{17};
    for (int roll = 0; roll < 200; ++roll)
    {
        const auto scores =
            og::roll_buck_rogers_abilities(Race::tinker, Profession::engineer, random);
        const auto minimums = og::career_ability_minimums(Profession::engineer);
        const auto adjustments = og::race_ability_adjustments(Race::tinker);
        for (std::size_t ability = 0; ability < 7; ++ability)
        {
            check(scores[ability] >= std::max(1, minimums[ability]) &&
                      scores[ability] <= std::max(18 + adjustments[ability], minimums[ability]),
                  "Each score is the best 3d6 with the race's adjustment, raised to the minimum");
        }
    }
}

void the_tables_run_to_the_level_limit()
{
    // gdconsts.pas experienceNeeded, careerBaseCombat; c_career.pas MAX_LEVEL.
    check(og::buck_rogers_level_limit(countdown) == 8 &&
              og::buck_rogers_starting_level(countdown) == 2,
          "Countdown starts at level 2 and stops at 8");
    check(og::career_experience_for_level(Profession::rocket_jock, 2, countdown) == 1250U &&
              og::career_experience_for_level(Profession::warrior, 9, countdown) == 250000U &&
              og::career_experience_for_level(Profession::medic, 1, countdown) == 0U &&
              !og::career_experience_for_level(Profession::warrior, 10, countdown),
          "Each career has its own table, to level 9");
    check(og::career_base_combat(Profession::warrior, 9, countdown) == 48 &&
              og::career_base_combat(Profession::rocket_jock, 9, countdown) == 44 &&
              og::career_base_combat(Profession::medic, 2, countdown) == 40,
          "Combat value rises from 40 to 44, or 48 for a Warrior");
    check(opengold::test::throws(
              []
              {
                  static_cast<void>(og::career_base_combat(Profession::warrior, 10, countdown));
              }),
          "Countdown's combat values stop at level 9");
}

void hit_points_follow_the_career()
{
    // chargen.pas AddLevelHitPoints; gdconsts.pas constitutionLevelBonus.
    check(og::constitution_level_bonus(14) == 0 && og::constitution_level_bonus(18) == 4 &&
              og::constitution_level_bonus(3) == -2,
          "Constitution adds from 15 and takes away below 7");
    opengold::RandomState random{5};
    for (int roll = 0; roll < 300; ++roll)
    {
        const int warrior =
            og::roll_level_hit_points(Profession::warrior, 3, 18, countdown, random);
        const int medic = og::roll_level_hit_points(Profession::medic, 3, 18, countdown, random);
        const int engineer =
            og::roll_level_hit_points(Profession::engineer, 3, 3, countdown, random);
        check(warrior >= 5 && warrior <= 14, "A Warrior rolls a d10 with the whole bonus");
        check(medic >= 3 && medic <= 8, "Others roll a d6 with at most 2");
        check(engineer >= 1 && engineer <= 6, "An Engineer's d8 less 2, at least 1");
    }
    check(og::roll_level_hit_points(Profession::warrior, 9, 10, countdown, random) == 3 &&
              og::roll_level_hit_points(Profession::rogue, 9, 10, countdown, random) == 2,
          "From level 9 the gain is fixed: 3 for a Warrior, 2 for the rest");
}

void saving_throws_follow_race_and_level()
{
    // chargen.pas InitializeSaves, TrainingMenu; gdconsts.pas baseSavingThrows, raceSaveAdjust.
    check(og::starting_saving_throws(Race::terran) ==
              std::array<int, 8>{14, 14, 14, 13, 13, 16, 13, 13},
          "A Terran's first save is one better than the base");
    check(og::starting_saving_throws(Race::mercurian) ==
              std::array<int, 8>{14, 14, 14, 13, 10, 15, 9, 16},
          "Each race adjusts the eight saves");
    check(og::buck_rogers_saving_throws(Race::terran, 6) ==
              std::array<int, 8>{12, 12, 12, 11, 11, 14, 11, 11},
          "Every third level improves each save by one");
    check(og::buck_rogers_movement(Race::tinker) == 9 &&
              og::buck_rogers_movement(Race::desert_runner) == 15 &&
              og::buck_rogers_movement(Race::venusian) == 12,
          "Tinkers move 9, Desert Runners 15, the rest 12");
}

void creation_makes_every_pair()
{
    // chargen.pas CreateCharacterMenu, TrainingMenu (free training), HpMinimum, HpMaximum.
    opengold::RandomState random{2026};
    for (const auto race : og::buck_rogers_creation_races())
    {
        for (const auto career : og::professions_for(race, countdown))
        {
            for (const auto gender : {og::Gender::male, og::Gender::female})
            {
                const auto made =
                    og::new_buck_rogers_character(race, gender, career, countdown, random);
                check(made.level == 2 &&
                          made.experience ==
                              og::career_experience_for_level(career, 2, countdown) &&
                          made.credits == 1000 && made.age >= 19 && made.age <= 24 &&
                          made.base_armor_class == 50 && made.armor_class[0] == 50 &&
                          made.status == 1 && made.enabled == 1,
                      "A new character is level 2 with its career's experience, 1,000 credits, "
                      "age 1d6 + 18 and Armor Class 50");
                check(made.base_combat == og::career_base_combat(career, 2, countdown) &&
                          made.base_movement == og::buck_rogers_movement(race),
                      "It has its career's combat value and its race's movement");
                check(made.hit_points == made.hit_point_maximum &&
                          made.hit_point_maximum >= og::buck_rogers_hit_point_minimum(made) &&
                          made.hit_point_maximum <= og::buck_rogers_hit_point_maximum(made) &&
                          made.hit_point_maximum >= 2,
                      "Two levels of Hit Points, within what Modify allows");
                std::array<int, 8> saves{};
                std::copy(made.saves.begin(), made.saves.end(), saves.begin());
                check(saves == og::starting_saving_throws(race), "Levels 1 and 2 leave the saves");
                const bool warrior = career == Profession::warrior;
                const bool runner = race == Race::desert_runner;
                check(made.unarmed_dice_count[0] == 1 &&
                          made.unarmed_dice_sides[0] == (warrior || runner ? 3 : 2) &&
                          made.unarmed_damage_bonus[0] == (runner ? 1 : 0) &&
                          made.unarmed_attacks_doubled == (warrior ? 4 : 2),
                      "A Warrior punches twice for 1d3, a Desert Runner's claws add a point");
                check(score(made, BuckRogersAbility::tech) >= 1 &&
                          score(made, BuckRogersAbility::tech) >=
                              og::career_ability_minimums(career)[6],
                      "The seventh ability, Tech, is rolled too");
                check(og::buck_rogers_modifiable(made, countdown), "Modify takes a new character");
                const auto round_trip = br::decode_creature(br::encode_creature(made));
                check(round_trip && br::encode_creature(*round_trip) == br::encode_creature(made) &&
                          round_trip->career == og::record_career(career),
                      "A new character saves as a 259-byte record and reads back");
            }
        }
    }
    check(opengold::test::throws<std::invalid_argument>(
              [&]
              {
                  static_cast<void>(og::new_buck_rogers_character(
                      Race::tinker, og::Gender::male, Profession::warrior, countdown, random));
              }),
          "Creation refuses a pair the menus do not offer");
}

void training_raises_a_level()
{
    // chargen.pas TrainingMenu, AddLevelHitPoints.
    opengold::RandomState random{9};
    auto character = og::new_buck_rogers_character(Race::terran, og::Gender::female,
                                                   Profession::rogue, countdown, random);
    check(og::training_refusal(character, countdown) == og::TrainingRefusal::not_enough_experience,
          "A new character lacks the experience to train");
    character.experience = 9000;
    const auto before = character;
    og::train_buck_rogers_character(character, countdown, random);
    check(character.level == 3 && character.experience == 4999 &&
              character.hit_point_maximum > before.hit_point_maximum &&
              character.saves[0] == before.saves[0] - 1,
          "Training gives one level, cuts the experience to one short of the next and, at level "
          "3, improves the saves");
    character.status = 5;
    check(og::training_refusal(character, countdown) == og::TrainingRefusal::not_conscious,
          "Only the conscious train");
    character.status = 1;
    character.level = 8;
    character.experience = 500000;
    check(og::training_refusal(character, countdown) == og::TrainingRefusal::at_level_limit,
          "Level 8 is Countdown's last");
    check(og::specializes_at(Profession::warrior, 4) &&
              !og::specializes_at(Profession::warrior, 3) &&
              !og::specializes_at(Profession::rogue, 4) &&
              og::creation_specializations(Profession::warrior, countdown) == 1 &&
              og::creation_specializations(Profession::medic, countdown) == 0,
          "A Warrior specializes once at creation and at every even level");
}

void skills_have_groups_and_prerequisites()
{
    // gdconsts.pas skillDefinitions, careerSkills; charhlp.pas MeetsSkillPrerequisite;
    // skills.pas StartingSkillLevel.
    check(og::skill_definition(Skill::repair_electrical).name == "Repair Electrical" &&
              og::skill_definition(Skill::planetary_survival).group == og::SkillGroup::wisdom &&
              og::skill_definition(Skill::pilot_rocket).group == og::SkillGroup::dexterity &&
              !og::skill_definition(Skill::hypnosis).offered &&
              og::skill_numbered(84) == Skill::planetary_survival,
          "84 skills in their groups, some never offered");
    const auto offered =
        std::ranges::count_if(std::views::iota(1U, 85U),
                              [](unsigned number)
                              {
                                  return og::skill_definition(og::skill_numbered(number)).offered;
                              });
    check(offered == 54, "54 skills are offered");
    const auto engineer = og::career_skills(Profession::engineer);
    check(engineer.size() == 8 && engineer[0] == Skill::notice && engineer[6] == Skill::jury_rig,
          "Each career has eight career skills");
    const auto general = og::general_skills(Profession::warrior);
    check(std::ranges::find(general, Skill::treat_light_wounds) == general.end() &&
              std::ranges::find(general, Skill::leadership) == general.end() &&
              std::ranges::find(general, Skill::programming) != general.end(),
          "The general lists leave out career, medical and unoffered skills");
    og::SkillLevels levels{};
    check(!og::meets_skill_prerequisite(levels, Profession::medic, Skill::treat_serious_wounds) &&
              og::meets_skill_prerequisite(levels, Profession::medic, Skill::treat_light_wounds) &&
              !og::meets_skill_prerequisite(levels, Profession::warrior, Skill::treat_light_wounds),
          "Medical skills are a Medic's, Serious Wounds after Light ones");
    og::level_of(levels, Skill::repair_electrical) = 30;
    og::level_of(levels, Skill::repair_mechanical) = 24;
    check(!og::meets_skill_prerequisite(levels, Profession::engineer, Skill::repair_life_support),
          "Life Support needs 30 Electrical and 25 Mechanical");
    og::level_of(levels, Skill::repair_mechanical) = 25;
    check(og::meets_skill_prerequisite(levels, Profession::engineer, Skill::repair_life_support),
          "With both it may be learned");
    check(og::starting_skill_level({}, Profession::rocket_jock, Skill::pilot_rocket) == 10 &&
              og::starting_skill_level({}, Profession::rocket_jock, Skill::notice) == 0 &&
              og::starting_skill_level({}, Profession::rogue, Skill::notice) == 10 &&
              og::starting_skill_level({}, Profession::rogue, Skill::animal_training) == 0,
          "A Rocketjock's piloting and a Rogue's favored skills start at 10");
}

void a_skill_value_adds_its_ability()
{
    // charhlp.pas SkillValue, SkillAbilityScore.
    auto character = made_character(Profession::engineer, 2);
    og::level_of(character.skills, Skill::repair_mechanical) = 20;
    check(og::skill_value(character, Skill::repair_mechanical) == 35,
          "A technical skill adds Tech (15) to its level");
    og::level_of(character.skills, Skill::repair_nuclear_engine) = 12;
    og::level_of(character.skills, Skill::repair_mechanical) = 5;
    check(og::skill_value(character, Skill::repair_nuclear_engine) == 12,
          "Without its prerequisite a skill adds no ability");
    check(og::skill_value(character, Skill::notice) == 0, "A skill with no level has no value");
    og::level_of(character.skills, Skill::maneuver_in_zero_g) = 125;
    check(og::skill_value(character, Skill::maneuver_in_zero_g) == 139,
          "A value past 127 stays a value (the original's turned negative)");
    character.enabled = 0;
    check(og::skill_value(character, Skill::maneuver_in_zero_g) == 0,
          "A character who cannot act brings no skill");
}

void skill_checks_have_four_grades()
{
    // charhlp.pas SelectSkillCharacter.
    check(og::scaled_skill_value(30, 0) == 120 && og::scaled_skill_value(30, 1) == 60 &&
              og::scaled_skill_value(30, 2) == 30 && og::scaled_skill_value(30, 3) == 15 &&
              og::scaled_skill_value(30, 4) == 7,
          "Difficulty 0 to 4 scales the value from four times to a quarter");
    using G = og::SkillCheckGrade;
    check(og::skill_check_grade(60, 10) == G::great_success &&
              og::skill_check_grade(60, 31) == G::success &&
              og::skill_check_grade(60, 60) == G::success &&
              og::skill_check_grade(60, 61) == G::failure &&
              og::skill_check_grade(40, 81) == G::bad_failure &&
              og::skill_check_grade(40, 80) == G::failure,
          "Succeeding by half the value is great; missing by more than the value is bad");
    for (std::uint64_t seed = 1; seed < 400; ++seed)
    {
        const std::array values{70, 5, 40};
        opengold::RandomState random{seed};
        auto replay = random;
        const auto result = og::check_skill(values, 2, random);
        std::size_t best = 0;
        int best_margin = 0, best_roll = 0;
        for (std::size_t member = 0; member < values.size(); ++member)
        {
            const int roll = opengold::roll_die(replay, 100);
            if (member == 0 || values[member] - roll > best_margin)
            {
                best = member;
                best_margin = values[member] - roll;
                best_roll = roll;
            }
        }
        check(result.roller == best, "The member who rolls best acts");
        check(result.grade == og::skill_check_grade(values[best], best_roll),
              "The grade is the roller's, not the last member's");
    }
}

void the_hail_checks_the_skills_it_names()
{
    // The hail menu of ECL2 script 34 (the user, 2026-11-20: fix it as a bug).
    check(og::script_check_skill(countdown, "ECL2.DAX", 34, 0x8DE8, 74) == 76 &&
              og::script_check_skill(countdown, "ECL2.DAX", 34, 0x8E67, 70) == 72 &&
              og::script_check_skill(countdown, "ECL2.DAX", 34, 0x8ED1, 69) == 71,
          "Bluff, intimidate and ask for help check Fast Talk, Intimidate and Act");
    check(og::script_check_skill(countdown, "ECL2.DAX", 34, 0x8DE8, 9) == 9 &&
              og::script_check_skill(countdown, "ECL3.DAX", 34, 0x8DE8, 74) == 74 &&
              og::script_check_skill(GoldBoxGame::matrix_cubed, "ECL2.DAX", 34, 0x8DE8, 74) == 74 &&
              og::script_check_skill(countdown, "ECL2.DAX", 34, 0x95FA, 11) == 11,
          "Every other check, and a changed operand, asks what it names");
}

void creation_spends_skill_points()
{
    // skills.pas EditSkills, AdjustPrerequisites; chargen.pas CreateCharacterMenu.
    opengold::RandomState random{44};
    const auto rogue = og::new_buck_rogers_character(Race::martian, og::Gender::male,
                                                     Profession::rogue, countdown, random);
    auto spending = og::SkillSpending::at_creation(rogue, countdown);
    check(spending.career_points() == 80 && spending.general_points() == 40 &&
              spending.max_per_skill() == 30 &&
              og::level_of(spending.levels(), Skill::notice) == 10,
          "Creation gives 80 career and 40 general points, at most 30 a skill");
    for (int point = 0; point < 30; ++point)
    {
        check(spending.add(Skill::climb) == SkillRefusal::none, "Career points buy career skills");
    }
    check(spending.add(Skill::climb) == SkillRefusal::at_level_maximum, "At most 30 a skill");
    check(spending.add(Skill::treat_light_wounds) == SkillRefusal::not_listed,
          "A Rogue has no medical skills");
    check(spending.add(Skill::programming) == SkillRefusal::prerequisites_unmet,
          "Programming needs Mathematics");
    // The original counted a career skill back at its start as a general skill
    // forgotten; here the count of general skills stays true.
    check(spending.add(Skill::open_lock) == SkillRefusal::none &&
              spending.subtract(Skill::open_lock) && spending.known_general_skills() == 0,
          "Lowering a career skill forgets no general skill");
    const std::array learned{Skill::mathematics, Skill::astronomy, Skill::acrobatics,
                             Skill::disguise,    Skill::etiquette, Skill::singing,
                             Skill::tracking};
    for (const auto skill : learned)
    {
        check(spending.add(skill) == SkillRefusal::none, "Seven new general skills at level 2");
    }
    check(spending.add(Skill::shadowing) == SkillRefusal::no_new_skills, "An eighth is refused");
    check(spending.add(Skill::mathematics) == SkillRefusal::none, "A known one may still rise");

    // Lowering a prerequisite refunds what its dependent cost.
    auto general = og::SkillSpending::at_creation(rogue, countdown);
    for (int point = 0; point < 20; ++point)
    {
        check(general.add(Skill::befriend_animal) == SkillRefusal::none, "Befriend Animal rises");
    }
    check(og::level_of(general.levels(), Skill::animal_training) == 10,
          "A Rogue's Animal Training starts at 10 once Befriend Animal reaches 20");
    check(general.subtract(Skill::befriend_animal) && general.general_points() == 21 &&
              og::level_of(general.levels(), Skill::animal_training) == 0,
          "Going back below 20 drops it and refunds only what was spent (the original paid 10)");
}

void dependents_are_refunded()
{
    // skills.pas AdjustPrerequisites.
    opengold::RandomState random{3};
    const auto engineer = og::new_buck_rogers_character(Race::terran, og::Gender::male,
                                                        Profession::engineer, countdown, random);
    auto spending = og::SkillSpending::at_creation(engineer, countdown);
    for (int point = 0; point < 10; ++point)
    {
        check(spending.add(Skill::repair_mechanical) == SkillRefusal::none, "Mechanical rises");
    }
    for (int point = 0; point < 5; ++point)
    {
        check(spending.add(Skill::repair_rocket_hull) == SkillRefusal::none, "Hull is open at 10");
    }
    check(spending.career_points() == 65 && spending.subtract(Skill::repair_mechanical) &&
              og::level_of(spending.levels(), Skill::repair_rocket_hull) == 0 &&
              spending.career_points() == 71,
          "Mechanical back to 9 takes Hull to 0 and refunds its 5 points");

    const auto medic = og::new_buck_rogers_character(Race::venusian, og::Gender::female,
                                                     Profession::medic, countdown, random);
    auto treating = og::SkillSpending::at_creation(medic, countdown);
    for (int point = 0; point < 30; ++point)
    {
        static_cast<void>(treating.add(Skill::treat_light_wounds));
    }
    for (int point = 0; point < 30; ++point)
    {
        static_cast<void>(treating.add(Skill::treat_serious_wounds));
    }
    check(og::level_of(treating.levels(), Skill::treat_serious_wounds) == 30,
          "Serious Wounds opens at 30 Light");
    check(treating.subtract(Skill::treat_light_wounds) &&
              og::level_of(treating.levels(), Skill::treat_serious_wounds) == 0 &&
              treating.career_points() == 80 - 29,
          "Light below 30 takes Serious Wounds back");
}

void training_spends_fewer_points()
{
    // chargen.pas TrainingMenu: EditSkills(40, 20, 15).
    auto character = made_character(Profession::warrior, 3);
    og::level_of(character.skills, Skill::programming) = 80;
    og::level_of(character.skills, Skill::mathematics) = 10;
    auto spending = og::SkillSpending::at_training(character, countdown);
    check(spending.career_points() == 40 && spending.general_points() == 20 &&
              spending.max_per_skill() == 15,
          "Training gives 40 career and 20 general points, at most 15 a skill");
    check(spending.add(Skill::programming) == SkillRefusal::at_general_maximum,
          "A general skill stops at 80");
    check(spending.add(Skill::acrobatics) == SkillRefusal::none &&
              spending.add(Skill::disguise) == SkillRefusal::no_new_skills,
          "One new general skill a level");
    check(!spending.subtract(Skill::programming), "Points spent before stay spent");
    spending.apply_to(character);
    check(og::level_of(character.skills, Skill::acrobatics) == 1, "The character keeps the levels");
}

void warriors_specialize()
{
    // viewchar.pas SpecializationMenu; charhlp.pas SpecializationLevel.
    auto character = made_character(Profession::warrior, 2);
    for (int time = 0; time < 3; ++time)
    {
        check(og::specialize(character, 7), "Up to three specializations in one weapon");
    }
    check(!og::specialize(character, 7) && og::specialization_level(character, 7) == 3,
          "A fourth is refused");
    for (std::uint8_t type = 1; type <= 7; ++type)
    {
        check(og::specialize(character, type == 7 ? 8 : type) || type == 7,
              "Others fill the empty slots");
    }
    check(!og::specialize(character, 9), "Ten slots in all");
    std::vector<br::Item> templates(20);
    for (std::size_t index = 0; index < templates.size(); ++index)
    {
        templates[index].type = static_cast<std::uint8_t>(index + 1);
    }
    templates[3].type = br::Item::no_type;
    const auto offered = og::specializable_weapon_types(templates);
    check(offered.size() == 14 && offered.front() == 15 && offered.back() == 1,
          "The menu offers templates 15 down to 1, the empty ones left out");
}

void modify_keeps_the_limits()
{
    // chargen.pas ModifyCharacterMenu, HpMinimum, HpMaximum.
    opengold::RandomState random{21};
    auto character = og::new_buck_rogers_character(Race::tinker, og::Gender::male,
                                                   Profession::engineer, countdown, random);
    const auto range = og::buck_rogers_modify_range(Race::tinker, Profession::engineer,
                                                    BuckRogersAbility::strength);
    check(range.lowest == 10 && range.highest == 16,
          "A Tinker Engineer's Strength runs from the career's 10 to the race's 16");
    for (int step = 0; step < 30; ++step)
    {
        og::modify_buck_rogers_ability(character, BuckRogersAbility::strength,
                                       opengold::rules::ModifyStep::lower);
    }
    check(score(character, BuckRogersAbility::strength) == 10, "No lower than the minimum");
    for (int step = 0; step < 30; ++step)
    {
        og::modify_buck_rogers_hit_points(character, opengold::rules::ModifyStep::raise);
    }
    check(character.hit_point_maximum == og::buck_rogers_hit_point_maximum(character) &&
              character.hit_points == character.hit_point_maximum,
          "The Hit Points stop at the career's die and the bonus, per level");
    for (int step = 0; step < 30; ++step)
    {
        og::modify_buck_rogers_ability(character, BuckRogersAbility::constitution,
                                       opengold::rules::ModifyStep::lower);
    }
    check(character.hit_point_maximum <= og::buck_rogers_hit_point_maximum(character),
          "A lower Constitution lowers the Hit Points with it");
    og::keep_buck_rogers_modify(character);
    check(character.current_abilities == character.abilities, "Keep sets the current scores");
    character.experience += 1;
    check(!og::buck_rogers_modifiable(character, countdown), "Experience earned ends Modify");
    check(og::modify_allowed(2000, countdown) && !og::modify_allowed(2001, countdown),
          "modify_allowed knows each career's starting experience");
}

void a_new_character_carries_two_items()
{
    // shop.pas BuildAmmoItems; gdconsts.pas freeAmmoTemplates, freeAmmoQuantities.
    std::vector<br::Item> templates(20);
    templates[6].type = 7;
    templates[6].quantity = 10;
    templates[19].type = 19;
    templates[19].quantity = 5;
    const auto items = og::buck_rogers_starting_items(templates);
    check(items.size() == 2 && items[0].type == 19 && items[0].quantity == 0 &&
              items[1].type == 7 && items[1].quantity == 100 &&
              (items[0].flags & br::Item::readied) != 0 &&
              (items[1].flags & br::Item::readied) != 0,
          "Template 20 then template 7 with 100 rounds, both readied");
    opengold::RandomState random{1};
    check(og::default_icon(Race::terran, og::Gender::female, Profession::warrior, random) == 6 &&
              og::default_icon(Race::tinker, og::Gender::male, Profession::medic, random) == 17,
          "The body icon follows career and gender, a Tinker's its own");
}

void records_round_trip()
{
    auto record = made_creature();
    record.resize(br::Creature::size);
    record[180] = 0xAB; // A pointer's leftover byte, which no field names.
    auto creature = br::decode_creature(record);
    check(creature && br::encode_creature(*creature) == record,
          "A creature encodes back to its bytes, leftovers included");
    creature->skills[0] = 33;
    creature->specializations[0] = 7;
    creature->age = 22;
    const auto changed = br::decode_creature(br::encode_creature(*creature));
    check(changed && changed->skills[0] == 33 && changed->specializations[0] == 7 &&
              changed->age == 22,
          "The fields the rules change are written in their places");
    creature->name = "A NAME TOO LONG!";
    check(opengold::test::throws<std::invalid_argument>(
              [&]
              {
                  static_cast<void>(br::encode_creature(*creature));
              }),
          "A name holds 15 letters");
    Bytes items(2 * br::Item::size);
    for (std::size_t at = 0; at < items.size(); ++at)
    {
        items[at] = static_cast<std::uint8_t>(at % 7);
    }
    items[46] = 13;
    items[br::Item::size + 46] = br::Item::no_type;
    const auto decoded = br::decode_items(items);
    check(decoded && br::encode_items(*decoded) == items, "Items encode back to their bytes");
}

// Ground combat (step B2; docs/games/countdown.md, "Ground combat"): each
// check names the routine of the original it follows.

// ITEMS' rows the checks use, as the game's file has them (checked against
// it in the_area_weapons_are_the_games).
std::vector<br::ItemType> ground_types()
{
    std::vector<br::ItemType> types(39);
    const auto row = [&types](std::size_t type, br::ItemType values)
    {
        types[type] = values;
    };
    // slot, attacks doubled, class or armor, heavy, dice, sides, bonus, range, consumable
    row(2, {0, 4, 0, 0, 1, 6, 0, 0, 0});
    row(7, {0, 8, 1, 0, 1, 4, 0, 16, 0});
    row(9, {0, 6, 3, 0, 1, 8, 0, 32, 0});
    row(10, {0, 8, 2, 0, 1, 10, 0, 16, 0});
    row(12, {0, 8, 4, 0, 1, 10, 0, 16, 0});
    row(15, {0, 4, 6, 0, 0, 0, 0, 4, 0});
    row(16, {6, 0, 0, 1, 0, 0, 0, 0, 0});
    row(17, {0, 2, 9, 1, 4, 10, 0, 20, 0});
    row(18, {0, 2, 7, 1, 5, 10, 0, 40, 0});
    row(22, {1, 0, 58, 0, 0, 0, 0, 0, 0});
    row(24, {1, 0, 62, 0, 0, 0, 0, 0, 0});
    row(26, {7, 0, 0, 0, 0, 0, 0, 0, 0});
    row(27, {3, 0, 0, 0, 0, 0, 0, 0, 0});
    row(28, {4, 0, 0, 0, 0, 0, 0, 0, 0});
    row(30, {2, 0, 0, 0, 0, 0, 0, 0, 0});
    row(33, {0, 2, 8, 0, 4, 10, 0, 3, 1});
    row(34, {0, 2, 8, 0, 0, 0, 0, 3, 1});
    row(35, {0, 2, 8, 0, 1, 4, 2, 3, 1});
    row(36, {0, 2, 8, 0, 1, 3, 0, 3, 1});
    row(37, {0, 2, 8, 0, 0, 0, 0, 3, 1});
    return types;
}

// An item of a type, readied or not, with its rounds and the projectile it
// fires (its special bytes).
br::Item ground_item(std::uint8_t type, int quantity = 0, bool readied = true, int kind = 0)
{
    br::Item item;
    item.type = type;
    item.quantity = static_cast<std::uint8_t>(quantity);
    // Grenades come in bundles; the rest are not joined (C_Item.NOT_JOINABLE).
    const bool grenade = type >= 32 && type <= 38;
    item.flags =
        static_cast<std::uint8_t>((readied ? br::Item::readied : 0) | (grenade ? 0 : 0x20));
    if (kind != 0)
    {
        item.special = {0, static_cast<std::uint8_t>(kind), 128};
    }
    return item;
}

// A made fighter of a career at a level: combat value 40, Armor Class 50,
// 20 Hit Points, its unarmed attack as creation gives it.
br::Creature made_fighter(Profession career, int level = 2)
{
    auto fighter = made_character(career, level);
    fighter.base_combat = 40;
    fighter.base_armor_class = 50;
    fighter.base_movement = 12;
    fighter.hit_points = fighter.hit_point_maximum = 20;
    fighter.unarmed_attacks_doubled = career == Profession::warrior ? 4 : 2;
    fighter.unarmed_dice_count = {1, 0};
    fighter.unarmed_dice_sides = {static_cast<std::uint8_t>(career == Profession::warrior ? 3 : 2),
                                  0};
    fighter.saves = {15, 14, 14, 13, 13, 16, 13, 13};
    return fighter;
}

void the_attack_roll_climbs()
{
    // fxengine.pas AttackRoll: d20 + combat value + the side's modifier must
    // reach the target; a 1 misses, a 20 counts as 100.
    check(og::ground_attack_hits(10, 40, 0, 50) && !og::ground_attack_hits(9, 40, 0, 50),
          "A combat value of 40 hits Armor Class 50 on a 10");
    check(!og::ground_attack_hits(1, 99, 0, 50), "A natural 1 always misses");
    check(og::ground_attack_hits(20, 0, 0, 99), "A natural 20 counts as 100");
    check(og::ground_attack_hits(8, 40, 2, 50) && !og::ground_attack_hits(11, 40, -2, 50),
          "The side's modifier adds to the roll");
}

void range_and_cover_raise_the_target()
{
    // cbtacts.pas ProjectileRangePenalty and ApplyRangePenalty.
    check(og::range_penalty(16, 32) == 0 && og::range_penalty(17, 32) == 2 &&
              og::range_penalty(24, 32) == 2 && og::range_penalty(25, 32) == 5 &&
              og::range_penalty(30, 1) == 0,
          "Past half the range 2, past three quarters 5; a melee weapon never");
    og::GroundNumbers defender;
    defender.armor_class = {55, 51};
    og::AttackSetting front;
    front.weapon_range = 32;
    front.ranged_weapon = true;
    front.line.distance = 20;
    check(og::target_number(defender, front) == 57, "A long-ish laser shot needs 2 more");
    front.line.blocked = true;
    check(og::target_number(defender, front) == 59, "Cover adds 2 more");
    front.guarding = true;
    check(og::target_number(defender, front) == 60,
          "A guard's shot is 5 harder, and neither cover nor range counts then");
    // cbtacts.pas RollAttacks: the rear Armor Class from behind, 2 lower for
    // a Rogue (the original's 2 higher, fixed as a bug; countdown.md, Bugs).
    og::AttackSetting behind;
    behind.from_behind = true;
    check(og::target_number(defender, behind) == 51, "From behind the rear Armor Class counts");
    behind.rogue = true;
    check(og::target_number(defender, behind) == 49, "A Rogue from behind needs 2 less");
    og::AttackSetting rogue_in_front;
    rogue_in_front.rogue = true;
    const int front_target = og::target_number(defender, rogue_in_front);
    const int behind_target = og::target_number(defender, behind);
    check(behind_target < front_target && !og::ground_attack_hits(9, 40, 0, front_target) &&
              og::ground_attack_hits(9, 40, 0, behind_target),
          "A Rogue hits from behind on a roll that misses from the front");
}

void rear_attacks_and_backstabs()
{
    // cbtacts.pas IsRearAttack: along the defender's facing or one either side,
    // once it has drawn two attacks; IsBackstab: straight along it.
    check(og::rear_attack(0, 0, 2, true) && og::rear_attack(0, 1, 2, true) &&
              og::rear_attack(7, 0, 2, true) && og::rear_attack(2, 1, 2, true),
          "The blow along the facing or one direction to a side is from behind");
    check(og::rear_attack(0, 7, 2, true),
          "Facing north, a blow heading north-west is from behind too (the original missed it)");
    check(!og::rear_attack(0, 0, 1, true) && !og::rear_attack(0, 0, 2, false) &&
              !og::rear_attack(0, 2, 2, true) && !og::rear_attack(0, 4, 2, true),
          "Not before two attacks, nor on a large defender, nor from its side or front");
    check(og::backstab_position(3, 3, 1) && !og::backstab_position(3, 2, 1) &&
              !og::backstab_position(3, 3, 32),
          "A backstab needs a melee weapon straight along the facing");
    // CalcWeaponDamage: a Rogue's (level + 7) / 4 times, anyone else's twice.
    auto rogue = made_fighter(Profession::rogue, 4);
    check(og::backstab_damage(5, rogue) == 10, "A level 4 Rogue backstabs for twice");
    rogue.level = 9;
    check(og::backstab_damage(5, rogue) == 20, "A level 9 Rogue backstabs for four times");
    check(og::backstab_damage(5, made_fighter(Profession::warrior, 9)) == 10,
          "Anyone else backstabs for twice");
    opengold::RandomState random{5};
    auto quiet = made_fighter(Profession::rogue);
    og::level_of(quiet.skills, Skill::move_silently) = 80;
    check(og::backstab_check(quiet, random), "Move Silently at 80, doubled for a Rogue, succeeds");
    check(!og::backstab_check(made_fighter(Profession::warrior), random),
          "Without Move Silently or Distract no backstab");
}

void the_gear_sets_the_numbers()
{
    // charhlp.pas RecalcStats, WeaponAdjust and AdjustEquippedItem.
    const auto types = ground_types();
    auto warrior = made_fighter(Profession::warrior);
    warrior.abilities[0] = 18;
    warrior.abilities[1] = 17;
    auto numbers = og::ground_numbers(warrior, {}, types, false);
    check(numbers.attack_bonus == 42 && numbers.damage[0] == og::DamageDice{1, 3, 2},
          "Unarmed, Strength 18 adds 2 to hit and 2 to a Warrior's 1d3 punch");
    check(numbers.armor_class[0] == 53 && numbers.armor_class[1] == 48,
          "Dexterity 17 raises the front Armor Class by 3; the rear is 2 below 50");
    const std::vector<br::Item> laser{ground_item(9, 10)};
    warrior.specializations[0] = 9;
    numbers = og::ground_numbers(warrior, laser, types, false);
    check(numbers.attack_bonus == 43 && numbers.damage[0] == og::DamageDice{1, 8, 1},
          "A laser pistol takes Dexterity's +2 and the specialization's +1 to hit and damage");
    warrior.specializations[0] = 15;
    numbers = og::ground_numbers(warrior, std::vector{ground_item(15, 10)}, types, false);
    check(numbers.attack_bonus == 43 && numbers.damage[0].bonus == 0,
          "A Sonic Stunner's specialization adds to hit only");
    const std::vector<br::Item> armored{ground_item(24), ground_item(26), ground_item(27),
                                        ground_item(28)};
    numbers = og::ground_numbers(warrior, armored, types, false);
    check(numbers.armor_class[0] == 65 && numbers.armor_class[1] == 60,
          "Battle armor with fields adds 12");
    check(numbers.armor_flags ==
              (og::armor_bit::body | og::armor_bit::breathing | og::armor_bit::fields |
               og::armor_bit::ecm | og::armor_bit::goggles),
          "Armor, ECM, goggles and a mask set their bits");
    og::set_command_status(warrior, og::BuckRogersStatus::dodging);
    numbers = og::ground_numbers(warrior, {}, types, false);
    check(numbers.armor_class[0] == 55 && numbers.armor_class[1] == 50,
          "Dodging adds 2 below level 4");
    warrior.level = 8;
    check(og::ground_numbers(warrior, {}, types, false).armor_class[0] == 57,
          "Dodging adds half the level from level 4");
    og::set_command_status(warrior, og::BuckRogersStatus::failed_zero_g);
    check(og::ground_numbers(warrior, {}, types, true).attack_bonus == 41,
          "A failed Zero-G roll takes 2 and the side's tactics add 1");
    auto loaded = warrior;
    loaded.abilities[0] = 10;
    std::vector<br::Item> load{ground_item(22, 0, false)};
    load[0].weight = 46;
    check(og::ground_numbers(loaded, load, types, false).movement == 6,
          "Past half the weight Strength allows, movement halves");
}

void what_stops_a_hit()
{
    // cbtacts.pas CalcWeaponDamage and effhndlr.pas's handlers at
    // WEAPON_DAMAGE_DEFENDER.
    using og::WeaponClass;
    opengold::RandomState random{11};
    const auto damage = [&](WeaponClass weapon, og::GroundLine line, std::uint8_t armor,
                            std::vector<og::GroundEffect> effects = {})
    {
        return og::damage_after_defenses(10, {weapon, line.distance, line, armor, 30}, effects,
                                         random);
    };
    og::GroundLine clear{4, false, false, false};
    og::GroundLine chaff = clear, mist = clear;
    chaff.chaff = true;
    mist.mist = true;
    check(damage(WeaponClass::rocket, chaff, 0) == 0 && damage(WeaponClass::rocket, clear, 0) == 10,
          "Chaff on the line stops a rocket");
    check(damage(WeaponClass::laser, mist, 0) == 0 && damage(WeaponClass::laser, chaff, 0) == 10,
          "Aerosol mist stops a laser, chaff does not");
    check(damage(WeaponClass::microwave, clear, og::armor_bit::body) == 0 &&
              damage(WeaponClass::microwave, clear, og::armor_bit::fields) == 10,
          "Body armor stops a microwave beam");
    int ecm = 0, fields = 0;
    for (int shot = 0; shot < 400; ++shot)
    {
        ecm += damage(WeaponClass::rocket, clear, og::armor_bit::ecm) == 0 ? 1 : 0;
        fields += damage(WeaponClass::rocket, clear, og::armor_bit::fields) == 0 ? 1 : 0;
    }
    check(ecm > 160 && ecm < 240 && fields > 260 && fields < 340,
          "ECM stops rockets half the time, fielded battle armor three times in four");
    check(damage(WeaponClass::laser, clear, 0, {{og::ground_effect::immune_to_laser, 0, 0}}) == 0 &&
              damage(WeaponClass::plasma, clear, 0,
                     {{og::ground_effect::immune_to_heat_and_plasma, 0, 0}}) == 0,
          "A creature's immunities stop their weapons");
    check(og::damage_after_defenses(50, {std::nullopt, 1, clear, og::armor_bit::body, 12},
                                    std::vector{og::GroundEffect{og::ground_effect::unkillable}},
                                    random) == 12,
          "An unkillable one is never taken below 0, even unarmed");
    // RollAttacks: a Sonic Stunner's hit stuns for 1d4 + 4 rounds.
    for (int roll = 0; roll < 20; ++roll)
    {
        const int rounds = og::stun_rounds(random);
        check(rounds >= 5 && rounds <= 8, "A stun lasts 5 to 8 rounds");
    }
    std::vector<og::GroundEffect> effects{{og::ground_effect::block_stun, 0, 0}};
    check(!og::give_ground_effect(effects, {og::ground_effect::stunned, 6, 0}, false, false, true),
          "A creature proof against stun weapons is unaffected");
    effects.clear();
    check(!og::give_ground_effect(effects, {og::ground_effect::stunned, 6, 0}, true, false, true) &&
              og::give_ground_effect(effects, {og::ground_effect::stunned, 6, 0}, false, false,
                                     true) &&
              og::give_ground_effect(effects, {og::ground_effect::stunned, 8, 0}, false, false,
                                     true) &&
              effects.size() == 1 && effects[0].minutes == 8,
          "A save cancels the stun; a second one lengthens the first");
}

void area_weapons_blast()
{
    // gdconsts.pas projectileKinds; cbtacts.pas CombatLaunchProjectile.
    check(og::projectile(5).effect == og::ground_effect::blind && og::projectile(6).radius == 1 &&
              og::projectile(10).radius == 3 && og::projectile(11).radius == 2 &&
              og::projectile(12).effect == og::ground_effect::stunned &&
              og::projectile(7).lingers && og::projectile(8).lingers && og::projectile(9).lingers &&
              !og::projectile(6).lingers,
          "The projectiles' blasts, effects and clouds are the game's");
    check(og::projectile(6).saved == og::SaveEffect::halves && og::projectile(6).save == 2 &&
              og::projectile(7).save == 1 && og::projectile(5).save == 3,
          "Blasts save 2 for half, gas 1 and the dazzle 3 to cancel");
    const auto types = ground_types();
    opengold::RandomState random{3};
    for (int launch = 0; launch < 30; ++launch)
    {
        const auto explosive = og::roll_blast(6, types[33], 0, random);
        check(explosive.damage >= 4 && explosive.damage <= 40, "An explosive grenade does 4d10");
        const auto stun = og::roll_blast(12, types[35], 0, random);
        check(stun.damage == 0 && stun.minutes >= 3 && stun.minutes <= 6,
              "A stun grenade stuns for its 1d4 + 2 and does no damage");
        const auto gas = og::roll_blast(7, types[34], 0, random);
        check(gas.damage == 0 && gas.minutes >= 2 && gas.minutes <= 7,
              "Gas leaves its victims helpless 2 to 7 rounds (the original's 0d0 for good)");
        const auto dazzle = og::roll_blast(5, types[36], 0, random);
        check(dazzle.damage >= 1 && dazzle.damage <= 3 && dazzle.minutes >= 2 &&
                  dazzle.minutes <= 7,
              "A dazzle grenade blinds for 1d6 + 1 and does 1d3");
    }
    check(og::projectile_strays(40, 10, 2, 3) && !og::projectile_strays(40, 13, 2, 3) &&
              og::projectile_strays(40, 15, 3, 3),
          "A throw strays at 50 or less, the range's penalty taken off");
    for (int roll = 0; roll < 30; ++roll)
    {
        const int out = og::scatter_offset(3, random), back = og::scatter_offset(-3, random);
        const int still = og::scatter_offset(0, random);
        check(out >= 3 && out <= 5 && back <= -3 && back >= -5 && (still == 1 || still == -1),
              "A stray shot lands up to 2 farther out, or a step aside on its own line");
    }
    check(og::spared_by_gear(5, og::armor_bit::goggles) &&
              og::spared_by_gear(7, og::armor_bit::breathing) &&
              !og::spared_by_gear(6, og::armor_bit::breathing),
          "Goggles spare a dazzle, a mask or body armor gas");
    check(og::blast_damage(21, og::SaveEffect::halves, true) == 10 &&
              og::blast_damage(21, og::SaveEffect::halves, false) == 21 &&
              og::blast_damage(3, og::SaveEffect::cancels, true) == 0,
          "A made save halves a blast's damage or cancels a dazzle's");
    check(og::within_blast(3, 1) && !og::within_blast(4, 1) && og::within_blast(5, 2) &&
              !og::within_blast(6, 2) && og::within_blast(7, 3),
          "A blast of radius r reaches 2r + 1 doubled steps");
    auto items = std::vector{ground_item(16), ground_item(33, 2, true, 6)};
    check(og::readied_weapon_range(items, types) == 12, "A grenade launcher sends a grenade 12");
    check(og::put_away_launcher(items, types, 1) && !og::readied(items[0]) &&
              og::readied_weapon_range(items, types) == 3,
          "Launching puts the launcher away; a grenade is then thrown 3");
    auto launcher = std::vector{ground_item(18, 3, true, 11)};
    check(!og::put_away_launcher(launcher, types, 0) && !og::readied(launcher[0]) &&
              og::readying_ends_turn(types[18]),
          "A rocket launcher must be readied again, which ends a turn");
}

void clouds_linger()
{
    // types.pas TAreaEffect; cbtacts.pas CombatLaunchProjectile;
    // fxengine.pas CountDownAreaEffects and ConsiderTerrainEffects.
    check(og::cloud_of(7) == og::CloudMatter::gas && og::cloud_of(8) == og::CloudMatter::chaff &&
              og::cloud_of(9) == og::CloudMatter::mist && !og::cloud_of(6),
          "Gas, chaff and mist grenades leave clouds");
    auto cloud = og::lay_ground_cloud({5, 5}, og::CloudMatter::chaff, false,
                                      [](opengold::rules::Cell cell)
                                      {
                                          return cell.x != 6;
                                      });
    check(cloud.squares().size() == 6 && cloud.covers_square({5, 5}) &&
              !cloud.covers_square({6, 5}) && cloud.covers_square({4, 6}),
          "A cloud covers the walkable squares of its block");
    int rounds = 1;
    while (og::cloud_lingers(cloud))
    {
        ++rounds;
    }
    check(rounds == 4, "A cloud lasts four rounds");
    auto weightless = og::lay_ground_cloud({5, 5}, og::CloudMatter::mist, true,
                                           [](opengold::rules::Cell)
                                           {
                                               return true;
                                           });
    check(!og::cloud_lingers(weightless), "In zero gravity a cloud lasts one");
    opengold::RandomState random{19};
    auto breather = made_fighter(Profession::rogue);
    breather.saves[1] = 21;
    int helpless = 0;
    for (int breath = 0; breath < 20; ++breath)
    {
        if (const auto gassed = og::gassed(breather, 0, false, false, random))
        {
            check(*gassed >= 2 && *gassed <= 7, "Gas leaves one helpless 1d6 + 1 rounds");
            ++helpless;
        }
    }
    check(helpless >= 15, "Gas leaves one who fails its save helpless");
    check(!og::gassed(breather, og::armor_bit::breathing, false, false, random) &&
              !og::gassed(breather, 0, true, false, random),
          "A mask spares it, and the helpless are not gassed again");
}

void ammunition_runs_out()
{
    // cbtacts.pas RecalcAttacksLeft, BeginRound and MakeAttack.
    const auto types = ground_types();
    auto fighter = made_fighter(Profession::rocket_jock);
    fighter.attacks_doubled = 3;
    auto items = std::vector{ground_item(9, 10)};
    check(og::ground_attacks(fighter, items, types, 0, false) == std::array{3, 1} &&
              og::ground_attacks(fighter, items, types, 1, false) == std::array{3, 2},
          "A laser pistol shoots 3 a round; a rate of 3 in two rounds gives 1, then 2 (the first "
          "round is 0)");
    items[0].quantity = 1;
    check(og::ground_attacks(fighter, items, types, 1, false)[0] == 1,
          "No more attacks than rounds");
    check(og::ground_attacks(fighter, {}, types, 1, true)[0] == 1 &&
              og::ground_attacks(made_fighter(Profession::warrior), {}, types, 0, false)[0] == 2,
          "Dizziness halves the rate; a Warrior punches twice");
    check(!og::spend_rounds(items[0], types[9], 1) && items[0].quantity == 0,
          "Each shot spends a round");
    auto grenades = ground_item(33, 1, true, 6);
    check(og::spend_rounds(grenades, types[33], 1), "The last grenade of a bundle goes");
}

void injuries_follow_the_damage()
{
    // charhlp.pas InflictDamage and ProcessDyingCharacters.
    using og::BuckRogersStatus;
    check(og::injury_from(10, 4).status == BuckRogersStatus::okay &&
              og::injury_from(10, 10).status == BuckRogersStatus::unconscious &&
              og::injury_from(10, 11).status == BuckRogersStatus::dying &&
              og::injury_from(10, 19).status == BuckRogersStatus::dying &&
              og::injury_from(10, 20).status == BuckRogersStatus::comatose &&
              og::injury_from(10, 39).status == BuckRogersStatus::comatose &&
              og::injury_from(10, 40).status == BuckRogersStatus::dead,
          "0 is unconscious, below 0 bleeding, past 9 below comatose, past 29 dead");
    auto victim = made_fighter(Profession::medic);
    victim.hit_points = 6;
    int count = 0;
    og::inflict_ground_damage(victim, 9, count);
    check(og::status_of(victim) == BuckRogersStatus::dying && count == 3 && victim.enabled == 0 &&
              victim.hit_points == 0,
          "Three below 0, it bleeds from 3");
    int rounds = 0;
    while (!og::bleed(victim, count))
    {
        ++rounds;
    }
    check(rounds == 6 && og::status_of(victim) == BuckRogersStatus::comatose,
          "Past 9 it slips into a coma, not death");
    auto poisoned = made_fighter(Profession::medic);
    og::poison_kills(poisoned);
    check(og::status_of(poisoned) == BuckRogersStatus::poisoned && poisoned.hit_points == 0,
          "A lethal poison leaves it poisoned at 0");
    // fxengine.pas RollSavingThrow.
    check(!og::ground_save_made(1, 30, 2) && og::ground_save_made(20, -30, 19) &&
              og::ground_save_made(13, 0, 13) && !og::ground_save_made(16, -4, 13),
          "1 fails, 20 saves, otherwise the roll must reach the save; blindness takes 4");
}

void the_new_commands_are_offered()
{
    // combat.pas CombatMenu and CombatOptionAllowed.
    const auto types = ground_types();
    auto fighter = made_fighter(Profession::warrior);
    const og::MenuSituation open{true, false, false, false};
    auto menu = og::ground_menu(fighter, {}, types, open);
    check(menu.move && menu.sprint && menu.dodge && menu.guard && !menu.intimidate && !menu.aid &&
              !menu.jetpack,
          "With movement left: Move, Sprint, Dodge and Guard");
    og::level_of(fighter.skills, Skill::intimidate) = 20;
    check(og::ground_menu(fighter, {}, types, {true, true, false, false}).intimidate &&
              !og::ground_menu(fighter, {}, types, open).intimidate,
          "Intimidate needs the skill and an enemy next to it");
    const std::vector<br::Item> pack{ground_item(30)};
    check(og::ground_menu(fighter, pack, types, {true, false, false, true}).jetpack &&
              !og::ground_menu(fighter, pack, types, open).jetpack,
          "Jetpack needs a readied jetpack, out of doors");
    og::set_command_status(fighter, og::BuckRogersStatus::sprinting);
    menu = og::ground_menu(fighter, pack, types, {true, false, true, true});
    check(!menu.sprint && !menu.dodge && !menu.guard && menu.aid && menu.jetpack &&
              !og::may_strike(fighter, {}, types),
          "A sprinter may not sprint again, dodge, guard or strike");
    og::set_command_status(fighter, og::BuckRogersStatus::failed_zero_g);
    menu = og::ground_menu(fighter, {}, types, {true, true, false, false});
    check(!menu.sprint && menu.dodge && !menu.intimidate,
          "After a failed Zero-G roll no Sprint and no Intimidate");
    og::set_status(fighter, og::BuckRogersStatus::okay);
    check(fighter.enabled == 1 && !og::may_strike(fighter, std::vector{ground_item(7, 0)}, types) &&
              og::may_strike(fighter, std::vector{ground_item(9, 0)}, types),
          "An empty slug weapon may not strike");
    check(!og::may_guard_with(std::vector{ground_item(17, 2, true, 10)}, types) &&
              og::may_guard_with(std::vector{ground_item(9, 2)}, types),
          "An area weapon does not guard");
    // cbtacts.pas RollTactics, UseLeadership, IntimidateTarget, RollZeroG;
    // viewchar.pas FindPatients.
    check(og::rolls_tactics(fighter, false) && !og::rolls_tactics(fighter, true),
          "The stunned make no tactics roll");
    auto leader = made_fighter(Profession::rogue);
    og::level_of(leader.skills, Skill::leadership) = 30;
    auto companion = made_fighter(Profession::warrior);
    companion.companion = 1;
    check(og::may_lead(leader) && !og::may_lead(companion) && og::may_follow(companion, false) &&
              !og::may_follow(companion, true),
          "A member with Leadership may lead a companion, never an unkillable one");
    companion.abilities[3] = 3;
    check(!og::may_follow(companion, false), "Not one of Intelligence 3");
    check(og::scared_effect() == og::GroundEffect{og::ground_effect::scared, 10, 12},
          "Intimidation scares for 10 minutes");
    check(og::sprint_movement(24) == 48 && og::zero_g_movement(24) == 6 &&
              og::rolls_zero_g(true, true, 2) && !og::rolls_zero_g(true, true, 1) &&
              !og::rolls_zero_g(false, true, 9) && og::dodge_armor_bonus(3) == 2 &&
              og::dodge_armor_bonus(9) == 4,
          "Sprint doubles, Zero-G quarters; one that waited rolls no Zero-G");
    opengold::RandomState random{23};
    std::set<int> rows, columns;
    for (int step = 0; step < 400; ++step)
    {
        const auto at = og::lurch_step({10, 10}, random);
        check(at != opengold::rules::Cell{10, 10}, "A lurch always moves");
        rows.insert(at.y - 10);
        columns.insert(at.x - 10);
    }
    check(rows == std::set{-2, -1, 0, 1, 2} && columns == std::set{-2, -1, 0, 1, 2},
          "A lurch goes two squares either way (the original's rows ran -1 to 3)");
}

void every_difficulty_level()
{
    // fxengine.pas AttackRoll and eclvm.pas EclLoadMonster;
    // docs/BUCK-ROGERS.md, Difficulty.
    using og::BuckRogersDifficulty;
    using opengold::DifficultySetting;
    const std::array<int, 6> to_hit{2, 1, 0, -1, -2, -3};
    const std::array<unsigned, 6> percent{50, 75, 100, 125, 150, 200};
    for (std::size_t level = 0; level < 6; ++level)
    {
        const auto at = static_cast<BuckRogersDifficulty>(level);
        check(og::party_to_hit(at) == to_hit[level] &&
                  og::monster_hit_points_percent(at) == percent[level],
              std::string(og::buck_rogers_difficulty_names[level]) +
                  ": the party's to-hit and the monsters' Hit Points");
    }
    check(og::loaded_hit_points(1, BuckRogersDifficulty::novice) == 1 &&
              og::loaded_hit_points(15, BuckRogersDifficulty::cadet) == 11 &&
              og::loaded_hit_points(15, BuckRogersDifficulty::ace) == 22,
          "LOAD MONSTER gives (level + 2) quarters, at least 1");
    check(opengold::parse_difficulty_setting("novice") == DifficultySetting::novice &&
              opengold::parse_difficulty_setting("infernal") == DifficultySetting::infernal &&
              opengold::parse_difficulty("novice") == opengold::Difficulty::easy &&
              opengold::played_difficulty(DifficultySetting::novice) ==
                  opengold::Difficulty::easy &&
              opengold::difficulty_setting_name(DifficultySetting::novice) == "novice" &&
              opengold::difficulty_setting_name(DifficultySetting::hard) == "hard",
          "Novice is a value of the setting, which any other game plays as Easy");
    check(opengold::offers_novice(countdown) &&
              opengold::offers_novice(GoldBoxGame::matrix_cubed) &&
              !opengold::offers_novice(GoldBoxGame::pool_of_radiance),
          "Only the Buck Rogers games offer Novice");
    const std::array<std::pair<int, int>, 6> table{
        {{-2, -2}, {-2, -2}, {0, 0}, {1, 1}, {2, 2}, {3, 3}}};
    for (std::size_t level = 0; level < 6; ++level)
    {
        opengold::CampaignEncounter encounter;
        encounter.enemies.emplace_back();
        opengold::apply_original_difficulty(encounter, static_cast<DifficultySetting>(level),
                                            countdown);
        const auto &strength = encounter.enemies.front().original_strength;
        check(strength.hit_points_percent == percent[level] &&
                  strength.to_hit == table[level].first &&
                  strength.armor_class_bonus == table[level].second &&
                  encounter.party_to_hit == to_hit[level],
              std::string(og::buck_rogers_difficulty_names[level]) +
                  " plays the monsters' Hit Points, the table's to-hit and AC, and the party's "
                  "to-hit");
    }
    opengold::CampaignEncounter pool;
    pool.enemies.emplace_back();
    opengold::apply_original_difficulty(pool, DifficultySetting::novice,
                                        GoldBoxGame::pool_of_radiance);
    check(pool.enemies.front().original_strength.hit_points_percent == 75 && pool.party_to_hit == 0,
          "Pool of Radiance plays Novice as Easy");
}

// A Buck Rogers fight through the rules module, as the game will run one.
struct GroundFight
{
    std::unique_ptr<opengold::rules::RulesModule> module;
    std::unique_ptr<opengold::rules::CombatSession> session;
};

opengold::rules::Participant ground_participant(opengold::rules::EntityId id,
                                                opengold::rules::Side side,
                                                opengold::rules::Cell cell,
                                                const br::Creature &creature,
                                                std::vector<br::Item> items)
{
    opengold::rules::Participant p;
    p.id = id;
    p.name = id == 1 ? "BUCK" : "RAM TROOPER " + std::to_string(id);
    p.definition = p.name;
    p.side = side;
    p.cell = cell;
    p.character_profile = og::encode_ground_record({creature, std::move(items), ground_types()});
    return p;
}

GroundFight ground_fight(std::vector<opengold::rules::Participant> participants, std::uint64_t seed,
                         bool outdoors = false, bool zero_gravity = false,
                         GoldBoxGame game = countdown,
                         std::vector<opengold::rules::Terrain> ground = {})
{
    opengold::rules::Encounter encounter;
    encounter.battlefield.width = 12;
    encounter.battlefield.height = 8;
    encounter.battlefield.terrain =
        ground.empty() ? std::vector(96, opengold::rules::Terrain::open) : std::move(ground);
    encounter.participants = std::move(participants);
    encounter.outdoors = outdoors;
    encounter.zero_gravity = zero_gravity;
    og::Content content;
    content.game = game;
    GroundFight fight{og::rules_module(std::move(content)), nullptr};
    fight.session = fight.module->create(std::move(encounter), seed);
    return fight;
}

const opengold::rules::CombatantView &view_of(const opengold::rules::Snapshot &snapshot,
                                              opengold::rules::EntityId id)
{
    return *std::ranges::find(snapshot.combatants, id, &opengold::rules::CombatantView::id);
}

std::optional<opengold::rules::Command> offered(const opengold::rules::CombatSession &session,
                                                std::string_view verb,
                                                opengold::rules::EntityId target = 0)
{
    for (const auto &command : session.legal_commands())
    {
        if (command.verb == verb && (target == 0 || command.target == target))
        {
            return command;
        }
    }
    return std::nullopt;
}

// Ends every turn that is not the member's, until it is, or the fight ends.
bool to_turn_of(opengold::rules::CombatSession &session, opengold::rules::EntityId id)
{
    for (int turns = 0; turns < 40; ++turns)
    {
        const auto snapshot = session.snapshot();
        if (snapshot.outcome != opengold::rules::Outcome::ongoing)
        {
            return false;
        }
        if (snapshot.actor == id)
        {
            return true;
        }
        if (!session.submit(*offered(session, "end")))
        {
            return false;
        }
    }
    return false;
}

void a_fight_runs_by_the_buck_rogers_rules()
{
    using opengold::rules::Cell;
    using opengold::rules::Side;
    auto buck = made_fighter(Profession::warrior);
    og::level_of(buck.skills, Skill::intimidate) = 50;
    auto trooper = made_fighter(Profession::warrior);
    trooper.hit_points = trooper.hit_point_maximum = 8;
    auto fight = ground_fight({ground_participant(1, Side::party, {2, 2}, buck, {ground_item(2)}),
                               ground_participant(2, Side::opposition, {3, 2}, trooper, {})},
                              41);
    auto &session = *fight.session;
    check(to_turn_of(session, 1), "Buck gets a turn");
    std::set<std::string> verbs;
    for (const auto &command : session.legal_commands())
    {
        verbs.insert(command.verb);
    }
    check(verbs.contains("melee") && verbs.contains("intimidate") && verbs.contains("sprint") &&
              verbs.contains("dodge") && verbs.contains("guard") && verbs.contains("delay") &&
              verbs.contains("end") && !verbs.contains("cast") && !verbs.contains("turn") &&
              !verbs.contains("bandage") && !verbs.contains("jetpack"),
          "The combat menu is Buck Rogers': no spells, turning or AD&D bandaging");
    check(view_of(session.snapshot(), 1).armor_class == 50,
          "The fight shows the ascending Armor Class");
    for (int attack = 0;
         attack < 40 && session.snapshot().outcome == opengold::rules::Outcome::ongoing; ++attack)
    {
        if (!to_turn_of(session, 1))
        {
            break;
        }
        // Once the trooper is down, Buck ends the turn with his attacks left.
        const auto blow = offered(session, "melee", 2);
        session.submit(blow ? *blow : *offered(session, "end"));
    }
    const auto after = session.snapshot();
    check(after.outcome == opengold::rules::Outcome::victory, "Buck beats the trooper");
    const auto left = og::decode_ground_record(view_of(after, 2).persistent.resources);
    check(left && og::status_of(left->creature) != og::BuckRogersStatus::okay &&
              left->creature.hit_points == 0,
          "The fight leaves the trooper's record down, with its status as the original sets it");
    const auto restored = fight.module->restore(session.save());
    check(restored->save() == session.save(), "A Buck Rogers fight saves and restores");
}

void area_weapons_and_clouds_in_a_fight()
{
    using opengold::rules::Side;
    auto gunner = made_fighter(Profession::rocket_jock);
    gunner.base_combat = 60;
    auto trooper = made_fighter(Profession::warrior);
    trooper.hit_points = trooper.hit_point_maximum = 60;
    auto fight = ground_fight(
        {ground_participant(1, Side::party, {1, 4}, gunner, {ground_item(17, 2, true, 10)}),
         ground_participant(2, Side::opposition, {8, 4}, trooper, {})},
        77);
    auto &session = *fight.session;
    check(to_turn_of(session, 1), "The gunner gets a turn");
    check(!offered(session, "ranged", 2) && offered(session, "launch"),
          "A plasma thrower aims at a square, not a creature");
    auto launch = *offered(session, "launch");
    launch.destination = {8, 4};
    bool launched = false;
    for (const auto &command : session.legal_commands())
    {
        if (command.verb == "launch" && command.destination == launch.destination)
        {
            launched = session.submit(command);
            break;
        }
    }
    check(launched, "It launches at the trooper's square");
    const auto after = session.snapshot();
    const auto thrower = og::decode_ground_record(view_of(after, 1).persistent.resources);
    check(thrower && thrower->items.size() == 1 && thrower->items[0].quantity == 1 &&
              !og::readied(thrower->items[0]),
          "It spent a round and must be readied again");
    const auto struck = og::decode_ground_record(view_of(after, 2).persistent.resources);
    check(struck && struck->creature.hit_points < 60, "The blast burns the trooper");
    // viewchar.pas ToggleReadyItem: readying the heavy weapon again ends a turn.
    check(to_turn_of(session, 1) && offered(session, "ready") && !offered(session, "launch"),
          "Next turn the plasma thrower must be readied before it fires again");
    session.submit(*offered(session, "ready"));
    const auto reloaded =
        og::decode_ground_record(view_of(session.snapshot(), 1).persistent.resources);
    check(reloaded && og::readied(reloaded->items[0]) && session.snapshot().actor != 1,
          "Readying it ends the gunner's turn");
    // A chaff grenade's cloud stays in the checkpoint as the fight goes on.
    auto thrower_jock = made_fighter(Profession::rocket_jock);
    thrower_jock.base_combat = 60;
    auto chaffed = ground_fight(
        {ground_participant(1, Side::party, {1, 4}, thrower_jock, {ground_item(37, 2, true, 8)}),
         ground_participant(2, Side::opposition, {8, 4}, trooper, {})},
        78);
    check(to_turn_of(*chaffed.session, 1), "The thrower gets a turn");
    const auto before = chaffed.session->save();
    chaffed.session->submit(*offered(*chaffed.session, "launch"));
    const auto restored = chaffed.module->restore(chaffed.session->save());
    check(restored->save() == chaffed.session->save() && restored->save() != before,
          "The cloud saves and restores with the fight");
}

void medics_and_jetpacks_in_a_fight()
{
    using opengold::rules::Side;
    auto medic = made_fighter(Profession::medic);
    og::level_of(medic.skills, Skill::first_aid) = 20;
    og::level_of(medic.skills, Skill::use_jetpack) = 100;
    auto wounded = made_fighter(Profession::warrior);
    og::set_status(wounded, og::BuckRogersStatus::dying);
    wounded.enabled = 0;
    wounded.hit_points = 0;
    auto trooper = made_fighter(Profession::warrior);
    auto fight = ground_fight({ground_participant(1, Side::party, {2, 2}, medic, {ground_item(30)}),
                               ground_participant(3, Side::party, {2, 3}, wounded, {}),
                               ground_participant(2, Side::opposition, {10, 6}, trooper, {})},
                              5, true);
    auto &session = *fight.session;
    check(to_turn_of(session, 1), "The medic gets a turn");
    check(offered(session, "aid") && offered(session, "jetpack"),
          "Aid for the bleeding teammate next to it, and a jetpack out of doors");
    session.submit(*offered(session, "aid"));
    const auto bandaged =
        og::decode_ground_record(view_of(session.snapshot(), 3).persistent.resources);
    check(bandaged && og::status_of(bandaged->creature) == og::BuckRogersStatus::unconscious,
          "First Aid bandages it without a roll: unconscious, no longer bleeding");
    check(to_turn_of(session, 1), "The medic's next turn");
    session.submit(*offered(session, "jetpack"));
    std::set<std::string> verbs;
    for (const auto &command : session.legal_commands())
    {
        verbs.insert(command.verb);
    }
    check(verbs == std::set<std::string>{"land"}, "In flight it can only land");
    auto land = *offered(session, "land");
    for (const auto &command : session.legal_commands())
    {
        if (command.destination == opengold::rules::Cell{9, 6})
        {
            land = command;
        }
    }
    session.submit(land);
    check(view_of(session.snapshot(), 1).cell == opengold::rules::Cell{9, 6},
          "It lands where it chose");
}

void zero_gravity_and_difficulty_in_a_fight()
{
    using opengold::rules::Side;
    auto floater = made_fighter(Profession::engineer);
    auto trooper = made_fighter(Profession::warrior);
    trooper.hit_points = trooper.hit_point_maximum = 30;
    auto monster = ground_participant(2, Side::opposition, {9, 5}, trooper, {});
    monster.original_strength.hit_points_percent = 50;
    auto fight = ground_fight({ground_participant(1, Side::party, {1, 1}, floater, {}), monster}, 9,
                              false, true);
    auto &session = *fight.session;
    check(view_of(session.snapshot(), 2).hit_points == 15 &&
              view_of(session.snapshot(), 2).max_hit_points == 15,
          "Novice's LOAD MONSTER halves a monster's Hit Points, its maximum with them");
    check(to_turn_of(session, 1), "The engineer gets a turn");
    const auto record =
        og::decode_ground_record(view_of(session.snapshot(), 1).persistent.resources);
    check(record && og::status_of(record->creature) == og::BuckRogersStatus::failed_zero_g,
          "Without Maneuver in Zero-G it fails its roll in zero gravity");
    const auto &log = session.snapshot().log_entries;
    check(std::ranges::any_of(log,
                              [](const opengold::rules::LogEntry &entry)
                              {
                                  return entry.message.source == "{name} fails the Zero-G roll.";
                              }),
          "The log says so");
}

// gdconsts.pas combatTerrain and pathing.pas WalkLine: ground no one can
// cross (cost 255) stops a shot only when its line class is 1; class 2 is
// cover to the shot.
void impassable_ground_lets_shots_across()
{
    using opengold::por::combat_ground;
    using opengold::rules::Cell;
    using opengold::rules::Side;
    using opengold::rules::Terrain;
    check(combat_ground(8) == Terrain::obstacle && combat_ground(92) == Terrain::impassable &&
              combat_ground(79) == Terrain::impassable_cover &&
              combat_ground(46) == Terrain::difficult && combat_ground(1) == Terrain::open,
          "The terrain kinds become the rules' ground by cost and line class");
    const auto gunner = made_fighter(Profession::rocket_jock);
    const auto trooper = made_fighter(Profession::warrior);
    const Cell between{3, 4};
    const auto fight_across = [&](Terrain ground)
    {
        std::vector<Terrain> field(96, Terrain::open);
        field[static_cast<std::size_t>(between.y * 12 + between.x)] = ground;
        return ground_fight(
            {ground_participant(1, Side::party, {1, 4}, gunner, {ground_item(9, 10)}),
             ground_participant(2, Side::opposition, {6, 4}, trooper, {})},
            12, false, false, countdown, std::move(field));
    };
    const auto steps_onto = [&](const opengold::rules::CombatSession &session)
    {
        return std::ranges::any_of(session.legal_commands(),
                                   [&](const opengold::rules::Command &command)
                                   {
                                       return command.verb == "move" &&
                                              command.destination == between;
                                   });
    };
    auto open = fight_across(Terrain::open);
    check(to_turn_of(*open.session, 1) && offered(*open.session, "ranged", 2) &&
              steps_onto(*open.session),
          "On open ground the gunner may shoot the trooper or step between");
    for (const auto ground : {Terrain::impassable, Terrain::impassable_cover})
    {
        auto fight = fight_across(ground);
        check(to_turn_of(*fight.session, 1), "The gunner gets a turn");
        check(offered(*fight.session, "ranged", 2).has_value(),
              "A shot crosses ground no one can walk on");
        check(!steps_onto(*fight.session), "No one steps onto that ground");
    }
    auto walled = fight_across(Terrain::obstacle);
    check(to_turn_of(*walled.session, 1) && !offered(*walled.session, "ranged", 2) &&
              !steps_onto(*walled.session),
          "A wall (line class 1) stops both the shot and the step");
}

std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "SHIPS.DAX"))
    {
        std::cout << opengold::game_title(game) << "'s files are not here; they are not read.\n";
        return std::nullopt;
    }
    return folder;
}

Bytes file_bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

Bytes record_of(const std::filesystem::path &path, unsigned id)
{
    const auto archive = opengold::decode_dax_archive(file_bytes(path));
    check(static_cast<bool>(archive), path.filename().string() + " is an archive");
    const auto found = std::find_if(archive.records.begin(), archive.records.end(),
                                    [id](const opengold::DaxRecord &record)
                                    {
                                        return record.id == id;
                                    });
    check(found != archive.records.end(), path.filename().string() + " holds the record");
    return found->bytes;
}

void the_census_is_complete(const std::filesystem::path &folder)
{
    check(opengold::identify_game(folder) == countdown, "The folder is Countdown to Doomsday's");
    const auto census = opengold::take_census(folder, countdown);
    check(census.complete(), "Every file of Countdown to Doomsday decodes");
    check(std::find(census.program_files.begin(), census.program_files.end(), "SAVGAMA.DAT") !=
              census.program_files.end(),
          "SAVGAMA.DAT is a saved game, which the census leaves alone");
}

void the_records_decode(const std::filesystem::path &folder)
{
    const auto ape = opengold::buck_rogers::decode_creature(record_of(folder / "MON0CHA.DAX", 4));
    check(ape && ape->name == "DESERT APE" && ape->level == 8 && ape->hit_point_maximum == 48 &&
              ape->base_armor_class == 55 && ape->experience_award == 625,
          "Creature 4 is the desert ape: level 8, 48 Hit Points, AC 55, 625 experience");
    const auto warrior =
        opengold::buck_rogers::decode_creature(record_of(folder / "MON0CHA.DAX", 0));
    check(warrior && warrior->name == "D.R. WARRIOR" && warrior->race == 6 && warrior->career == 3,
          "Creature 0 is a Desert Runner warrior");

    const auto templates =
        opengold::buck_rogers::decode_item_templates(record_of(folder / "ITEM0.DAX", 1));
    check(templates && templates->size() == 91, "ITEM0 holds 91 item templates");
    const auto image = opengold::por::unpack_sound_executable(file_bytes(folder / "START.EXE"));
    const auto words = opengold::por::decode_item_words(image, countdown);
    check(words && words->size() == 45 && words->at(1) == "Knife",
          "START.EXE names items with 44 words, Knife first");
    const auto &bolt_gun = templates->at(6);
    check(bolt_gun.name_parts[1] == 7 && words->at(bolt_gun.name_parts[1]) == "Gun" &&
              words->at(bolt_gun.name_parts[2]) == "Bolt",
          "Template 7, the free weapon, is a Bolt Gun");

    const auto types = opengold::buck_rogers::decode_item_types(file_bytes(folder / "ITEMS"));
    check(types && types->size() == 39, "ITEMS holds 39 of the 129 rows the game asks for");

    const auto party_ship = opengold::buck_rogers::decode_ship(record_of(folder / "SHIPS.DAX", 50));
    check(party_ship && party_ship->name == "Maelstrom Rider" && party_ship->tonnage == 150 &&
              party_ship->system_status[4] == 450 && party_ship->system_maximum[0] == 600,
          "Ship 50 is the party's Maelstrom Rider: 150 tons, 450 fuel, a hull of 600");

    const auto sounds = opengold::por::load_sound_effects(folder, countdown);
    check(sounds.size() == 21 && sounds[9].name == "Footstep" && sounds[9].audible() &&
              sounds[14].unused,
          "START.EXE's driver plays 21 songs; command 10 is the footstep");
}

void the_scripts_are_listed(const std::filesystem::path &folder)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, countdown);
    check(listing.stalls.empty(), "No script reaches 3Bh or 3Ch");
    check(listing.uneven_skips.empty(), "No IF skips a command the original skips unevenly");
    check(listing.skill_checks.size() == 49, "The scripts check skills 49 times");
    const auto demo_charge =
        std::find_if(listing.skill_checks.begin(), listing.skill_checks.end(),
                     [](const opengold::SkillCheckUse &use)
                     {
                         return use.place.script == 98 && use.place.address == 0x95FA;
                     });
    check(demo_charge != listing.skill_checks.end() && demo_charge->skill.values == std::set{11U} &&
              demo_charge->difficulty.values == std::set{3U} && demo_charge->difficulty.complete,
          "The escape pod's door asks for skill 11 at difficulty 3 (half the skill)");
    check(listing.ship_combats.size() == 3 && listing.shops.size() == 15 &&
              listing.solar_maps.size() == 4,
          "Three ship combats, fifteen shops and four ways onto the solar map");
    const auto spends = std::count_if(listing.fuel_writes.begin(), listing.fuel_writes.end(),
                                      [](const opengold::ScriptCommandUse &use)
                                      {
                                          return use.command == "SUBTRACT";
                                      });
    check(spends == 1 && listing.fuel_writes.size() == 5,
          "One command spends fuel; four others set or add to it");
    // The win: after the escape from the Mariposa, script 98 sends the team
    // home to Salvation, script 17 on disk 1.
    const auto home =
        std::find_if(listing.script_loads.begin(), listing.script_loads.end(),
                     [](const opengold::ScriptLoad &load)
                     {
                         return load.place.script == 98 && load.place.address == 0x9F42;
                     });
    check(home != listing.script_loads.end() && home->script.values == std::set{17U} &&
              home->data_set.values == std::set{1U} && home->data_set.complete,
          "The escape from the Mariposa ends with script 17 on disk 1");
}

void the_records_save_as_they_were(const std::filesystem::path &folder)
{
    const auto characters = opengold::decode_dax_archive(file_bytes(folder / "MON0CHA.DAX"));
    std::size_t creatures = 0;
    for (const auto &record : characters.records)
    {
        const auto creature = br::decode_creature(record.bytes);
        check(creature.has_value(), "Every MON0CHA entry is a creature");
        check(br::encode_creature(*creature) ==
                  Bytes(record.bytes.begin(), record.bytes.begin() + br::Creature::size),
              "Each creature saves byte for byte as the game holds it");
        ++creatures;
    }
    check(creatures == 54, "All 54 creatures round trip");
    const auto item_lists = opengold::decode_dax_archive(file_bytes(folder / "MON0ITM.DAX"));
    for (const auto &record : item_lists.records)
    {
        const auto items = br::decode_items(record.bytes);
        check(items && br::encode_items(*items) == record.bytes,
              "Each item list saves byte for byte");
    }
}

void a_new_character_uses_the_games_items(const std::filesystem::path &folder)
{
    const auto templates = br::decode_item_templates(record_of(folder / "ITEM0.DAX", 1));
    const auto image = opengold::por::unpack_sound_executable(file_bytes(folder / "START.EXE"));
    const auto words = opengold::por::decode_item_words(image, countdown);
    const auto items = og::buck_rogers_starting_items(*templates);
    const auto named = [&](const br::Item &item)
    {
        std::string name;
        for (const auto part : item.name_parts)
        {
            if (part != 0)
            {
                name = words->at(part) + (name.empty() ? "" : " ") + name;
            }
        }
        return name;
    };
    check(named(items[1]) == "Bolt Gun" && items[1].quantity == 100,
          "The free weapon is a Bolt Gun with 100 rounds");
    check(named(items[0]) == "Spacesuit" && items[0].quantity == 0,
          "The second free item is template 20, a Spacesuit");
    const auto weapons = og::specializable_weapon_types(*templates);
    check(weapons.size() == 15 && weapons.front() == 15 && weapons.back() == 1,
          "Item templates 1 to 15 are the weapons a Warrior may specialize in");
    opengold::RandomState random{77};
    auto warrior = og::new_buck_rogers_character(Race::desert_runner, og::Gender::female,
                                                 Profession::warrior, countdown, random);
    check(og::specialize(warrior, weapons.front()), "A new Warrior specializes in one");
    warrior.name = "SANDY";
    const auto saved = br::encode_creature(warrior);
    check(saved.size() == 259 && br::decode_creature(saved)->name == "SANDY" &&
              br::encode_items(items).size() == 2 * 62,
          "It saves as a .who record of 259 bytes and a .stf of two 62-byte items");
}

void the_hail_is_corrected_where_it_is(const std::filesystem::path &folder)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, countdown);
    const std::array<std::pair<std::uint16_t, unsigned>, 3> hails{
        {{0x8DE8, 74}, {0x8E67, 70}, {0x8ED1, 69}}};
    for (const auto &[address, shipped] : hails)
    {
        const auto use = std::ranges::find_if(listing.skill_checks,
                                              [address](const opengold::SkillCheckUse &check_use)
                                              {
                                                  return check_use.place.script == 34 &&
                                                         check_use.place.address == address;
                                              });
        check(use != listing.skill_checks.end() && use->place.archive == "ECL2.DAX" &&
                  use->skill.values == std::set{shipped},
              "The hail checks the shipped skill there");
        const auto meant =
            og::script_check_skill(countdown, use->place.archive, 34, address, shipped);
        check(meant == shipped + 2 && og::skill_definition(og::skill_numbered(meant)).offered,
              "The corrected skill is two higher, and one a character can learn");
    }
}

void the_area_weapons_are_the_games(const std::filesystem::path &folder)
{
    // gdconsts.pas projectileKinds against ITEM0.DAX and ITEMS: the plasma
    // thrower, the rocket launcher and the seven grenades fire the
    // projectiles the fight plays, and the ITEMS rows the ground combat
    // checks use are the game's.
    const auto templates = br::decode_item_templates(record_of(folder / "ITEM0.DAX", 1));
    const auto types = br::decode_item_types(file_bytes(folder / "ITEMS"));
    check(templates && types, "ITEM0 and ITEMS decode");
    const std::array<std::pair<std::size_t, int>, 9> fired{
        {{17, 10}, {18, 11}, {32, 6}, {33, 6}, {34, 7}, {35, 12}, {36, 5}, {37, 8}, {38, 9}}};
    for (const auto &[number, kind] : fired)
    {
        const auto &item = templates->at(number - 1);
        check(og::projectile_kind(item) == kind &&
                  og::hits_an_area(static_cast<og::WeaponClass>(types->at(item.type).weapon_class)),
              "Template " + std::to_string(number) + " fires projectile " + std::to_string(kind));
    }
    check(og::projectile_kind(templates->at(14)) == 0 &&
              templates->at(14).type == og::ground_item::sonic_stunner,
          "The Sonic Stunner (template 15) fires no projectile");
    const auto made = ground_types();
    for (std::size_t type = 0; type < made.size(); ++type)
    {
        const auto &row = made[type];
        const auto &game = types->at(type);
        if (row.slot == 0 && row.attacks_doubled == 0 && row.weapon_class == 0)
        {
            continue;
        }
        check(row.slot == game.slot && row.attacks_doubled == game.attacks_doubled &&
                  row.weapon_class == game.weapon_class && row.heavy == game.heavy &&
                  row.dice_count == game.dice_count && row.dice_sides == game.dice_sides &&
                  row.damage_bonus == game.damage_bonus && row.range == game.range &&
                  row.consumable == game.consumable,
              "ITEMS row " + std::to_string(type) + " is as the checks have it");
    }
}

void the_files_decode()
{
    const auto folder = files_of(countdown);
    if (!folder)
    {
        return;
    }
    the_census_is_complete(*folder);
    the_records_decode(*folder);
    the_scripts_are_listed(*folder);
    the_records_save_as_they_were(*folder);
    a_new_character_uses_the_games_items(*folder);
    the_hail_is_corrected_where_it_is(*folder);
    the_area_weapons_are_the_games(*folder);
}

} // namespace

int main()
{
    try
    {
        the_commands_follow_the_buck_rogers_table();
        four_byte_operands_decode();
        the_empty_commands_never_go_on();
        creatures_are_read_from_their_layout();
        items_ships_and_types_are_read();
        the_menus_pair_races_and_careers();
        abilities_are_rolled_and_raised();
        the_tables_run_to_the_level_limit();
        hit_points_follow_the_career();
        saving_throws_follow_race_and_level();
        creation_makes_every_pair();
        training_raises_a_level();
        skills_have_groups_and_prerequisites();
        a_skill_value_adds_its_ability();
        skill_checks_have_four_grades();
        the_hail_checks_the_skills_it_names();
        creation_spends_skill_points();
        dependents_are_refunded();
        training_spends_fewer_points();
        warriors_specialize();
        modify_keeps_the_limits();
        a_new_character_carries_two_items();
        records_round_trip();
        the_attack_roll_climbs();
        range_and_cover_raise_the_target();
        rear_attacks_and_backstabs();
        the_gear_sets_the_numbers();
        what_stops_a_hit();
        area_weapons_blast();
        clouds_linger();
        ammunition_runs_out();
        injuries_follow_the_damage();
        the_new_commands_are_offered();
        every_difficulty_level();
        a_fight_runs_by_the_buck_rogers_rules();
        area_weapons_and_clouds_in_a_fight();
        medics_and_jetpacks_in_a_fight();
        zero_gravity_and_difficulty_in_a_fight();
        impassable_ground_lets_shots_across();
        the_files_decode();
        std::cout << "Countdown to Doomsday passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
