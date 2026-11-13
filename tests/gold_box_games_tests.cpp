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
#include "opengold/coin_purse.h"
#include "opengold/original_combat.h"
#include "opengold/original.h"
#include "opengold/original_spells.h"
#include "opengold/character_creator.h"
#include "opengold/combat_demo.h"
#include "tour_access.h"
#include <map>
#include "opengold/game_census.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/pool_radiance.h"
#include "opengold/sound_bank.h"
#include "opengold/srd5.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
    // The current character's cells keep their offsets in zone 1 (eclmenu.pas SpecialReadZ1).
    check(game_address(opengold::por::ecl_character_hit_points, curse).location == 0x7D19,
          "The character's hit points move with zone 1");
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

// Curse's step 2 (docs/GAMES.md): every square of the map the party stands on,
// each of its events run to its end, answering the first choice; prints what
// the scripts still need. Run with --curse-walk.
// True when every event it reached ran: no script needs a command or form
// the session lacks.
// A square of a story's route: stood on, facing, its event played with these
// answers (a choice whose label holds the answer, or input to type), in turn.
struct RouteBeat
{
    unsigned map, x, y;
    opengold::por::MapDirection facing;
    std::vector<std::string> answers;
    // Rest until every member is whole first, as a player would before a
    // known fight.
    bool rest_first = false;
    // Walk into the map, its script taking over, as its way in from the last
    // map does, instead of looking at a square of the map the party is on.
    bool enter_map = false;
    // Answer the menu the last beat stopped at (the wilderness's), where the
    // party stands, instead of going anywhere.
    bool waiting = false;
    // A script cell that must hold this value after the beat.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> expect_cell;
    // Experience and gold the test gives each member before the beat, as the
    // Pool of Radiance expedition does before its harder areas, for a party
    // as strong as one brought from Pool of Radiance.
    unsigned test_experience = 0, test_gold = 0;
    // Play the beat again (up to 40 times), a square further east each time
    // along the row, until this cell is at least the value: Hap's patrols
    // meet the party as it goes about.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> repeat_until_at_least;
    // Before the beat and between those rounds the test heals the party
    // outside the game's commands (rests where no patrol interrupts it), as
    // the Pool of Radiance expedition does where the game allows no rest.
    bool test_heal = false;
    // Step forward from the square instead of looking at it: a map's ways
    // out run as the party steps (a script's entry 0).
    bool step = false;
};

// Answers that train each of the route's members a level at a hall: its
// name, then the hall's own Train (the first choice).
std::vector<std::string> train_everyone()
{
    return {"YES",      "paladin 1", "paladin 2",    "ranger 3",
            "cleric 4", "cleric 5",  "magic_user 6", "Leave"};
}

// Curse's story, beat by beat, as its cluebook tells it (the route test, plan
// step 7): Tilverton, until the party is exiled to the wilderness.
const std::vector<RouteBeat> &curse_route()
{
    using D = opengold::por::MapDirection;
    static const std::vector<RouteBeat> beats{
        {1, 12, 7, D::north, {}},      // The armor shop.
        {1, 6, 5, D::north, {}},       // Julynn the sage.
        {1, 1, 0, D::north, {}},       // The guards turn the party back once,
        {1, 1, 1, D::north, {}, true}, // twice, and the king's carriage comes:
                                       // the party surrenders, is freed from jail
                                       // and led to the Thieves' Guild, rested.
        {1, 9, 3, D::north, {}, true}, // The guildmaster's news, rested; the Fire Knives burst in.
        {3, 0, 0, D::south, {}, true, true}, // Out through the guild's back to the sewers.
        // The guild's secret training hall, six levels over (an award stops
        // short of the level after next).
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {3, 14, 6, D::north, train_everyone(), false, false, false, std::nullopt, 60000, 7000},
        {4, 0, 0, D::south, {}, true, true}, // Through the sewers to the Fire Knives' hideout.
        {4, 3, 13, D::north, {"stop:JOURNEY"}, true},             // The Fire Knives' leader and his
                                                                  // band; freed of the first bond,
                                                                  // the party is exiled to the
                                                                  // wilderness's menu.
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true}, // Rested in camp.
        // Shadowdale is the journey's destination (cell 0x4C9C), though the
        // sage's flag is still set from Tilverton.
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "SHADOWDALE", "stop:TRAIL"},
         false,
         false,
         true,
         std::pair<std::uint16_t, std::uint16_t>{0x4C9C, 1}},
        {0, 0, 0, D::north, {"TRAIL", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        // On to Hap, resting at each stop: by boat on the Ashaben (kept clear of
        // pirates), then by the menu's first way.
        {0,
         0,
         0,
         D::north,
         {"JOURNEY", "ASHABENFORD", "BY BOAT", "stop:JOURNEY"},
         false,
         false,
         true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "STANDING STONE", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "ESSEMBRA", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"JOURNEY", "HAP", "stop:JOURNEY"}, false, false, true},
        {0, 0, 0, D::north, {"stop:JOURNEY"}, true, false, true},
        {0, 0, 0, D::north, {"ENTER CITY"}, false, false, true}, // Hap.
        // Hap's dark elf patrols: once four have fallen (cell 0x4C47 counts
        // them) the barn holds two of each kind, not six. A camp in Hap draws
        // them on the party, so it does not rest there.
        {50,
         0,
         0,
         D::north,
         {"Fight"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::pair<std::uint16_t, std::uint16_t>{0x4C47, 4},
         true},
        {50, 8, 4, D::north, {}}, // The efreeti and the dark elves in the barn.
        // Out through Hap's gate, following the efreeti's map to the caves.
        {50,
         3,
         0,
         D::north,
         {"YES", "CAVES"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // Crimdrac the dracolich lets the party live and takes it up to the
        // Wizard's Tower; on its roof the party, freed of the bond, parleys
        // with the dragons and is nice to them, as the cluebook advises.
        {50,
         7,
         12,
         D::north,
         {"PARLAY WITH THE DRAGONS", "NICE"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         true},
        // Dracandros in the tower's courtyard; among his treasures, the Helm
        // of Dragons (cell 0x4C59): the second bond is gone.
        {51,
         4,
         13,
         D::north,
         {},
         false,
         false,
         false,
         std::pair<std::uint16_t, std::uint16_t>{0x4C59, 1},
         0,
         0,
         std::nullopt,
         true},
        // Out of the tower by its secret passage, to Hap's village, and the
        // dead raised at its Temple of Sune.
        {51,
         7,
         15,
         D::east,
         {"WILDERNESS", "VILLAGE"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
        // (Stepping in from the street, as a building is entered only from
        // outside: each sets cell 0x4C02 until the party is out again.)
        {50,
         3,
         8,
         D::west,
         {"YES", "Raise Dead", "ranger 3", "Pay", "Raise Dead", "cleric 5", "Pay", "Raise Dead",
          "magic_user 6", "Pay", "Leave"},
         false,
         false,
         false,
         std::nullopt,
         0,
         0,
         std::nullopt,
         false,
         true},
    };
    return beats;
}

bool curse_walk_census(bool route)
{
    using opengold::por::TourPhase;
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "CURSE" : std::filesystem::path();
    if (!games || opengold::identify_game(folder) != GoldBoxGame::curse_of_the_azure_bonds)
    {
        std::cout << "Curse's files are not here.\n";
        return true;
    }
    auto session = opengold::load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds);
    // A party under the original rules, fighting Curse's own creatures.
    opengold::original::Content content;
    content.game = GoldBoxGame::curse_of_the_azure_bonds;
    const auto catalog =
        opengold::por::CreatureCatalog::load(folder, GoldBoxGame::curse_of_the_azure_bonds);
    for (const auto &[id, creature] : catalog.all())
    {
        std::vector<opengold::original::CarriedItem> carried;
        for (const auto &item : creature.equipment)
        {
            carried.push_back({item.stored, item.base});
        }
        std::vector<opengold::por::EffectRecord> effects;
        for (const auto &effect : creature.effects)
        {
            effects.push_back(effect.stored);
        }
        content.bestiary.emplace(
            id.key(), opengold::original::combatant_from_record(creature.stored, carried, effects));
    }
    std::ifstream items(folder / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> item_bytes{std::istreambuf_iterator<char>(items), {}};
    content.item_types = *opengold::por::decode_item_templates(item_bytes);
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::curse_of_the_azure_bonds);
    std::shared_ptr<opengold::CampaignParty> party;
    // The walk's party is six fighters; the route's the cluebook's: two
    // paladins, a ranger, two clerics and a magic-user.
    const std::array<std::string, 6> walk_classes{"fighter", "fighter", "fighter",
                                                  "fighter", "fighter", "fighter"};
    const std::array<std::string, 6> route_classes{"paladin", "paladin", "ranger",
                                                   "cleric",  "cleric",  "magic_user"};
    // Each day's spells: the strongest a caster knows at each of its levels.
    const auto memorize = [&]
    {
        constexpr std::array<std::string_view, 15> favorites{
            "cone_of_cold",      "ice_storm",      "confusion",     "hold_monster",
            "fireball",          "stinking_cloud", "magic_missile", "sleep",
            "lightning_bolt",    "flame_strike",   "hold_person",   "cure_serious_wounds",
            "cure_light_wounds", "prayer",         "bless"};
        if (!party->state().spell_rest)
        {
            return;
        }
        // Those the last Long Rest left choosing; choose_spells takes each off.
        for (const auto id : std::vector(party->state().spell_rest->members))
        {
            const auto options = party->spell_choice_options(id);
            const auto &numbers = *party->member(id).character.sheet().original;
            if (!options.may_prepare)
            {
                party->keep_rest_spells(id);
                continue;
            }
            std::vector<std::string> prepared;
            for (const auto &[prefix, slots] :
                 {std::pair{std::string("cleric:"), numbers.cleric_slots},
                  std::pair{std::string("magic_user:"), numbers.magic_user_slots}})
            {
                for (int level = 1; level <= 5; ++level)
                {
                    std::vector<std::string> candidates;
                    for (const auto &choice : options.preparation)
                    {
                        const auto number = opengold::original::spell_number(choice.id);
                        if (choice.id.starts_with(prefix) && number &&
                            opengold::original::spell(*number)->level == level)
                        {
                            candidates.push_back(choice.id);
                        }
                    }
                    const auto rank = [&](const std::string &id)
                    {
                        const auto found =
                            std::ranges::find_if(favorites,
                                                 [&](std::string_view name)
                                                 {
                                                     return id.find(name) != std::string::npos;
                                                 });
                        return static_cast<int>(found - favorites.begin());
                    };
                    std::ranges::stable_sort(candidates, {}, rank);
                    for (int n = 0; n < slots[static_cast<std::size_t>(level - 1)] &&
                                    n < static_cast<int>(candidates.size());
                         ++n)
                    {
                        prepared.push_back(candidates[static_cast<std::size_t>(n)]);
                    }
                }
            }
            party->choose_spells(id, {{}, prepared});
        }
    };
    const auto new_party = [&]
    {
        party = std::make_shared<opengold::CampaignParty>(opengold::original::rules_module(content),
                                                          creation);
        const auto &classes = route ? route_classes : walk_classes;
        for (unsigned n = 0; n < 6; ++n)
        {
            const auto &klass = classes[n];
            opengold::CharacterCreator creator(creation, 90 + n);
            creator.select(opengold::rules::CreationField::race, "human");
            creator.select(opengold::rules::CreationField::character_class, klass);
            creator.name(klass + " " + std::to_string(n + 1));
            for (const bool roll : {false, false, false, true, false, false})
            {
                if (roll)
                {
                    creator.roll();
                }
                creator.next();
            }
            const auto id = party->add_pc(creator.create_character());
            // A long sword, plate mail and a shield of Curse's own item types; a
            // cleric's mace; a magic-user's dagger.
            const std::vector<std::uint8_t> gear_types =
                klass == "magic_user" ? std::vector<std::uint8_t>{8}
                : klass == "cleric"   ? std::vector<std::uint8_t>{23, 55, 59}
                                      : std::vector<std::uint8_t>{36, 55, 59};
            for (const std::uint8_t type : gear_types)
            {
                std::array<std::uint8_t, 63> raw{};
                raw[46] = type;
                // The route's party carries the magic arms of one brought from
                // Pool of Radiance: weapons +2, armor and shields +1 (the
                // record's magic bonus, byte 50).
                if (route)
                {
                    raw[50] = type == 55 || type == 59 ? 1 : 2;
                }
                opengold::por::Equipment gear;
                gear.stored = opengold::por::decode_items(raw)->front();
                party->purchase(id, gear);
                party->equip(id, party->member(id).character.inventory().items().back().id);
            }
        }
        party->add_coins(opengold::Coin::gold, 5000);
        if (route && party->rest())
        {
            memorize();
        }
    };
    new_party();
    session.campaign_party(party);
    std::map<std::string, std::vector<std::string>> needs;
    std::map<std::string, unsigned> phases;
    const bool trace = route || std::getenv("OPENGOLD_CURSE_TRACE");
    // One play of the pending fight with these dice; the party ends as it left it.
    const auto fight_once = [&](std::uint64_t seed)
    {
        opengold::CombatDemo combat(opengold::original::rules_module(content));
        combat.campaign_party(party);
        combat.encounter(*session.pending_encounter(), seed);
        if (trace)
        {
            std::cout << "  fight:";
            for (const auto &unit : combat.combat().snapshot().combatants)
            {
                std::cout << " " << unit.name << " " << unit.hit_points << "/"
                          << unit.max_hit_points;
            }
            std::cout << "\n";
        }
        // Each turn chosen as the game's combat view chooses it: the party all
        // on Quick with Quick magic on, everyone else at the default difficulty.
        for (int command = 0; command < 3000 && combat.combat().snapshot().outcome ==
                                                    opengold::rules::Outcome::ongoing;
             ++command)
        {
            const auto &session = combat.combat();
            const auto state = session.snapshot();
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            const bool member = actor != state.combatants.end() &&
                                actor->side == opengold::rules::Side::party &&
                                !opengold::is_ally(state.actor);
            const auto chosen =
                member
                    ? opengold::choose_quick_command(session, true, opengold::Searching::everyone)
                    : opengold::choose_demo_command(session, opengold::Searching::everyone,
                                                    opengold::default_difficulty);
            if (!combat.submit(chosen))
            {
                ++phases["fight stopped at " + chosen.verb];
                break;
            }
        }
        return combat.combat().snapshot();
    };
    // The pending fight. The route plays it with several dice from the party
    // as it stood, as a player reloads a save, and keeps the best play: a win,
    // then the fewest dead, then the fewest Hit Points lost. The walk plays it
    // once.
    const auto fight = [&]
    {
        const auto before = party->state();
        const auto score = [](const opengold::rules::Snapshot &result)
        {
            int lost = result.outcome == opengold::rules::Outcome::victory ? 0 : 1'000'000;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == opengold::rules::Side::party)
                {
                    lost +=
                        (unit.dead ? 1000 : 0) + unit.max_hit_points - std::max(unit.hit_points, 0);
                }
            }
            return lost;
        };
        std::uint64_t seed = 7, best_seed = seed;
        auto result = fight_once(seed);
        auto best = score(result);
        while (route && best > 0 && ++seed < 47)
        {
            party->restore(before);
            result = fight_once(seed);
            if (score(result) < best)
            {
                best = score(result);
                best_seed = seed;
            }
        }
        if (seed != best_seed && route)
        {
            party->restore(before);
            result = fight_once(best_seed);
        }
        ++phases["fights ended " + std::to_string(static_cast<int>(result.outcome))];
        return session.resolve_combat(result);
    };
    std::size_t diagnosed = 0;
    std::vector<std::string> answers;
    // The survey declines every offer (the last choice), so it does not walk
    // into a trap or surrender.
    bool declining = false;
    const auto play = [&](const std::string &where)
    {
        const auto attribute = [&]
        {
            const auto &found = session.script_diagnostics();
            for (; diagnosed < found.size(); ++diagnosed)
            {
                needs[found[diagnosed]].push_back(where);
            }
        };
        // A menu that comes back (a temple's services) is left by its last choice.
        std::map<std::string, unsigned> shown;
        for (int step = 0; step < 400; ++step)
        {
            const auto &s = session.snapshot();
            if (!s.diagnostic.empty())
            {
                needs[s.diagnostic].push_back(where);
            }
            switch (s.phase)
            {
            case TourPhase::running:
                session.advance(1.0);
                continue;
            case TourPhase::awaiting_continue:
            {
                // OPENGOLD_CURSE_TRACE prints each square's dialogue, to plan a route by.
                if (trace && shown[s.dialogue] == 0)
                {
                    std::cout << where << ": " << s.dialogue.substr(0, 160) << " [";
                    for (const auto &choice : s.choices)
                    {
                        std::cout << choice << "|";
                    }
                    std::cout << "]\n";
                }
                // An answer "stop:TEXT" ends the beat at the menu offering TEXT.
                if (!answers.empty() && answers.front().starts_with("stop:") &&
                    std::ranges::any_of(s.choices,
                                        [&](const std::string &choice)
                                        {
                                            return choice.find(answers.front().substr(5)) !=
                                                   std::string::npos;
                                        }))
                {
                    answers.erase(answers.begin());
                    return;
                }
                std::size_t pick = (++shown[s.dialogue] > 2 || declining) && !s.choices.empty()
                                       ? s.choices.size() - 1
                                       : 0;
                if (!answers.empty())
                {
                    const auto found = std::ranges::find_if(
                        s.choices,
                        [&](const std::string &choice)
                        {
                            return choice.find(answers.front()) != std::string::npos;
                        });
                    if (found != s.choices.end())
                    {
                        pick = static_cast<std::size_t>(found - s.choices.begin());
                        answers.erase(answers.begin());
                    }
                }
                if (!session.choose(s.continue_ticket, pick) &&
                    !session.continue_dialogue(s.continue_ticket))
                {
                    ++phases["stuck at a choice"];
                    attribute();
                    return;
                }
                continue;
            }
            case TourPhase::awaiting_input:
            {
                std::string typed = "0";
                if (!answers.empty() && answers.front().starts_with("type:"))
                {
                    typed = answers.front().substr(5);
                    answers.erase(answers.begin());
                }
                if (trace)
                {
                    std::cout << where << ": " << s.dialogue.substr(0, 160) << " <" << typed
                              << ">\n";
                }
                (void)session.input(s.continue_ticket, typed);
                continue;
            }
            case TourPhase::shopping:
                if (session.shop_stock().empty())
                {
                    needs["A shop with nothing to sell"].push_back(where);
                }
                ++phases["shops with goods"];
                (void)session.leave_shop(s.continue_ticket);
                continue;
            case TourPhase::treasure:
                (void)session.leave_treasure(s.continue_ticket);
                continue;
            case TourPhase::combat:
                if (session.monster_picture())
                {
                    (void)session.start_encounter();
                    continue;
                }
                if (!session.pending_encounter() || !fight())
                {
                    ++phases["fight not resolved"];
                    attribute();
                    return;
                }
                continue;
            default:
                ++phases[std::to_string(static_cast<int>(s.phase))];
                attribute();
                return;
            }
        }
        const auto &s = session.snapshot();
        std::cout << "Too long at " << where << ", phase " << static_cast<int>(s.phase) << ": "
                  << s.dialogue << "\n";
        attribute();
        ++phases["too long"];
    };
    // A new game with a new party, standing on `map` as its script's entry has it.
    const auto restart = [&](unsigned map)
    {
        session = opengold::load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds);
        new_party();
        session.campaign_party(party);
        diagnosed = 0;
        play("opening");
        if (session.snapshot().area_id == map)
        {
            return true;
        }
        const auto where = "map " + std::to_string(map) + " entry";
        try
        {
            opengold::test::TourAccess::enter(session, map,
                                              {8, 8, opengold::por::MapDirection::north});
            play(where);
        }
        catch (const std::exception &error)
        {
            needs[error.what()].push_back(where);
            return false;
        }
        return session.snapshot().area_id == map && session.can_leave();
    };
    // Every square of the map, its event run where it has one; an event that
    // ends the game or takes the party away is played from a new game.
    const auto walk = [&](unsigned area)
    {
        for (unsigned y = 0; y < 16; ++y)
        {
            for (unsigned x = 0; x < 16; ++x)
            {
                const auto where = "map " + std::to_string(area) + " " + std::to_string(x) + "," +
                                   std::to_string(y);
                if ((session.snapshot().area_id != area || !session.can_leave()) && !restart(area))
                {
                    ++phases["map not reentered"];
                    return;
                }
                opengold::test::TourAccess::place(session,
                                                  {x, y, opengold::por::MapDirection::north});
                try
                {
                    if (session.explore(opengold::por::ExplorationCommand::look))
                    {
                        play(where);
                    }
                }
                catch (const std::exception &error)
                {
                    needs[error.what()].push_back(where);
                    return;
                }
            }
        }
    };
    if (route)
    {
        // Days of rest in camp until every member is whole, as a player rests
        // (or a script's PROGRAM 9 offers the camp).
        const auto party_whole = [&]
        {
            return std::ranges::all_of(
                party->state().slots,
                [&](opengold::MemberId id)
                {
                    return !id || party->member(id).vitals.dead ||
                           party->member(id).vitals.hit_points >=
                               party->member(id).character.sheet().hit_points;
                });
        };
        const auto rest_until_healed = [&]
        {
            for (int day = 0; day < 90 && session.can_leave(); ++day)
            {
                if (party_whole() || !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                // A camp may be interrupted, or the map's script run at it.
                play("camp");
                // The next day's camp waits on each caster's spells.
                memorize();
            }
        };
        // Days in camp from the wilderness's menu until every member is whole:
        // CAMP opens the camp, and the menu returns after each night or fight.
        const auto rest_in_wilderness = [&]
        {
            // Leaving the camp without resting brings the menu back.
            answers = {"CAMP"};
            play("wilderness camp");
            session.decline_camp();
            answers = {"stop:JOURNEY"};
            play("wilderness camp");
            if (session.snapshot().phase != TourPhase::awaiting_continue)
            {
                needs["The wilderness's menu does not return after a declined camp"].push_back(
                    "wilderness camp");
            }
            for (int day = 0; day < 90 && !party_whole(); ++day)
            {
                answers = {"CAMP"};
                play("wilderness camp");
                if (!session.snapshot().camp_offered ||
                    !session.camp(opengold::RestKind::long_rest))
                {
                    break;
                }
                answers = {"stop:JOURNEY"};
                play("wilderness camp");
                memorize();
            }
        };
        play("opening");
        std::size_t beat_number = 0;
        // A beat that throws ends the route, named with the beat it was in.
        std::string current_beat = "the route's start";
        try
        {
            for (const auto &beat : curse_route())
            {
                ++beat_number;
                const auto where = "beat " + std::to_string(beat.map) + " " +
                                   std::to_string(beat.x) + "," + std::to_string(beat.y);
                current_beat = where + " (beat " + std::to_string(beat_number) + ")";
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                if (beat.rest_first)
                {
                    beat.waiting ? rest_in_wilderness() : rest_until_healed();
                }
                if (session.snapshot().phase == TourPhase::defeated)
                {
                    break;
                }
                for (int day = 0; beat.test_heal && day < 90 && !party_whole() && party->rest();
                     ++day)
                {
                    memorize();
                }
                if (beat.test_experience)
                {
                    for (const auto id : party->state().slots)
                    {
                        if (id)
                        {
                            party->award_experience(beat.test_experience,
                                                    "test:curse-route-" +
                                                        std::to_string(beat_number) + "-" +
                                                        std::to_string(id),
                                                    id);
                        }
                    }
                    party->add_coins(opengold::Coin::gold, beat.test_gold);
                }
                const auto cell = [&](std::uint16_t address)
                {
                    return opengold::test::TourAccess::machine(session).variable(
                        opengold::por::EclAddress{address});
                };
                for (int round = 0; round < 40; ++round)
                {
                    answers = beat.answers;
                    if (beat.waiting)
                    {
                        play(where);
                    }
                    else if (beat.enter_map)
                    {
                        opengold::test::TourAccess::enter(session, beat.map,
                                                          {beat.x, beat.y, beat.facing});
                        play(where);
                    }
                    else
                    {
                        const unsigned x = (beat.x + static_cast<unsigned>(round)) % 16;
                        opengold::test::TourAccess::place(session, {x, beat.y, beat.facing});
                        if (session.explore(beat.step ? opengold::por::ExplorationCommand::forward
                                                      : opengold::por::ExplorationCommand::look))
                        {
                            play(where);
                        }
                    }
                    const auto &until = beat.repeat_until_at_least;
                    if (!until || cell(until->first) >= until->second ||
                        session.snapshot().phase == TourPhase::defeated)
                    {
                        break;
                    }
                    for (int day = 0; beat.test_heal && day < 90 && !party_whole() && party->rest();
                         ++day)
                    {
                        memorize();
                    }
                }
                if (session.snapshot().camp_offered)
                {
                    rest_until_healed();
                }
                if (beat.expect_cell)
                {
                    const auto held = opengold::test::TourAccess::machine(session).variable(
                        opengold::por::EclAddress{beat.expect_cell->first});
                    if (held != beat.expect_cell->second)
                    {
                        needs["Cell " + std::to_string(beat.expect_cell->first) + " holds " +
                              std::to_string(held) + ", not " +
                              std::to_string(beat.expect_cell->second)]
                            .push_back(where);
                    }
                }
                std::cout << "  party after " << where << ":";
                for (const auto id : party->state().slots)
                {
                    if (id)
                    {
                        const auto &member = party->member(id);
                        std::cout << " " << member.vitals.hit_points << "/"
                                  << member.character.sheet().hit_points
                                  << (member.vitals.dead ? " dead" : "");
                        if (const auto &numbers = member.character.sheet().original)
                        {
                            std::cout << " (" << member.experience << " xp, level "
                                      << std::ranges::max(numbers->class_levels) << ")";
                        }
                    }
                }
                std::cout << (session.snapshot().camp_offered ? " (camp offered)" : "") << "\n";
                if (std::getenv("OPENGOLD_CURSE_CELL"))
                {
                    const auto cell =
                        static_cast<std::uint16_t>(std::stoul(std::getenv("OPENGOLD_CURSE_CELL")));
                    std::cout << "  cell " << cell << " = "
                              << opengold::test::TourAccess::machine(session).variable(
                                     opengold::por::EclAddress{cell})
                              << "\n";
                }
            }
        }
        catch (const std::exception &error)
        {
            needs[error.what()].push_back(current_beat);
        }

        for (const auto &[phase, count] : phases)
        {
            std::cout << "  ended " << count << "x in phase " << phase << "\n";
        }
        for (const auto &[message, places] : needs)
        {
            std::cout << "  " << message << " (at " << places.front() << ")\n";
        }
        const auto &end = session.snapshot();
        std::cout << "Route ends on map " << end.area_id << " (script " << end.script_id << ") at "
                  << end.pose.x << "," << end.pose.y << ", phase " << static_cast<int>(end.phase)
                  << "\n";
        // OPENGOLD_CURSE_SURVEY walks the map the route ended on, printing each
        // square's event, to plan the route's next beats by.
        if (std::getenv("OPENGOLD_CURSE_SURVEY") && end.phase != TourPhase::defeated)
        {
            const auto map = end.area_id;
            declining = true;
            for (unsigned y = 0; y < 16 && session.snapshot().area_id == map; ++y)
            {
                for (unsigned x = 0; x < 16 && session.snapshot().area_id == map; ++x)
                {
                    if (!session.can_leave())
                    {
                        break;
                    }
                    opengold::test::TourAccess::place(session,
                                                      {x, y, opengold::por::MapDirection::north});
                    if (session.explore(opengold::por::ExplorationCommand::look))
                    {
                        play("survey " + std::to_string(map) + " " + std::to_string(x) + "," +
                             std::to_string(y));
                    }
                    // A planning aid, so the party rests outside the map, where
                    // no patrol interrupts it.
                    for (int day = 0; day < 90 && !party_whole() && party->rest(); ++day)
                    {
                        memorize();
                    }
                }
            }
            std::cout << "Survey ends on map " << session.snapshot().area_id << ", phase "
                      << static_cast<int>(session.snapshot().phase) << "\n";
        }
        // The route's last beat leaves the party in Hap (script 49), Dracandros
        // dead and its own dead raised at the Temple of Sune.
        constexpr unsigned hap_script = 49;
        const bool all_alive = std::ranges::none_of(party->state().slots,
                                                    [&](opengold::MemberId id)
                                                    {
                                                        return id && party->member(id).vitals.dead;
                                                    });
        return needs.empty() && end.phase != TourPhase::defeated && end.script_id == hap_script &&
               all_alive;
    }
    // OPENGOLD_CURSE_MAP walks that map alone.
    const char *only = std::getenv("OPENGOLD_CURSE_MAP");
    play("opening");
    if (!only)
    {
        walk(session.snapshot().area_id);
    }
    for (const auto &[map, script] : opengold::test::TourAccess::area_scripts(session))
    {
        if ((!only || std::to_string(map) == only) && restart(map))
        {
            walk(map);
        }
    }
    std::cout << "Curse walk:\n";
    for (const auto &[message, places] : needs)
    {
        std::cout << "  " << places.size() << "x " << message << " (first at " << places.front()
                  << ")\n";
    }
    for (const auto &[phase, count] : phases)
    {
        std::cout << "  ended " << count << "x in phase " << phase << "\n";
    }
    return needs.empty();
}

// An item type's property bits, byte 14 of ITEMS, as Pool of Radiance's own
// files hold them (OPENGOLD_GAME_DIR).
void item_types_carry_their_properties()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    if (!game)
    {
        return;
    }
    std::ifstream file(std::filesystem::path(game) / "ITEMS", std::ios::binary);
    const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
    const auto types = opengold::por::decode_item_templates(bytes);
    check(types.has_value() && types->size() > 36, "ITEMS decodes");
    namespace bit = opengold::por::item_property;
    const auto &quarrels = (*types)[28], &long_sword = (*types)[36], &darts = (*types)[9];
    check(quarrels.has(bit::fires_quarrels) && quarrels.has(bit::fires_ammunition) &&
              quarrels.has(bit::dexterity_missile),
          "Quarrels: fired, as ammunition, with Dexterity");
    check(long_sword.has(bit::strength_bonus) && !long_sword.has(bit::thrown),
          "A long sword takes Strength");
    check(darts.has(bit::thrown) && darts.rate_of_fire == 6, "Darts are thrown, three a round");
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
    check(curse_save.starts_with("OPENGOLD-CAMPAIGN 50\n"), "Saves are format 50");
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

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--curse-walk")
        {
            return curse_walk_census(false) ? 0 : 1;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--curse-route")
        {
            return curse_walk_census(true) ? 0 : 1;
        }
        each_game_is_told_apart_by_its_files();
        curse_creatures_are_read_from_their_layout();
        curse_scripts_run_in_their_own_bank();
        the_machine_runs_curse_scripts();
        curse_script_memory_is_pool_of_radiance_moved();
        engine_routines_are_the_games();
        the_loaders_read_curse();
        a_save_names_its_game();
        item_types_carry_their_properties();
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
