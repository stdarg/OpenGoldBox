// Telling the Gold Box games apart and reading what Curse of the Azure Bonds
// changed (docs/GAMES.md, docs/games/curse.md). The census of the real files
// runs in tests/game_census.cmake.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_art.h"
#include "opengold/creature_catalog.h"
#include "opengold/curse_records.h"
#include "opengold/ecl.h"
#include "opengold/ecl_address.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/game_census.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/sound_bank.h"
#include "opengold/srd5.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using opengold::test::rejects;

std::filesystem::path empty_folder(const std::string &name)
{
    const auto folder = std::filesystem::temp_directory_path() / ("opengold-games-" + name);
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

void each_game_is_told_apart_by_its_files()
{
    const std::vector<std::pair<std::string, GoldBoxGame>> markers{
        {"SQRPACI.DAX", GoldBoxGame::pool_of_radiance},
        {"tiles.dax", GoldBoxGame::curse_of_the_azure_bonds},
        {"SOUND.DAX", GoldBoxGame::secret_of_the_silver_blades},
        {"ITEMS.1", GoldBoxGame::gateway_to_the_savage_frontier},
        {"INTRO.BMA", GoldBoxGame::pools_of_darkness},
        {"LUCK.DAX", GoldBoxGame::treasures_of_the_savage_frontier},
        {"CKIT.EXE", GoldBoxGame::unlimited_adventures}};
    for (const auto &[marker, game] : markers)
    {
        const auto folder = empty_folder(marker);
        std::ofstream(folder / "GEO1.DAX").put('\0');
        std::ofstream(folder / marker).put('\0');
        check(opengold::identify_game(folder) == game,
              marker + " must identify " + std::string(opengold::game_title(game)));
        std::filesystem::remove_all(folder);
    }
    const auto folder = empty_folder("none");
    std::ofstream(folder / "GEO1.DAX").put('\0');
    check(!opengold::identify_game(folder), "A folder without a game's files is no game");
    std::filesystem::remove_all(folder);
}

// Curse's 422-byte creature record, each field at its place.
void curse_creatures_are_read_from_their_layout()
{
    std::vector<std::uint8_t> bytes(422);
    const std::string name = "ETTIN";
    bytes[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), bytes.begin() + 1);
    bytes[16] = 18;  // Strength, permanent
    bytes[17] = 17;  // and current.
    bytes[27] = 9;   // Charisma, current.
    bytes[29] = 50;  // Exceptional Strength 18/50.
    bytes[115] = 50; // THAC0 10.
    bytes[116] = 7;
    bytes[117] = 2;
    bytes[120] = 70;
    bytes[121] = 47; // First known spell.
    bytes[228] = 12;
    bytes[251] = 0xd2; // Copper 1234.
    bytes[252] = 0x04;
    bytes[267] = 10;   // Fighter level.
    bytes[284] = 4;    // Four attacks in two rounds,
    bytes[286] = 2;    // two
    bytes[288] = 8;    // eight-sided dice
    bytes[290] = 0xff; // minus one.
    bytes[292] = 57;   // Armor Class 3.
    bytes[295] = 0x50; // 1950 experience.
    bytes[296] = 0x07;
    bytes[316] = 0x9e; // 1950 experience for killing it.
    bytes[317] = 0x07;
    bytes[332] = 3;
    bytes[407] = 1;
    bytes[420] = 66;
    bytes[421] = 9;

    const auto creature = opengold::curse::decode_creature(bytes);
    check(creature.has_value(), "A 422-byte record must decode");
    check(creature->game == GoldBoxGame::curse_of_the_azure_bonds && creature->raw == bytes,
          "It keeps its game and bytes");
    check(creature->name == name, "The name");
    check(creature->abilities.strength == 18 && creature->current_abilities.strength == 17,
          "Strength's permanent and current scores");
    check(creature->current_abilities.charisma == 9, "Charisma's current score");
    check(creature->abilities.exceptional_strength == 50, "Exceptional Strength");
    check(creature->base_thac0 == 10 && creature->base_armor_class == 3,
          "THAC0 and Armor Class are 60 minus the stored byte");
    check(creature->race == 7 && creature->character_class == 2, "Race and class");
    check(creature->max_hit_points == 70 && creature->current.hit_points_raw == 66, "Hit points");
    check(creature->spell_book.size() == 100 && creature->spell_book[0] == 47, "Known spells");
    check(creature->memorized_spells.size() == 84, "84 memorized spells");
    check(creature->base_movement == 12 && creature->current.movement == 9, "Movement");
    check(creature->wealth[0] == 1234, "Coins");
    check(creature->levels.fighter == 10, "Fighter level");
    const auto &attack = creature->base_attacks[0];
    check(attack.attacks_per_two_rounds == 4 && attack.damage.count == 2 &&
              attack.damage.sides == 8 && attack.damage.modifier == -1,
          "The first attack's rate and dice");
    check(creature->experience == 1872 && creature->base_xp_award == 1950,
          "Experience and its award");
    check(creature->item_count == 3 && creature->hostile == 1, "Items and hostility");
    check(opengold::por::decode_character(bytes, GoldBoxGame::curse_of_the_azure_bonds).has_value(),
          "decode_character reads each game's layout");

    bytes.push_back(0);
    check(!opengold::curse::decode_creature(bytes), "Pool of Radiance's sizes are not Curse's");
    bytes.pop_back();
    bytes[0] = 16;
    check(!opengold::curse::decode_creature(bytes), "A name holds at most 15 letters");
}

// Curse runs its scripts at 8000h, gives ECL CLOCK a second operand and adds three commands.
void curse_scripts_run_in_their_own_bank()
{
    std::vector<std::uint8_t> record{0, 0};
    constexpr std::uint16_t body = 0x8000 + 5 * 4;
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x02, body & 0xff, body >> 8});
    }
    record.insert(record.end(), {0x40, 0x00, 0x05});             // DESTROY ITEMS 5
    record.insert(record.end(), {0x34, 0x00, 0x01, 0x00, 0x02}); // ECL CLOCK 1, 2
    record.push_back(0x00);                                      // EXIT

    const auto program =
        opengold::por::EclProgram::decode(record, "test", GoldBoxGame::curse_of_the_azure_bonds);
    check(program.entries()[0] == body, "The entries point into the body at 8014h");
    const auto destroy = program.instruction(body);
    check(opengold::por::ecl_opcode(destroy.opcode, program.game()).name == "DESTROY ITEMS" &&
              destroy.operands.size() == 1 && destroy.operands[0].value == 5,
          "DESTROY ITEMS takes one operand");
    const auto clock = program.instruction(destroy.next);
    check(clock.operands.size() == 2, "Curse's ECL CLOCK takes two operands");
    check(opengold::por::walk_reachable(program).errors.empty(), "The script walks cleanly");
    check(opengold::por::ecl_opcode(0x34).operands == 1,
          "Pool of Radiance's ECL CLOCK keeps one operand");
    rejects(
        [&]
        {
            static_cast<void>(opengold::por::EclProgram::decode(record, "test"));
        },
        "Pool of Radiance's scripts start at 9900h");
    rejects(
        []
        {
            static_cast<void>(opengold::por::ecl_opcode(0x40));
        },
        "Pool of Radiance has no command 40h");
}

// The script machine runs a Curse script in Curse's bank: its entries, its
// commands and a write into its own code.
void the_machine_runs_curse_scripts()
{
    std::vector<std::uint8_t> record{0, 0};
    constexpr std::uint16_t body = 0x8000 + 5 * 4;
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x01, body & 0xff, body >> 8});
    }
    constexpr std::uint16_t own_byte = body + 12;                      // The EXIT below.
    record.insert(record.end(), {0x09, 0x00, 0x07, 0x01, 0x00, 0x40}); // SAVE 7 at 4000h
    record.insert(record.end(), {0x09, 0x00, 0x00, 0x01, own_byte & 0xff, own_byte >> 8});
    record.push_back(0x00); // EXIT, overwritten with EXIT
    opengold::por::EclMachine machine(
        std::make_shared<const opengold::por::EclProgram>(opengold::por::EclProgram::decode(
            record, "CURSE:1", GoldBoxGame::curse_of_the_azure_bonds)));
    machine.bind_variable(opengold::por::EclAddress{0x4000}, 0);
    check(machine.start(0), "A Curse script starts at its first entry");
    const auto result = machine.run();
    check(result.state == opengold::por::EclState::completed,
          "It runs to its EXIT: " + result.diagnostic);
    check(machine.variable(opengold::por::EclAddress{0x4000}) == 7, "SAVE wrote the variable");
}

// Every cell Curse's source names lies where moving Pool of Radiance's zones puts it.
void curse_script_memory_is_pool_of_radiance_moved()
{
    using opengold::por::EclAddress;
    using opengold::por::game_address;
    constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const std::vector<std::pair<std::uint16_t, std::uint16_t>> cells{
        {0x49C5, 0x4BC5}, // The 3D map loaded.
        {0x49C9, 0x4BC9}, // The clock's hour.
        {0x49E6, 0x4BE6}, // A 3D map is loaded.
        {0x49F2, 0x4BF2}, // The script before the last NEW ECL.
        {0x6DC1, 0x7EC1}, // The monsters' distance.
        {0x6DC6, 0x7EC6}, // Morale.
        {0x6DC9, 0x7EC9}, // The party's next step blocked.
        {0x6DCA, 0x7ECA}, // Search flags.
        {0x6DCB, 0x7ECB}, // Surprise.
        {0x6DD2, 0x7ED2}, // Rest checks.
        {0x6DD5, 0x7ED5}, // Leaving the map.
        {0x6DE1, 0x7EE1}, // The portrait's head.
        {0x6DE6, 0x7EE6}, // The mirror duel.
        {0x6E33, 0x7F33}, // An ally attacked.
        {0x6E3E, 0x7F3E}, // The party's size.
        {0x6E70, 0x7F70}, // Monsters' to-hit.
        {0x6E72, 0x7F72}, // The party's movement in combat.
        {0x9700, 0x7A00}, // The strings.
        {0xC04B, 0xC04B}, // The party's position stays.
    };
    for (const auto &[pool, expected] : cells)
    {
        check(game_address(EclAddress{pool}, curse).location == expected,
              "Cell " + std::to_string(pool) + " in Curse");
        check(game_address(EclAddress{pool}, GoldBoxGame::pool_of_radiance).location == pool,
              "Pool of Radiance keeps its own cells");
    }
    rejects(
        []
        {
            static_cast<void>(game_address(opengold::por::ecl_character_hit_points,
                                           GoldBoxGame::curse_of_the_azure_bonds));
        },
        "The character record's cells are not moved: its layout changed");
}

// A script's CALL runs an engine routine by the address its game gives it:
// Curse's view redraw, 2E10h, means nothing to Pool of Radiance.
void engine_routines_are_the_games()
{
    using namespace opengold::por;
    const auto run_call = [](GoldBoxGame game)
    {
        const auto origin = ecl_memory(game).origin;
        const auto body = static_cast<std::uint16_t>(origin + 5 * 4);
        std::vector<std::uint8_t> record{0, 0};
        for (int entry = 0; entry < 5; ++entry)
        {
            record.insert(record.end(), {0x01, 0x01, static_cast<std::uint8_t>(body & 0xff),
                                         static_cast<std::uint8_t>(body >> 8)});
        }
        record.insert(record.end(), {0x2D, 0x02, 0x10, 0x2E, 0x00}); // CALL 2E10h, EXIT
        EclMachine machine(
            std::make_shared<const EclProgram>(EclProgram::decode(record, "CALL", game)));
        machine.enable_host(0x2D);
        check(machine.start(0), "The script starts");
        return machine.run();
    };
    const auto curse = run_call(GoldBoxGame::curse_of_the_azure_bonds);
    check(curse.state == EclState::waiting && curse.request.has_value(),
          "Curse's redraw reaches the host");
    check(run_call(GoldBoxGame::pool_of_radiance).state == EclState::completed,
          "In Pool of Radiance 2E10h is no routine, and the CALL does nothing");
    const auto routines = ecl_engine_routines(GoldBoxGame::curse_of_the_azure_bonds);
    check(routines.redraw_view == 0x2E10 && routines.sound == 0xB200 &&
              routines.move_forward ==
                  ecl_engine_routines(GoldBoxGame::pool_of_radiance).move_forward,
          "Curse's routines: its own redraw and sound, Pool of Radiance's step forward");
}

void the_font_has_eight_bytes_a_glyph()
{
    std::vector<std::uint8_t> font(64 * 8);
    font[8] = 0x0f; // Glyph 1's top row.
    const auto glyphs = opengold::decode_game_font(font);
    check(glyphs && glyphs->size() == 64 && (*glyphs)[1][0] == 0x0f, "64 glyphs, in order");
    font.pop_back();
    check(!opengold::decode_game_font(font), "Whole glyphs only");
    check(!opengold::decode_game_font(std::vector<std::uint8_t>(63 * 8)),
          "Upper-case letters need the first 64 glyphs");
}

// The game's own loaders read Curse's files, when the developer has them
// (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
void the_loaders_read_curse()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "CURSE" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::curse_of_the_azure_bonds)
    {
        std::cout << "Curse's files are not here; its loaders are not checked.\n";
        return;
    }
    constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
    const auto creatures = opengold::por::CreatureCatalog::load(folder, curse);
    check(creatures.all().size() == 81, "Curse has 81 creature records");
    bool found_ettin = false;
    for (const auto &[id, creature] : creatures.all())
    {
        if (creature.stored.name == "ETTIN")
        {
            found_ettin =
                creature.stored.base_armor_class == 3 && creature.stored.max_hit_points == 70;
        }
    }
    check(found_ettin, "The ettin has Armor Class 3 and 70 hit points");
    const auto items = opengold::por::MagicItemCatalog::load(folder, curse);
    check(!items.lists.empty(), "Curse's magic items list");
    const auto art = opengold::por::CharacterArt::load(folder, curse);
    check(art.heads().empty() && art.combat_heads().size() == 56,
          "Curse's player characters have combat icons but no portraits to choose");
    const auto sounds = opengold::por::SoundBank::load(folder, curse);
    check(sounds.at(10).audible, "Curse's footstep sounds");
}

// Curse's opening, as the game plays it: its first script wakes the party in a
// small room, shows picture 10, then leaves it on its first map, square (7,13)
// facing east, with the view drawn (docs/GAMES.md, phase 3 step 1).
void curse_opening_reaches_the_first_map()
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "CURSE" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::curse_of_the_azure_bonds)
    {
        std::cout << "Curse's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds);
    const auto settle = [&session]
    {
        for (int step = 0; step < 50 && session.snapshot().phase == TourPhase::running; ++step)
        {
            session.advance(1.0);
        }
    };
    const auto diagnostics = [&session]
    {
        return session.snapshot().diagnostic +
               (session.script_diagnostics().empty() ? "" : session.script_diagnostics().front());
    };
    settle();
    check(session.snapshot().phase == TourPhase::awaiting_continue &&
              session.snapshot().dialogue.find("YOU AWAKEN") != std::string::npos,
          "The opening wakes the party: " + diagnostics());
    check(session.snapshot().area_id == 1, "Its first map is loaded");
    check(session.choose(session.snapshot().continue_ticket, 0), "The party goes on");
    settle();
    check(session.picture().has_value(), "Picture 10 shows: " + diagnostics());
    check(session.choose(session.snapshot().continue_ticket, 0), "The party goes on again");
    settle();
    const auto &snapshot = session.snapshot();
    check(snapshot.phase == TourPhase::completed, "The opening ends: " + diagnostics());
    check(snapshot.pose.x == 7 && snapshot.pose.y == 13 &&
              snapshot.pose.facing == opengold::por::MapDirection::east,
          "The party stands on square (7,13) facing east");
    check(!session.picture().has_value(), "The redraw puts the view back over the picture");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto colours = [](const opengold::Image &image)
    {
        std::set<std::uint32_t> seen;
        for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
        {
            seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
        }
        return seen.size();
    };
    const auto view = session.observe_view();
    check(colours(view) > 2, "The 3D view shows the room's walls");
    check(session.explore(opengold::por::ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
    // Through the room's west door the innkeeper welcomes the party to Tilverton.
    check(session.explore(opengold::por::ExplorationCommand::turn_left), "The party faces west");
    check(session.explore(opengold::por::ExplorationCommand::forward), "The party steps");
    settle();
    check(session.snapshot().pose.x == 6 && session.snapshot().pose.y == 13 &&
              session.snapshot().dialogue.find("TILVERTON") != std::string::npos,
          "The innkeeper speaks: " + diagnostics());
}

// A save names its game; a save made before format 48 is Pool of Radiance's.
void a_save_names_its_game()
{
    const auto rules_file =
        std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules";
    opengold::CampaignParty party(opengold::srd5::load(rules_file));
    const auto module = opengold::srd5::load(rules_file);
    const auto creation_rules = opengold::srd5::character_rules();
    const auto &creation = *creation_rules;
    const auto curse_save =
        opengold::encode_campaign(party, nullptr, "files", GoldBoxGame::curse_of_the_azure_bonds);
    check(curse_save.starts_with("OPENGOLD-CAMPAIGN 48\n"), "Saves are format 48");
    check(opengold::decode_campaign(curse_save, creation, *module, "files", nullptr).game ==
              GoldBoxGame::curse_of_the_azure_bonds,
          "The save keeps its game");
    const auto older = opengold::encode_campaign_in_format(party, nullptr, "files", 47);
    check(opengold::decode_campaign(older, creation, *module, "files", nullptr).game ==
              GoldBoxGame::pool_of_radiance,
          "A format 47 save is Pool of Radiance's");
    rejects(
        [&]
        {
            static_cast<void>(opengold::encode_campaign_in_format(
                party, nullptr, "files", 47, GoldBoxGame::curse_of_the_azure_bonds));
        },
        "Format 47 cannot record another game");
}

} // namespace

int main()
{
    try
    {
        each_game_is_told_apart_by_its_files();
        curse_creatures_are_read_from_their_layout();
        curse_scripts_run_in_their_own_bank();
        the_machine_runs_curse_scripts();
        curse_script_memory_is_pool_of_radiance_moved();
        engine_routines_are_the_games();
        the_loaders_read_curse();
        a_save_names_its_game();
        curse_opening_reaches_the_first_map();
        the_font_has_eight_bytes_a_glyph();
        std::cout << "Gold Box games passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
