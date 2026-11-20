// Buck Rogers: Countdown to Doomsday, plan step B1 (docs/BUCK-ROGERS.md): its
// script commands, its records and what its scripts ask of the engine, first
// on made-up bytes, then on the game's own files when OPENGOLD_GOLDBOX_DIR
// names the folder holding them (docs/games/countdown.md). The whole census
// of the real files also runs in tests/game_census.cmake.
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_scripts.h"
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
#include <string>
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
        the_files_decode();
        std::cout << "Countdown to Doomsday passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
