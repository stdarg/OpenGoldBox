// opengold_census: the reading census of docs/GAMES.md. For each game folder it
// lists every kind of file, how many of its pieces the game's decoders read and
// the first that failed.
//   opengold_census [--require-complete] [--scripts] FOLDER...
//   opengold_census --find [START]
// A FOLDER is one game's folder, or a folder whose children or grandchildren
// are games' folders, each told by its files.
// --require-complete exits with 1 unless every game found decodes completely.
// --scripts also lists what the Buck Rogers games' scripts ask of the engine
// (opengold/buck_rogers_scripts.h).
// --find prints each game the game itself would find (opengold/game_finder.h),
// one "NAME<TAB>FOLDER" line each (NAME as game_folder gives it), searching
// upward from START's source checkout (the current folder's by default).

#include "opengold/buck_rogers_scripts.h"
#include "opengold/game_census.h"
#include "opengold/game_finder.h"
#include "opengold/path_text.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using opengold::GoldBoxGame;

void print_list(const char *label, const std::vector<std::string> &names)
{
    if (names.empty())
    {
        return;
    }
    std::cout << "  " << label << ':';
    for (const auto &name : names)
    {
        std::cout << ' ' << name;
    }
    std::cout << '\n';
}

void print(const opengold::GameCensus &census, const std::filesystem::path &folder)
{
    std::cout << opengold::game_title(census.game) << " (" << opengold::path_text(folder)
              << "): " << (census.complete() ? "complete" : "incomplete") << '\n';
    for (const auto &row : census.rows)
    {
        std::cout << "  " << (row.complete() ? "ok  " : "FAIL") << ' ' << std::left << std::setw(52)
                  << row.kind << std::right << std::setw(5) << row.decoded << " of " << std::setw(5)
                  << row.pieces;
        if (!row.complete())
        {
            std::cout << "  first: " << row.first_failure;
        }
        std::cout << '\n';
    }
    print_list("Not read yet", census.unread_files);
    print_list("Programs, settings and saves (never read)", census.program_files);
    print_list("Not the game's (it never opens them)", census.foreign_files);
}

std::string hex(unsigned value)
{
    std::ostringstream text;
    text << std::hex << std::uppercase << value << 'h';
    return text.str();
}

std::string where(const opengold::ScriptPlace &place)
{
    return place.archive + " script " + std::to_string(place.script) + " at " + hex(place.address);
}

// The values, "or" between them, and "or set elsewhere" when the trace is incomplete.
std::string values(const opengold::TracedValues &traced)
{
    std::string text;
    for (const auto value : traced.values)
    {
        text += (text.empty() ? "" : " or ") + std::to_string(value);
    }
    if (!traced.complete)
    {
        text += text.empty() ? "set elsewhere" : " or set elsewhere";
    }
    return text;
}

void print_uses(const char *heading, const std::vector<opengold::ScriptCommandUse> &uses)
{
    std::cout << "  " << heading << ": " << uses.size() << '\n';
    for (const auto &use : uses)
    {
        std::cout << "    " << where(use.place) << ": " << use.command;
        for (const auto &operand : use.operands)
        {
            std::cout << ' ' << operand;
        }
        std::cout << '\n';
    }
}

void print_scripts(const std::filesystem::path &folder, GoldBoxGame game)
{
    const auto listing = opengold::list_buck_rogers_scripts(folder, game);
    std::cout << "  Skill checks (difficulty 0 easiest to 4 hardest; who rolls: 0 the current "
                 "character, else the party's best): "
              << listing.skill_checks.size() << '\n';
    for (const auto &check : listing.skill_checks)
    {
        std::cout << "    " << where(check.place) << ": " << hex(check.command) << " skill "
                  << values(check.skill) << ", difficulty " << values(check.difficulty) << ", who "
                  << values(check.whole_party) << '\n';
    }
    print_uses("Ship combats", listing.ship_combats);
    print_uses("Solar map", listing.solar_maps);
    print_uses("Shops", listing.shops);
    print_uses("Fuel (cell 4D1Eh) written", listing.fuel_writes);
    print_uses("PROGRAM", listing.programs);
    std::cout << "  Scripts loaded: " << listing.script_loads.size() << '\n';
    for (const auto &load : listing.script_loads)
    {
        std::cout << "    " << where(load.place) << ": script " << values(load.script) << ", disk "
                  << values(load.data_set) << '\n';
    }
    print_uses("Commands an IF skips unevenly", listing.uneven_skips);
    print_uses("Reachable 3Bh and 3Ch", listing.stalls);
}

std::vector<std::pair<std::filesystem::path, GoldBoxGame>>
games_in(const std::filesystem::path &folder)
{
    opengold::GameSearch search;
    search.named = {folder};
    std::vector<std::pair<std::filesystem::path, GoldBoxGame>> found;
    for (const auto &[game, path] : opengold::find_game_folders(search))
    {
        found.emplace_back(path, game);
    }
    return found;
}

// Each game's folder as the game would find it, from start's checkout.
int print_found_games(const std::filesystem::path &start)
{
    const auto search = opengold::environment_game_search(opengold::checkout_folder(start));
    for (const auto &[game, folder] : opengold::find_game_folders(search))
    {
        std::cout << opengold::game_folder(game) << '\t' << opengold::path_text(folder) << '\n';
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    bool require_complete = false;
    bool list_scripts = false;
    std::vector<std::filesystem::path> folders;
    if (argc >= 2 && argc <= 3 && std::string(argv[1]) == "--find")
    {
        return print_found_games(argc == 3 ? std::filesystem::path(argv[2])
                                           : std::filesystem::current_path());
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--require-complete")
        {
            require_complete = true;
        }
        else if (argument == "--scripts")
        {
            list_scripts = true;
        }
        else
        {
            folders.emplace_back(argument);
        }
    }
    if (folders.empty())
    {
        std::cerr << "Usage: opengold_census [--require-complete] [--scripts] FOLDER... | "
                     "--find [START]\n";
        return 2;
    }
    bool all_complete = true;
    for (const auto &folder : folders)
    {
        const auto games = games_in(folder);
        if (games.empty())
        {
            std::cerr << opengold::path_text(folder) << ": no Gold Box game found\n";
            return 2;
        }
        for (const auto &[path, game] : games)
        {
            const auto census = opengold::take_census(path, game);
            print(census, path);
            if (list_scripts && opengold::is_buck_rogers(game))
            {
                print_scripts(path, game);
            }
            all_complete = all_complete && census.complete();
        }
    }
    return require_complete && !all_complete ? 1 : 0;
}
