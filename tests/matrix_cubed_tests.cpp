// Buck Rogers: Matrix Cubed, plan steps B1 and B2 (docs/BUCK-ROGERS.md): its
// music files through the Audio Interface Library and the rules where its
// characters differ from Countdown's, first on made-up bytes, then the
// game's own files when its folder is found (installed_games.h,
// docs/games/matrix-cubed.md). Countdown to Doomsday's tests cover the
// script commands and records both games share; the whole census of the real
// files also runs in tests/game_census.cmake.
#include "opengold/ail_files.h"
#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_scripts.h"
#include "opengold/campaign_encounter.h"
#include "opengold/difficulty.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "opengold/ua_media.h"
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
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;

constexpr auto matrix = GoldBoxGame::matrix_cubed;

void put_word(Bytes &bytes, std::size_t at, unsigned value)
{
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void drivers_are_recognized()
{
    constexpr std::string_view notice = "Copyright (C) 1991 John Miles";
    Bytes driver(64);
    put_word(driver, 0, 0x20);
    std::copy(notice.begin(), notice.end(), driver.begin() + 2);
    put_word(driver, 0x20, 100); // Function 100 at 30h,
    put_word(driver, 0x22, 0x30);
    put_word(driver, 0x24, 0xFFFF); // then the table's end.
    const auto read = opengold::ail::decode_driver(driver);
    check(read && read->functions.size() == 1 && read->functions[0][0] == 100,
          "A driver lists its functions after John Miles' notice");
    driver[2] = 'c';
    check(!opengold::ail::decode_driver(driver), "Without the notice it is not a driver");
}

void the_timbre_library_is_indexed()
{
    // Two timbres, each a size word and two bytes, after a 14-byte index.
    Bytes library(14 + 8);
    library[0] = 3; // Patch 3, bank 0, at 14.
    put_word(library, 2, 14);
    library[6] = 5; // Patch 5, bank 1, at 18.
    library[7] = 1;
    put_word(library, 8, 18);
    library[13] = 0xFF; // The index ends.
    put_word(library, 14, 4);
    put_word(library, 18, 4);
    const auto timbres = opengold::ail::decode_global_timbre_library(library);
    check(timbres && timbres->size() == 2 && (*timbres)[1].patch == 5 && (*timbres)[1].bank == 1 &&
              (*timbres)[1].bytes.size() == 4,
          "Each index entry names a patch, a bank and where the timbre starts");
    put_word(library, 18, 9);
    check(!opengold::ail::decode_global_timbre_library(library),
          "A timbre may not run past the end");
}

void the_bank_and_catalog_are_read()
{
    constexpr std::string_view signature = "ADLIB-";
    Bytes bank(28 + 2 * 12 + 30);
    bank[0] = 1;
    std::copy(signature.begin(), signature.end(), bank.begin() + 2);
    put_word(bank, 8, 1);  // One instrument used,
    put_word(bank, 10, 2); // two names,
    put_word(bank, 12, 28);
    put_word(bank, 16, 28 + 24);
    bank[28 + 2] = 1; // the first used, named PIANO.
    const std::string_view piano = "PIANO";
    std::copy(piano.begin(), piano.end(), bank.begin() + 28 + 3);
    const auto instruments = opengold::ail::decode_instrument_bank(bank);
    check(instruments && instruments->size() == 1 && instruments->front().name == "PIANO",
          "An AdLib bank names its instruments");

    const std::string text = ";a catalog\r\noutfile(\"x.ad\")\r\n"
                             "timbre(1,7) = BNK_inst(\"piano\",0)   ;a piano\r\n";
    const auto catalog = opengold::ail::decode_timbre_catalog(Bytes(text.begin(), text.end()));
    check(catalog && catalog->size() == 1 && catalog->front().bank == 1 &&
              catalog->front().patch == 7 && catalog->front().instrument == "piano",
          "The catalog assigns a bank's instrument to each bank and patch");
    const std::string broken = "timbre(1 = BNK_inst(\"piano\",0)\n";
    check(!opengold::ail::decode_timbre_catalog(Bytes(broken.begin(), broken.end())),
          "A timbre line that cannot be read is refused");
}

// The rules of step B2 where Matrix Cubed's differ from Countdown's
// (docs/games/matrix-cubed.md, "Rules"); Countdown's tests cover the rest.
// Each test names the routine of the original it follows.

namespace br = opengold::buck_rogers;
namespace og = opengold::original;
using og::Profession;
using og::Race;
using og::Skill;
using og::SkillRefusal;

std::array<int, 8> saves_of(const br::Creature &character)
{
    std::array<int, 8> saves{};
    std::copy(character.saves.begin(), character.saves.end(), saves.begin());
    return saves;
}

void the_tables_run_to_level_20()
{
    // chargen.pas MAX_LEVEL, AddLevelHitPoints; gdconsts.pas experienceNeeded, careerBaseCombat.
    check(og::buck_rogers_starting_level(matrix) == 7 && og::buck_rogers_level_limit(matrix) == 20,
          "Matrix Cubed starts at level 7 and stops at 20");
    check(og::career_experience_for_level(Profession::warrior, 20, matrix) == 256000000U &&
              og::career_experience_for_level(Profession::rocket_jock, 7, matrix) == 40000U &&
              !og::career_experience_for_level(Profession::warrior, 21, matrix),
          "The tables run to level 20 and no further");
    check(
        og::career_base_combat(Profession::warrior, 20, matrix) == 59 &&
            og::career_base_combat(Profession::medic, 20, matrix) == 52 &&
            og::career_base_combat(Profession::warrior, 9, matrix) ==
                og::career_base_combat(Profession::warrior, 9, GoldBoxGame::countdown_to_doomsday),
        "Combat value goes on rising, the same as Countdown's to level 9");
    opengold::RandomState random{8};
    bool rolled_past_three = false;
    for (int roll = 0; roll < 100; ++roll)
    {
        rolled_past_three = rolled_past_three || og::roll_level_hit_points(Profession::warrior, 9,
                                                                           10, matrix, random) > 3;
    }
    check(rolled_past_three &&
              og::roll_level_hit_points(Profession::warrior, 10, 10, matrix, random) == 3 &&
              og::roll_level_hit_points(Profession::medic, 10, 10, matrix, random) == 2,
          "Level 9 is still rolled; from 10 the gain is fixed");
}

void creation_makes_every_pair()
{
    // chargen.pas CreateCharacterMenu with its level 7 free training.
    opengold::RandomState random{1992};
    for (const auto race : og::buck_rogers_creation_races())
    {
        for (const auto career : og::professions_for(race, matrix))
        {
            const auto made =
                og::new_buck_rogers_character(race, og::Gender::male, career, matrix, random);
            check(made.level == 7 &&
                      made.experience == og::career_experience_for_level(career, 7, matrix) &&
                      made.credits == 1000 &&
                      made.base_combat == og::career_base_combat(career, 7, matrix),
                  "A new character is trained free to level 7, with 1,000 credits");
            check(made.hit_point_maximum >= og::buck_rogers_hit_point_minimum(made) &&
                      made.hit_point_maximum <= og::buck_rogers_hit_point_maximum(made),
                  "Seven levels of Hit Points, within what Modify allows");
            check(saves_of(made) == og::buck_rogers_saving_throws(race, 7),
                  "Levels 3 and 6 improve the saves");
            check(og::buck_rogers_modifiable(made, matrix), "Modify takes a new character");
        }
    }
    check(og::creation_specializations(Profession::warrior, matrix) == 3,
          "A new Warrior chooses three specializations");
}

void a_reroll_starts_the_saves_over()
{
    // chargen.pas CreateCharacterMenu: the original lowered the saves again at
    // every reroll, as InitializeSaves runs before the reroll loop.
    opengold::RandomState random{4};
    auto character = og::new_buck_rogers_character(Race::venusian, og::Gender::female,
                                                   Profession::medic, matrix, random);
    for (int reroll = 0; reroll < 5; ++reroll)
    {
        og::reroll_buck_rogers_character(character, matrix, random);
    }
    check(saves_of(character) == og::buck_rogers_saving_throws(Race::venusian, 7) &&
              character.level == 7 && character.hit_points == character.hit_point_maximum,
          "After five rerolls the saves are a level 7 character's, not better");
}

void training_at_the_limit_keeps_experience()
{
    // chargen.pas TrainingMenu, which read experienceNeeded past level 20.
    opengold::RandomState random{12};
    auto character = og::new_buck_rogers_character(Race::terran, og::Gender::male,
                                                   Profession::rogue, matrix, random);
    character.level = 19;
    character.experience = 200000000;
    og::train_buck_rogers_character(character, matrix, random);
    check(character.level == 20 && character.experience == 200000000,
          "Training to level 20 leaves the experience (the original cut it to 1,499)");
    check(og::training_refusal(character, matrix) == og::TrainingRefusal::at_level_limit,
          "Level 20 is the last");
    character.level = 8;
    character.experience = 110000;
    check(og::training_refusal(character, matrix) == og::TrainingRefusal::none &&
              og::training_refusal(character, GoldBoxGame::countdown_to_doomsday) ==
                  og::TrainingRefusal::at_level_limit,
          "A Countdown character at level 8 trains on in Matrix Cubed");
}

void new_characters_start_with_preset_skills()
{
    // skills.pas ApplyInitialSkills, EditSkills(0, 0, 105, 1); gdconsts.pas careerInitialSkills.
    opengold::RandomState random{30};
    const auto rogue = og::new_buck_rogers_character(Race::martian, og::Gender::female,
                                                     Profession::rogue, matrix, random);
    auto spending = og::SkillSpending::at_creation(rogue, matrix);
    check(spending.career_points() == 0 && spending.general_points() == 0 &&
              spending.max_per_skill() == 105,
          "No points to spend; at most 105 above a skill's start");
    for (const auto &preset : og::preset_skills(Profession::rogue, matrix))
    {
        check(og::level_of(spending.levels(), preset.skill) == preset.level,
              "Each preset skill is at its level");
    }
    check(og::preset_skills(Profession::rogue, GoldBoxGame::countdown_to_doomsday).empty(),
          "Countdown has no preset skills");
    check(spending.add(Skill::notice) == SkillRefusal::no_points, "Points come only from presets");
    for (int point = 0; point < 30; ++point)
    {
        check(spending.subtract(Skill::notice), "A preset skill gives its points back");
    }
    check(!spending.subtract(Skill::notice) && og::level_of(spending.levels(), Skill::notice) == 10,
          "Down to the skill's start, a Rogue's 10 in Notice");
    check(spending.career_points() == 30 && spending.add(Skill::climb) == SkillRefusal::none,
          "Points taken from one career skill go to another");
    check(spending.known_general_skills() == 4, "A Rogue's preset general skills are four");
    for (int point = 0; point < 10; ++point)
    {
        static_cast<void>(spending.subtract(Skill::first_aid));
    }
    const std::array learned{Skill::astronomy, Skill::acrobatics, Skill::disguise,
                             Skill::etiquette, Skill::singing,    Skill::tracking,
                             Skill::shadowing, Skill::act,        Skill::distract};
    for (const auto skill : learned)
    {
        check(spending.add(skill) == SkillRefusal::none, "Up to twelve general skills at level 7");
    }
    check(spending.add(Skill::intimidate) == SkillRefusal::no_new_skills,
          "A thirteenth is refused");
}

void treat_light_wounds_takes_critical_back_too()
{
    // skills.pas AdjustPrerequisites (Matrix Cubed's Treat Light Wounds row).
    opengold::RandomState random{6};
    const auto medic = og::new_buck_rogers_character(Race::terran, og::Gender::female,
                                                     Profession::medic, matrix, random);
    auto spending = og::SkillSpending::at_creation(medic, matrix);
    for (int point = 0; point < 21; ++point)
    {
        check(spending.subtract(Skill::treat_light_wounds), "Light Wounds may be lowered");
    }
    check(og::level_of(spending.levels(), Skill::treat_serious_wounds) == 0 &&
              og::level_of(spending.levels(), Skill::treat_critical_wounds) == 0 &&
              spending.career_points() == 21 + 40 + 75,
          "Below 30 Light, Serious and Critical Wounds go and give their points back");
}

void the_hail_checks_the_skills_it_names()
{
    // The hail menu of ECL1 script 19 (the user, 2026-11-20: fix it as a bug).
    check(og::script_check_skill(matrix, "ECL1.DAX", 19, 0x932E, 74) == 76 &&
              og::script_check_skill(matrix, "ECL1.DAX", 19, 0x93B7, 70) == 72 &&
              og::script_check_skill(matrix, "ECL1.DAX", 19, 0x9422, 69) == 71 &&
              og::script_check_skill(matrix, "ECL1.DAX", 19, 0x9422, 70) == 70,
          "Bluff, intimidate and ask for help check Fast Talk, Intimidate and Act");
}

void ground_combat_is_countdowns()
{
    // Matrix Cubed's cbtacts.pas, fxengine.pas and eclvm.pas play ground
    // combat as Countdown's; what its levels change is a Rogue's backstab
    // past level 8, (level + 7) / 4 (CalcWeaponDamage).
    br::Creature rogue;
    rogue.career = og::record_career(Profession::rogue);
    rogue.level = 20;
    check(og::backstab_damage(3, rogue) == 18, "A level 20 Rogue backstabs for six times");
    check(og::game_of(og::identity(matrix)) == matrix,
          "Its fights keep Matrix Cubed's identity, which keys its rules");
    opengold::CampaignEncounter encounter;
    encounter.enemies.emplace_back();
    opengold::apply_original_difficulty(encounter, opengold::DifficultySetting::infernal, matrix);
    check(encounter.enemies.front().original_strength.hit_points_percent == 200 &&
              encounter.party_to_hit == -3,
          "Admiral doubles the monsters' Hit Points and takes 3 from the party's to-hit");
    // A fight under its key runs by the Buck Rogers rules.
    br::Creature fighter = rogue;
    fighter.level = 7;
    fighter.enabled = 1;
    fighter.status = 1;
    fighter.base_combat = 44;
    fighter.base_armor_class = 50;
    fighter.base_movement = 12;
    fighter.hit_points = fighter.hit_point_maximum = 30;
    fighter.abilities = {12, 14, 12, 13, 11, 13, 10};
    fighter.unarmed_attacks_doubled = 2;
    fighter.unarmed_dice_count = {1, 0};
    fighter.unarmed_dice_sides = {2, 0};
    fighter.saves = {15, 14, 14, 13, 13, 16, 13, 13};
    opengold::rules::Encounter fight;
    fight.battlefield.width = 8;
    fight.battlefield.height = 8;
    fight.battlefield.terrain.assign(64, opengold::rules::Terrain::open);
    for (const auto &[id, side, cell] :
         {std::tuple{1U, opengold::rules::Side::party, opengold::rules::Cell{2, 2}},
          std::tuple{2U, opengold::rules::Side::opposition, opengold::rules::Cell{3, 2}}})
    {
        opengold::rules::Participant p;
        p.id = id;
        p.name = p.definition = id == 1 ? "TEAM ROGUE" : "SPACE PIRATE";
        p.side = side;
        p.cell = cell;
        p.character_profile = og::encode_ground_record({fighter, {}, {}});
        fight.participants.push_back(p);
    }
    og::Content content;
    content.game = matrix;
    const auto module = og::rules_module(std::move(content));
    const auto session = module->create(std::move(fight), 12);
    const auto commands = session->legal_commands();
    check(std::ranges::any_of(commands,
                              [](const opengold::rules::Command &command)
                              {
                                  return command.verb == "dodge";
                              }) &&
              std::ranges::none_of(commands,
                                   [](const opengold::rules::Command &command)
                                   {
                                       return command.verb == "cast" || command.verb == "turn";
                                   }),
          "Its combat menu is the Buck Rogers one");
    check(session->snapshot().combatants.front().armor_class == 50,
          "Its Armor Class climbs from 50");
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
    check(opengold::identify_game(folder) == matrix, "The folder is Matrix Cubed's");
    const auto census = opengold::take_census(folder, matrix);
    check(census.complete(), "Every file of Matrix Cubed decodes");
    check(census.foreign_files ==
              std::vector<std::string>{"SEG00036.001", "SEG00036.002", "JOURNAL/"},
          "Two text fragments and an empty folder are not the game's");
}

void the_records_decode(const std::filesystem::path &folder)
{
    const auto refuge =
        opengold::buck_rogers::decode_creature(record_of(folder / "MON0CHA.DAX", 1));
    check(refuge && refuge->name == "SID REFUGE" && refuge->level == 12 &&
              refuge->hit_point_maximum == 84 && refuge->raw.size() == 259,
          "Creature 1 is a level 12 Sid refuge in a 259-byte record");
    const auto image = opengold::por::unpack_sound_executable(file_bytes(folder / "START.EXE"));
    const auto words = opengold::por::decode_item_words(image, matrix);
    check(words && words->size() == 46 && words->at(1) == "Knife",
          "START.EXE names items with 45 words, one more than Countdown's");
    const auto sounds = opengold::por::load_sound_effects(folder, matrix);
    check(sounds.size() == 21 && sounds[9].audible(), "START.EXE's driver plays the footstep");
}

void the_music_needs_only_its_library(const std::filesystem::path &folder)
{
    const auto library =
        opengold::ail::decode_global_timbre_library(file_bytes(folder / "FATV9.AD"));
    check(library && library->size() == 152, "FATV9.AD holds 152 timbres");
    for (const auto *name : {"BUCKA.XMI", "BUCKB.XMI", "BUCKC.XMI"})
    {
        const auto songs = opengold::ua::decode_xmi(file_bytes(folder / name));
        check(songs && !songs->empty(), std::string(name) + " holds songs");
        for (const auto &song : *songs)
        {
            for (const auto &[patch, bank] : song.timbres)
            {
                check(std::any_of(library->begin(), library->end(),
                                  [&](const opengold::ail::GlobalTimbre &timbre)
                                  {
                                      return timbre.patch == patch && timbre.bank == bank;
                                  }),
                      std::string(name) + " asks only for timbres FATV9.AD holds");
            }
        }
    }
    for (const auto *name : {"ADLIB.ADV", "SBFM.ADV", "MT32MPU.ADV", "PCSPKR.ADV", "TANDY.ADV"})
    {
        check(opengold::ail::decode_driver(file_bytes(folder / name)).has_value(),
              std::string(name) + " is an AIL driver");
    }
}

void the_scripts_are_listed(const std::filesystem::path &folder)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, matrix);
    check(listing.stalls.empty(), "No script reaches 3Bh or 3Ch");
    check(listing.uneven_skips.empty(), "No IF skips a command the original skips unevenly");
    check(listing.skill_checks.size() == 120, "The scripts check skills 120 times");
    check(listing.ship_combats.size() == 4, "Four ship combats");
    const auto end = std::find_if(listing.programs.begin(), listing.programs.end(),
                                  [](const opengold::ScriptCommandUse &use)
                                  {
                                      return use.operands == std::vector<std::string>{"2"};
                                  });
    check(end != listing.programs.end() && end->place.script == 114 &&
              end->place.address == 0x9E08 &&
              std::count_if(listing.programs.begin(), listing.programs.end(),
                            [](const opengold::ScriptCommandUse &use)
                            {
                                return use.operands == std::vector<std::string>{"2"};
                            }) == 1,
          "Only script 114 ends the game, with PROGRAM 2 after the heroes' welcome");
}

void the_records_save_as_they_were(const std::filesystem::path &folder)
{
    const auto characters = opengold::decode_dax_archive(file_bytes(folder / "MON0CHA.DAX"));
    for (const auto &record : characters.records)
    {
        const auto creature = br::decode_creature(record.bytes);
        check(creature && br::encode_creature(*creature) == record.bytes,
              "Each of Matrix Cubed's creatures saves byte for byte");
    }
    check(characters.records.size() == 63, "All 63 creatures round trip");
    const auto item_lists = opengold::decode_dax_archive(file_bytes(folder / "MON0ITM.DAX"));
    for (const auto &record : item_lists.records)
    {
        const auto items = br::decode_items(record.bytes);
        check(items && br::encode_items(*items) == record.bytes,
              "Each item list saves byte for byte");
    }
}

void the_hail_is_corrected_where_it_is(const std::filesystem::path &folder)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, matrix);
    const std::array<std::pair<std::uint16_t, unsigned>, 3> hails{
        {{0x932E, 74}, {0x93B7, 70}, {0x9422, 69}}};
    for (const auto &[address, shipped] : hails)
    {
        const auto use = std::ranges::find_if(listing.skill_checks,
                                              [address](const opengold::SkillCheckUse &check_use)
                                              {
                                                  return check_use.place.script == 19 &&
                                                         check_use.place.address == address;
                                              });
        check(use != listing.skill_checks.end() && use->place.archive == "ECL1.DAX" &&
                  use->skill.values == std::set{shipped},
              "The hail checks the shipped skill there");
        check(og::script_check_skill(matrix, use->place.archive, 19, address, shipped) ==
                  shipped + 2,
              "and the corrected one is two higher");
    }
}

void the_files_decode()
{
    const auto folder = files_of(matrix);
    if (!folder)
    {
        return;
    }
    the_census_is_complete(*folder);
    the_records_decode(*folder);
    the_music_needs_only_its_library(*folder);
    the_scripts_are_listed(*folder);
    the_records_save_as_they_were(*folder);
    the_hail_is_corrected_where_it_is(*folder);
}

} // namespace

int main()
{
    try
    {
        drivers_are_recognized();
        the_timbre_library_is_indexed();
        the_bank_and_catalog_are_read();
        the_tables_run_to_level_20();
        creation_makes_every_pair();
        a_reroll_starts_the_saves_over();
        training_at_the_limit_keeps_experience();
        new_characters_start_with_preset_skills();
        treat_light_wounds_takes_critical_back_too();
        the_hail_checks_the_skills_it_names();
        ground_combat_is_countdowns();
        the_files_decode();
        std::cout << "Matrix Cubed passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
