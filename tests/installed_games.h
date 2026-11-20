#ifndef OPENGOLD_INSTALLED_GAMES_H
#define OPENGOLD_INSTALLED_GAMES_H

#include <filesystem>
#include <map>

#include "opengold/game_finder.h"

namespace opengold::test
{
// The folder holding a game's original files, for the checks that read them:
// found by its files as the game finds it (opengold/game_finder.h), from the
// source checkout ctest runs in. Empty when the game's files are not here.
[[nodiscard]] inline std::filesystem::path installed_game_folder(GoldBoxGame game)
{
    static std::map<GoldBoxGame, std::filesystem::path> found;
    if (const auto known = found.find(game); known != found.end())
    {
        return known->second;
    }
    const auto search = environment_game_search(checkout_folder(std::filesystem::current_path()));
    return found[game] = find_game_folder(game, search).value_or(std::filesystem::path{});
}
} // namespace opengold::test

#endif
