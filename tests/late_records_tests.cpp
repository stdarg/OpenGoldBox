// The records Pools of Darkness and Treasures of the Savage Frontier share:
// their scripts, 510-byte creatures, 17-byte items and headerless item types
// (docs/games/darkness.md, docs/games/treasure.md). The census of the real
// files runs in tests/game_census.cmake.
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/late_records.h"
#include "opengold/pool_radiance.h"
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
using opengold::test::rejects;
using Bytes = std::vector<std::uint8_t>;

constexpr auto darkness = GoldBoxGame::pools_of_darkness;
constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;

// A 510-byte creature with each field the decoder reads set to its own value.
Bytes made_creature()
{
    Bytes record(510);
    const std::string name = "BULETTE";
    record[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), record.begin() + 1);
    record[16] = 17; // Strength, permanent.
    record[17] = 15; // Strength, current.
    record[29] = 50; // Exceptional Strength, permanent.
    record[172] = 48;
    record[174] = 2;
    record[178] = 41;
    record[304] = 7;
    record[305] = 3;
    record[310] = 11;
    record[312] = 9;
    record[327] = 0x80;
    record[333] = 12; // Gems.
    record[339] = 9;  // Fighter level.
    record[360] = 2;
    record[362] = 3; // First attack: 3d12-1.
    record[364] = 12;
    record[366] = 0xFF;
    record[368] = 62;
    record[381 + 2 * 9] = 4; // Magic-user slots of level 1.
    record[408] = 0xE8;      // 2792 experience for defeating it.
    record[409] = 0x0A;
    record[422] = 2;
    record[498] = 63;
    record[508] = 40;
    return record;
}

void creatures_are_read_from_their_layout()
{
    const auto record = made_creature();
    const auto creature = opengold::late::decode_creature(record, darkness);
    check(creature.has_value(), "A 510-byte record is a Pools of Darkness creature");
    check(creature->game == darkness && creature->name == "BULETTE", "The name comes first");
    check(creature->abilities.strength == 17 && creature->current_abilities.strength == 15 &&
              creature->abilities.exceptional_strength == 50,
          "Each ability keeps a permanent and a current score");
    check(creature->base_thac0 == 12 && creature->base_armor_class == -2,
          "THAC0 and Armor Class are 60 less their bytes");
    check(creature->character_class == 2 && creature->max_hit_points == 41,
          "Class and hit points follow THAC0");
    check(creature->icon_size_raw == 3 && creature->saves.spells == 11,
          "The icon size and saving throws follow the known spells");
    check(creature->highest_level_raw == 9, "Byte 312 holds the creature's Hit Dice");
    check(creature->wealth[5] == 12 && creature->wealth[3] == 0,
          "Only platinum, gems and jewelry are kept");
    check(creature->levels.fighter == 9, "The fighter level is the third");
    check(creature->base_attacks[0].attacks_per_two_rounds == 2 &&
              creature->base_attacks[0].damage.count == 3 &&
              creature->base_attacks[0].damage.sides == 12 &&
              creature->base_attacks[0].damage.modifier == -1,
          "The attacks' dice are kept field by field");
    check(creature->spell_slots[2][0] == 4, "The magic-user slots follow nine levels of druid's");
    check(creature->base_xp_award == 2792 && creature->item_count == 2,
          "The award and the number of items are read");
    check(creature->current.armor_class == -3 && creature->current.hit_points_raw == 40,
          "The current figures close the record");
    check(creature->attack_level == 7 && creature->spell_book.size() == 125,
          "Pools of Darkness knows 125 spells and keeps an attack level after them");

    const auto treasures_creature = opengold::late::decode_creature(record, treasures);
    check(treasures_creature && treasures_creature->attack_level == 0 &&
              treasures_creature->spell_book.size() == 126 &&
              treasures_creature->spell_book.back() == 7,
          "Treasures knows 126 spells, the last in that byte");

    check(!opengold::late::decode_creature(Bytes(439), darkness), "A 439-byte record is not one");
    check(!opengold::late::decode_creature(record, GoldBoxGame::secret_of_the_silver_blades),
          "Secret's records have their own layout");
    auto long_name = record;
    long_name[0] = 16;
    check(!opengold::late::decode_creature(long_name, darkness), "A name has at most 15 letters");
    check(opengold::por::decode_character(record, treasures).has_value(),
          "decode_character reads the game's own layout");
}

void items_are_the_last_bytes_of_the_earlier_items()
{
    const Bytes short_item{1, 0, 0, 1, 2, 0xFE, 1, 4, 0, 75, 0, 3, 25, 0, 5, 6, 7};
    Bytes earlier_item(46);
    earlier_item.insert(earlier_item.end(), short_item.begin(), short_item.end());
    const auto late = opengold::por::decode_items(short_item, darkness);
    const auto early = opengold::por::decode_items(earlier_item);
    check(late && late->size() == 1 && early && early->size() == 1,
          "A 17-byte item decodes as the earlier games' 63 bytes do");
    const auto &item = late->front();
    check(item.raw == early->front().raw && item.stored_name.empty(),
          "Its raw record is the earlier item with no name");
    check(item.type == 1 && item.name_components[2] == 1 && item.magic_bonus == 2 &&
              item.save_bonus == -2 && item.readied() && item.hidden_name_words == 4 &&
              item.weight == 75 && item.stack_size == 3 && item.value == 25 &&
              item.effect_codes[2] == 7,
          "Each field is in its place");
    check(!opengold::por::decode_items(Bytes(18), treasures), "A list is whole items");
    check(opengold::por::decode_items(Bytes{}, treasures)->empty(), "An empty list has no items");
}

void item_types_have_no_header()
{
    Bytes table(2048);
    table[16 + 1] = 2; // Type 1: two hands, 1d8 against large creatures.
    table[16 + 2] = 1;
    table[16 + 3] = 8;
    table[16 + 13] = 0xE8;
    table[16 + 14] = opengold::por::item_property::strength_bonus;
    const auto types = opengold::por::decode_item_templates(table, darkness);
    check(types && types->size() == 128, "ITEMS holds 128 item types and nothing before them");
    const auto &axe = types->at(1);
    check(axe.hands == 2 && axe.large_damage.count == 1 && axe.large_damage.sides == 8 &&
              axe.class_restrictions == 0xE8 &&
              axe.has(opengold::por::item_property::strength_bonus),
          "A type's fields are as in the earlier games");
    check(!opengold::por::decode_item_templates(table, GoldBoxGame::secret_of_the_silver_blades),
          "The earlier games' files start with two header bytes");
}

// A script of five entry jumps to 8014h and then the body.
Bytes script(const Bytes &body)
{
    Bytes record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {1, 1, 0x14, 0x80});
    }
    record.insert(record.end(), body.begin(), body.end());
    return record;
}

void scripts_decode_with_the_changed_commands()
{
    // TREASURE: platinum 5, no gems or jewelry, two items (7 and 9);
    // LOAD FILES of a map and one wall set; CLEAR AND EXIT, then data.
    const auto record =
        script({0x27, 0, 5, 0, 0, 0, 0, 0, 2, 0, 7, 0, 9, 0x21, 0, 81, 0, 5, 0x23, 0x4C, 0x05});
    const auto program = opengold::por::EclProgram::decode(record, "made", darkness);
    check(program.memory().origin == 0x8000 && program.memory().limit == 0x9E00,
          "Pools of Darkness' scripts load at 8000h, as Secret's");
    const auto treasure = program.instruction(0x8014);
    check(opengold::por::ecl_opcode(treasure.opcode, darkness).name == "TREASURE" &&
              treasure.operands.size() == 6 && treasure.operands[5].value == 9,
          "TREASURE takes three kinds of wealth and a list of items");
    const auto load = program.instruction(treasure.next);
    check(load.operands.size() == 2 && load.operands[0].value == 81 && load.operands[1].value == 5,
          "LOAD FILES takes a map and one wall set");
    const auto walk = opengold::por::walk_reachable(program);
    check(walk.errors.empty() && walk.instructions.size() == 3,
          "CLEAR AND EXIT ends the script, and the data after it is not read");
    check(opengold::por::ecl_opcode(0x23, darkness).name == "CLEAR AND EXIT" &&
              opengold::por::ecl_opcode(0x2C, darkness).requires_host,
          "Pools of Darkness' commands run, PLAY SOUND by the host");
    rejects(
        [&]
        {
            static_cast<void>(opengold::por::ecl_opcode(0x42, darkness));
        },
        "Pools of Darkness has no command 42h");

    // LOAD WALL SETS: map 16 with wall sets 9, 10 and 11.
    const auto wall_sets = script({0x42, 0, 16, 0, 9, 0, 10, 0, 11, 0});
    const auto treasures_program = opengold::por::EclProgram::decode(wall_sets, "made", treasures);
    check(treasures_program.memory().limit == 0xB6B1, "Treasures' scripts may run to B6B0h");
    const auto load_walls = treasures_program.instruction(0x8014);
    check(opengold::por::ecl_opcode(load_walls.opcode, treasures).name == "LOAD WALL SETS" &&
              load_walls.operands.size() == 4 && load_walls.operands[3].value == 11,
          "Treasures' LOAD WALL SETS names a map and three wall sets");
}

Bytes file_bytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const Bytes &record_of(const std::vector<opengold::DaxRecord> &records, unsigned id)
{
    for (const auto &record : records)
    {
        if (record.id == id)
        {
            return record.bytes;
        }
    }
    throw std::runtime_error("No record " + std::to_string(id));
}

std::vector<opengold::DaxRecord> archive(const std::filesystem::path &path)
{
    auto decoded = opengold::decode_dax_archive(file_bytes(path));
    check(static_cast<bool>(decoded), path.filename().string() + " is an archive");
    return std::move(decoded.records);
}

// The real files, when the developer has them (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / opengold::game_folder(game)
                              : std::filesystem::path();
    if (!games || !std::filesystem::exists(folder / "MON1CHA.DAX"))
    {
        std::cout << opengold::game_title(game) << "'s files are not here; they are not read.\n";
        return std::nullopt;
    }
    return folder;
}

void the_files_of_pools_of_darkness_decode()
{
    const auto folder = files_of(darkness);
    if (!folder)
    {
        return;
    }
    const auto creatures = archive(*folder / "MON1CHA.DAX");
    const auto bulette = opengold::por::decode_character(record_of(creatures, 1), darkness);
    check(bulette && bulette->name == "BULETTE" && bulette->highest_level_raw == 9 &&
              bulette->base_thac0 == 12 && bulette->base_armor_class == -2 &&
              bulette->max_hit_points == 41 && bulette->base_xp_award == 2792,
          "Creature 1 is the bulette: 9 Hit Dice, THAC0 12, AC -2, 41 hit points");

    const auto items =
        opengold::por::decode_items(record_of(archive(*folder / "ITEM0.DAX"), 1), darkness);
    check(items && items->size() == 254, "ITEM0 holds 254 item definitions");
    check(items->at(1).type == 1 && items->at(1).name_components[2] == 1 &&
              items->at(1).weight == 75,
          "Definition 2 is a battle axe of 75 weight");
    const auto carried =
        opengold::por::decode_items(record_of(archive(*folder / "MON1ITM.DAX"), 12), darkness);
    check(carried && carried->size() == 1 && carried->front().type == 73,
          "Creature 12 carries one item, of type 73");

    const auto types =
        opengold::por::decode_item_templates(file_bytes(*folder / "ITEMS"), darkness);
    check(types && types->size() == 128 && types->at(1).hands == 2,
          "ITEMS starts with its empty type 0; type 1, the battle axe, takes two hands");

    const auto scripts = archive(*folder / "ECL1.DAX");
    const auto program =
        opengold::por::EclProgram::decode(record_of(scripts, 4), "ECL1 4", darkness);
    const auto music = program.instruction(program.entries()[4]);
    const auto load = program.instruction(music.next);
    check(opengold::por::ecl_opcode(music.opcode, darkness).name == "PLAY SOUND" &&
              music.operands[0].value == 0x81 && load.opcode == 0x21 &&
              load.operands[0].value == 81 && load.operands[1].value == 5,
          "Script 4 starts the music, then loads map 81 with wall set 5");
}

void the_files_of_treasures_decode()
{
    const auto folder = files_of(treasures);
    if (!folder)
    {
        return;
    }
    const auto creatures = archive(*folder / "MON1CHA.DAX");
    const auto orc = opengold::por::decode_character(record_of(creatures, 12), treasures);
    check(orc && orc->name == "ORC" && orc->highest_level_raw == 1 && orc->base_thac0 == 18 &&
              orc->base_armor_class == 6 && orc->levels.fighter == 1,
          "Creature 12 is the orc: 1 Hit Die, THAC0 18, AC 6");

    const auto carried =
        opengold::por::decode_items(record_of(archive(*folder / "MON1ITM.DAX"), 2), treasures);
    check(carried && carried->size() == 3 && carried->front().type == 1,
          "Creature 2 carries three items, the first a battle axe");

    const auto scripts = archive(*folder / "ECL1.DAX");
    const auto program =
        opengold::por::EclProgram::decode(record_of(scripts, 4), "ECL1 4", treasures);
    const auto load_walls = program.instruction(0x804E);
    check(load_walls.opcode == 0x42 && load_walls.operands[0].value == 16 &&
              load_walls.operands[1].value == 9 && load_walls.operands[3].value == 11,
          "Script 4 loads map 16 with wall sets 9, 10 and 11");
}

} // namespace

int main()
{
    try
    {
        creatures_are_read_from_their_layout();
        items_are_the_last_bytes_of_the_earlier_items();
        item_types_have_no_header();
        scripts_decode_with_the_changed_commands();
        the_files_of_pools_of_darkness_decode();
        the_files_of_treasures_decode();
        std::cout << "Late records passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
