// Pools of Darkness's campaign (docs/games/darkness.md): its script commands,
// and its opening played on the real files when OPENGOLD_GOLDBOX_DIR names the
// folder holding DARKNESS.
#include "opengold/campaign_session.h"
#include "opengold/ecl.h"
#include "opengold/ecl_machine.h"
#include "opengold/game_census.h"
#include "opengold/pool_radiance.h"
#include "opengold/por_sound.h"
#include "test_support.h"

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
using opengold::por::EclAddress;
using opengold::por::EclMachine;
using opengold::por::EclState;
using opengold::por::ExplorationCommand;
using opengold::por::TourPhase;
using opengold::test::check;

constexpr auto darkness_game = GoldBoxGame::pools_of_darkness;

std::filesystem::path darkness_folder()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto folder = games ? std::filesystem::path(games) / "DARKNESS" : std::filesystem::path();
    return games && opengold::identify_game(folder) == darkness_game ? folder
                                                                     : std::filesystem::path();
}

// A script of Pools of Darkness whose five entries run body, which is
// followed by EXIT.
std::shared_ptr<const opengold::por::EclProgram> script(std::vector<std::uint8_t> body)
{
    const auto start =
        static_cast<std::uint16_t>(opengold::por::ecl_memory(darkness_game).origin + 5 * 4);
    std::vector<std::uint8_t> record{0, 0};
    for (int entry = 0; entry < 5; ++entry)
    {
        record.insert(record.end(), {0x01, 0x01, static_cast<std::uint8_t>(start & 0xff),
                                     static_cast<std::uint8_t>(start >> 8)});
    }
    record.insert(record.end(), body.begin(), body.end());
    record.push_back(0x00);
    return std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(record, "commands", darkness_game));
}

// YES NO (22h) asks and sets only the comparison: yes the equal flag. WAIT KEY
// (1Dh) waits for Enter. CLEAR AND EXIT (23h) clears the text and ends the
// script, though data follow it.
void the_new_commands_run()
{
    const EclAddress said_yes{0x0100};
    // YES NO; IF =; SAVE 7 to 100h.
    const std::vector<std::uint8_t> ask{0x22, 0x16, 0x09, 0x00, 0x07, 0x01, 0x00, 0x01};
    for (const std::size_t choice : {0U, 1U})
    {
        EclMachine machine(script(ask));
        machine.bind_variable(said_yes, 0);
        check(machine.start(4), "The script starts");
        const auto asked = machine.run();
        check(asked.state == EclState::waiting && asked.request &&
                  asked.request->choices == std::vector<std::string>{"YES", "NO"},
              "YES NO asks");
        check(machine.resume(asked.request->id, choice), "The player answers");
        check(machine.run().state == EclState::completed, "The script goes on to its end");
        check(machine.variable(said_yes) == (choice == 0 ? 7 : 0), "Yes compares equal, no not");
    }
    // WAIT KEY; CLEAR AND EXIT; then a byte no command starts with.
    EclMachine machine(script({0x1D, 0x23, 0xFF}));
    check(machine.start(4), "The script starts");
    const auto waiting = machine.run();
    check(waiting.state == EclState::waiting && waiting.request &&
              waiting.request->choices.size() == 1,
          "WAIT KEY waits for Enter");
    check(machine.resume(waiting.request->id, 0), "Enter goes on");
    const auto cleared = machine.run();
    check(cleared.state == EclState::waiting && cleared.request && cleared.request->clear,
          "CLEAR AND EXIT clears the text");
    check(machine.resume(cleared.request->id) && machine.run().state == EclState::completed,
          "and ends the script there");
}

// The item words are in GAME.EXE: 125 slots of 21 bytes.
void the_item_words_are_in_the_game_program()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its item words are not read.\n";
        return;
    }
    std::ifstream program(folder / "GAME.EXE", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(program), {}};
    const auto words = opengold::por::decode_item_words(
        opengold::por::unpack_sound_executable(bytes), darkness_game);
    check(words && words->size() == 126 && (*words)[1] == "Battle Axe" && (*words)[2] == "Hand Axe",
          "GAME.EXE names the item words");
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

// A new game runs script 16: map 16, New Phlan's docks, with wall set 1, the
// party at (15,0) facing west over picture 148, where Rolf, the portmaster,
// offers a tour. Declining it, he walks the party out, and the docks' walls are
// drawn in the wall set's own colors.
void the_opening_reaches_new_phlan()
{
    const auto folder = darkness_folder();
    if (folder.empty())
    {
        std::cout << "Pools of Darkness's files are not here; its opening is not played.\n";
        return;
    }
    auto session = opengold::load_campaign(folder, darkness_game);
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
    const auto &snapshot = session.snapshot();
    check(snapshot.area_id == 16, "New Phlan's docks are loaded: " + diagnostics());
    check(snapshot.dialogue.find("I AM ROLF, PORTMASTER") != std::string::npos &&
              snapshot.choices == std::vector<std::string>{"YES", "NO"},
          "Rolf offers a tour: " + diagnostics());
    check(session.picture().has_value() && session.picture()->width == 88 &&
              colours(*session.picture()) > 16,
          "Picture 148 shows Rolf in 256 colors");
    check(session.choose(snapshot.continue_ticket, 1), "The party declines");
    settle();
    check(session.snapshot().dialogue.find("THEN LET ME ESCORT YOU OUT") != std::string::npos,
          "Rolf walks the party out: " + diagnostics());
    for (int screen = 0; screen < 8 && session.snapshot().phase == TourPhase::awaiting_continue;
         ++screen)
    {
        check(session.choose(session.snapshot().continue_ticket, 0), "The words go on");
        settle();
    }
    check(session.snapshot().phase == TourPhase::completed, "The opening ends: " + diagnostics());
    // Two steps west from (15,0), then the steps of his table, to the gate's
    // street at (11,2) facing south.
    const auto pose = session.snapshot().pose;
    check(pose.x == 11 && pose.y == 2 && pose.facing == opengold::por::MapDirection::south,
          "Rolf leaves the party at (11,2) facing south");
    check(session.snapshot().footsteps > 0, "His steps are heard");
    check(session.script_diagnostics().empty(), "The opening needs nothing unsupported");
    const auto view = session.observe_view();
    check(colours(view) > 8, "The 3D view shows the docks' walls in 256 colors");
    check(session.explore(ExplorationCommand::turn_left), "The party turns");
    check(session.observe_view().rgba != view.rgba, "Turning shows another view");
}

} // namespace

int main()
{
    try
    {
        the_new_commands_run();
        the_item_words_are_in_the_game_program();
        the_opening_reaches_new_phlan();
        std::cout << "Pools of Darkness passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
