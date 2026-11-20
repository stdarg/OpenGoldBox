// Buck Rogers: Countdown to Doomsday, plan steps B1 and B2 (docs/BUCK-ROGERS.md):
// its script commands, its records, what its scripts ask of the engine and
// the rules of its characters (careers, skills, creation, training), first
// on made-up bytes, then on the game's own files when OPENGOLD_GOLDBOX_DIR
// names the folder holding them (docs/games/countdown.md). The whole census
// of the real files also runs in tests/game_census.cmake.
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_scripts.h"
#include "opengold/dice.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
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

std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / opengold::game_folder(game)
                              : std::filesystem::path();
    if (!games || !std::filesystem::exists(folder / "SHIPS.DAX"))
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
        the_files_decode();
        std::cout << "Countdown to Doomsday passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
