// Gateway to the Savage Frontier's campaign (docs/games/gateway.md): its
// opening plays on the real files when OPENGOLD_GOLDBOX_DIR names the folder
// holding GATEWAY.
#include "opengold/campaign_session.h"
#include "opengold/ecl.h"
#include "opengold/ecl_machine.h"
#include "opengold/game_census.h"
#include "opengold/pool_radiance.h"
#include "opengold/overland.h"
#include "opengold/por_sound.h"
#include "test_support.h"
#include "tour_access.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::por::ExplorationCommand;
using opengold::por::TourPhase;
using opengold::test::check;

constexpr auto gateway_game = GoldBoxGame::gateway_to_the_savage_frontier;

std::filesystem::path gateway_folder()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "GATEWAY" : std::filesystem::path();
    return games && opengold::identify_game(folder) == gateway_game ? folder
                                                                    : std::filesystem::path();
}

// A script of Gateway whose five entries run body, which is followed by EXIT.
std::shared_ptr<const opengold::por::EclProgram> script(std::vector<std::uint8_t> body)
{
    const auto start =
        static_cast<std::uint16_t>(opengold::por::ecl_memory(gateway_game).origin + 5 * 4);
    std::vector<std::uint8_t> record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x01, static_cast<std::uint8_t>(start & 0xff),
                                     static_cast<std::uint8_t>(start >> 8)});
    }
    record.insert(record.end(), body.begin(), body.end());
    record.push_back(0x00);
    return std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(record, "prompt", gateway_game));
}

// CALL 880Ah waits for Enter; CALL 880Dh asks Yes or No, stores the answer
// (Yes 0, No 1) in 7F79h and compares it with 1 (Gateway's EclCall).
void the_engine_prompts_are_menus()
{
    using opengold::por::EclAddress;
    using opengold::por::EclMachine;
    using opengold::por::EclState;
    const EclAddress answer{0x7F79}, said_no{0x7F7A};
    // CALL 880Dh; IF =; SAVE 7 to 7F7Ah.
    const std::vector<std::uint8_t> ask{0x2D, 0x02, 0x0C, 0x08, 0x16, 0x09,
                                        0x00, 0x07, 0x01, 0x7A, 0x7F};
    for (const std::size_t choice : {0U, 1U})
    {
        EclMachine machine(script(ask));
        machine.bind_variable(answer, 9);
        machine.bind_variable(said_no, 0);
        check(machine.start(4), "The script starts");
        const auto asked = machine.run();
        check(asked.state == EclState::waiting && asked.request &&
                  asked.request->choices == std::vector<std::string>{"YES", "NO"},
              "880Dh asks Yes or No");
        check(machine.resume(asked.request->id, choice), "The player answers");
        check(machine.run().state == EclState::completed, "The script goes on to its end");
        check(machine.variable(answer) == choice, "The answer is stored in 7F79h");
        check(machine.variable(said_no) == (choice == 1 ? 7 : 0),
              "No compares equal with 1, Yes does not");
    }
    EclMachine machine(script({0x2D, 0x02, 0x09, 0x08}));
    check(machine.start(4), "The script starts");
    const auto waiting = machine.run();
    check(waiting.state == EclState::waiting && waiting.request &&
              waiting.request->choices.size() == 1,
          "880Ah waits for Enter");
    check(machine.resume(waiting.request->id, 0) && machine.run().state == EclState::completed,
          "Enter goes on");
}

// The item words are in GAME.EXE: 255 slots of 21 bytes, as in the earlier
// games' START.EXE.
void the_item_words_are_in_the_game_program()
{
    const auto folder = gateway_folder();
    if (folder.empty())
    {
        std::cout << "Gateway's files are not here; its item words are not read.\n";
        return;
    }
    std::ifstream program(folder / "GAME.EXE", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(program), {}};
    const auto words = opengold::por::decode_item_words(
        opengold::por::unpack_sound_executable(bytes), gateway_game);
    check(words && words->size() == 256 && (*words)[1] == "Battle Axe" && (*words)[2] == "Hand Axe",
          "GAME.EXE names the item words");
}

// Gateway's fights on the overland are on grass from its GAME.EXE's terrains
// (its BuildWildernessArena): on rough ground (class 10h) a river, here with
// a ford, then trees and scattered features, every die rolling its least.
void overland_fights_are_on_grass()
{
    const auto folder = gateway_folder();
    if (folder.empty())
    {
        std::cout << "Gateway's files are not here; its battlefield is not grown.\n";
        return;
    }
    std::ifstream program(folder / "GAME.EXE", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(program), {}};
    const auto wilderness =
        opengold::por::gateway_wilderness(opengold::por::unpack_sound_executable(bytes));
    check(wilderness && wilderness->combat_tiles.size() == 66 &&
              wilderness->terrain_classes.size() == 32,
          "GAME.EXE holds 65 terrains and 32 grounds");
    const auto least = [](int count, int)
    {
        return count;
    };
    const auto field =
        opengold::por::gateway_wilderness_battlefield(wilderness->combat_tiles, 0x10, least);
    const auto &board = field.geometry;
    check(board.width == 50 && board.height == 25, "The field is 50 squares by 25");
    const auto tile = [&](int row, int column)
    {
        return field.tiles.at(static_cast<std::size_t>(row * board.width + column));
    };
    const auto terrain = [&](int row, int column)
    {
        return board.terrain.at(static_cast<std::size_t>(row * board.width + column));
    };
    using opengold::rules::Terrain;
    // The river starts at column 26 (34 less 5, back to a column 2 short of
    // a multiple of 7) and drifts a column a row.
    check(tile(0, 26) == 28 && terrain(0, 26) == Terrain::open, "The river can be waded");
    check(tile(0, 27) == 32 && tile(1, 27) == 28, "A ford crosses it, and it drifts east");
    check(tile(1, 0) == 10 && terrain(1, 0) == Terrain::obstacle, "A small tree stands in the way");
    check(tile(0, 0) == 16 && terrain(0, 0) == Terrain::open, "Features lie on the grass");
}

std::size_t colours(const opengold::Image &image)
{
    std::set<std::uint32_t> seen;
    for (std::size_t p = 0; p + 3 < image.rgba.size(); p += 4)
    {
        seen.insert(image.rgba[p] << 16 | image.rgba[p + 1] << 8 | image.rgba[p + 2]);
    }
    return seen.size();
}

// A new game runs script 1 of disk 2: over picture 5 the party's night of
// celebration in Yartar's tavern, each screen waiting for Enter (CALL 880Ah),
// then the morning's loss, in the inn's room at (1,10) facing north with the
// walls of Yartar's three large sets (LOAD PIECES 36, 31, 15).
void the_opening_reaches_yartar()
{
    const auto folder = gateway_folder();
    if (folder.empty())
    {
        std::cout << "Gateway's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, gateway_game);
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
    const auto press_enter = [&session, &settle]
    {
        const auto &snapshot = session.snapshot();
        check(snapshot.phase == TourPhase::awaiting_continue && snapshot.choices.size() == 1,
              "The screen waits for Enter");
        check(session.choose(snapshot.continue_ticket, 0), "Enter goes on");
        settle();
    };
    settle();
    check(session.snapshot().area_id == 1, "Yartar's first map is loaded: " + diagnostics());
    check(session.snapshot().dialogue.find("IT IS A NIGHT OF CELEBRATION!") != std::string::npos,
          "The celebration begins: " + diagnostics());
    check(session.picture().has_value() && session.picture()->width == 88 &&
              colours(*session.picture()) > 16,
          "Picture 5 shows the tavern in the VGA mode's colors");
    press_enter();
    check(session.snapshot().dialogue.find("TELLS EVERYONE IN THE TAVERN") != std::string::npos &&
              session.snapshot().dialogue.find("CELEBRATION") == std::string::npos,
          "Enter clears the text before the next: " + diagnostics());
    press_enter();
    check(session.snapshot().dialogue.find("YOU AWAKEN TO DESPAIR") != std::string::npos &&
              !session.picture().has_value(),
          "The morning shows the inn's room: " + diagnostics());
    press_enter();
    const auto &snapshot = session.snapshot();
    check(snapshot.phase == TourPhase::completed &&
              snapshot.dialogue.find("JOURNAL ENTRY #5") != std::string::npos,
          "The opening ends: " + diagnostics());
    check(snapshot.pose.x == 1 && snapshot.pose.y == 10 &&
              snapshot.pose.facing == opengold::por::MapDirection::north,
          "The party stands at (1,10) facing north");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto view = session.observe_view();
    check(colours(view) > 8, "The 3D view shows the room's walls");
    check(session.explore(ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
}

// The overland: script 21 of disk 6 loads the map its cell 4C24h names
// (Yartar's is 22) and, with its in-dungeon cell clear, the symbol sets 18, 52
// and 53 alone, which draw the map from above with the party's arrow; a new
// script starts in a dungeon again, with its walls.
void the_overland_is_drawn_from_above()
{
    using opengold::por::EclAddress;
    using opengold::por::MapDirection;
    using opengold::test::TourAccess;
    const auto folder = gateway_folder();
    if (folder.empty())
    {
        std::cout << "Gateway's files are not here; its overland is not drawn.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, gateway_game);
    const auto play = [&session]
    {
        for (int step = 0; step < 200; ++step)
        {
            const auto &snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::running)
            {
                session.advance(1.0);
            }
            else if (snapshot.phase == TourPhase::awaiting_continue)
            {
                check(session.choose(snapshot.continue_ticket, 0), "The screen goes on");
            }
            else
            {
                return;
            }
        }
    };
    const auto cell = [&session](std::uint16_t address)
    {
        return TourAccess::machine(session).variable(EclAddress{address});
    };
    constexpr std::uint16_t in_dungeon = 0x4BE6, overland_map = 0x4C24;
    play();
    TourAccess::set_variable(session, EclAddress{overland_map}, 22);
    TourAccess::enter(session, 22, {8, 8, MapDirection::north});
    play();
    check(session.snapshot().area_id == 22 && session.script_diagnostics().empty(),
          "The overland's script loads Yartar's overland map");
    check(cell(in_dungeon) == 0, "On the overland the party is out of a dungeon");
    const auto view = session.observe_view();
    check(view.width == 88 && view.height == 88 && colours(view) > 4,
          "The overland is drawn from above in the view's 88 pixels");
    check(session.explore(ExplorationCommand::turn_left) &&
              session.observe_view().rgba != view.rgba,
          "The party's arrow turns with it");
    TourAccess::enter(session, 1, {1, 10, MapDirection::north});
    play();
    check(session.snapshot().area_id == 1 && cell(in_dungeon) == 1,
          "Back in Yartar the party is in a dungeon again");
    check(colours(session.observe_view()) > 8, "Yartar's walls show again");
}

} // namespace

int main()
{
    try
    {
        the_engine_prompts_are_menus();
        the_item_words_are_in_the_game_program();
        the_opening_reaches_yartar();
        overland_fights_are_on_grass();
        the_overland_is_drawn_from_above();
        std::cout << "Gateway passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
