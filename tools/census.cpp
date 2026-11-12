// opengold_census: the reading census of docs/GAMES.md. For each game folder it
// lists every kind of file, how many of its pieces the game's decoders read and
// the first that failed.
//   opengold_census [--require-complete] FOLDER...
// A FOLDER is one game's folder, or a folder holding several (POOLRAD, CURSE, ...).
// --require-complete exits with 1 unless every game found decodes completely.

#include "opengold/game_census.h"
#include "opengold/path_text.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
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
}

std::vector<std::pair<std::filesystem::path, GoldBoxGame>>
games_in(const std::filesystem::path &folder)
{
    if (const auto game = opengold::identify_game(folder))
    {
        return {{folder, *game}};
    }
    std::vector<std::pair<std::filesystem::path, GoldBoxGame>> found;
    for (const auto game : opengold::gold_box_games)
    {
        const auto candidate = folder / std::string(opengold::game_folder(game));
        if (opengold::identify_game(candidate) == game)
        {
            found.emplace_back(candidate, game);
        }
    }
    return found;
}

} // namespace

int main(int argc, char **argv)
{
    bool require_complete = false;
    std::vector<std::filesystem::path> folders;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--require-complete")
        {
            require_complete = true;
        }
        else
        {
            folders.emplace_back(argument);
        }
    }
    if (folders.empty())
    {
        std::cerr << "Usage: opengold_census [--require-complete] FOLDER...\n";
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
            all_complete = all_complete && census.complete();
        }
    }
    return require_complete && !all_complete ? 1 : 0;
}
