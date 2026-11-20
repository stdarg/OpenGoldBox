#ifndef OPENGOLD_GAME_FINDER_H
#define OPENGOLD_GAME_FINDER_H

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include "opengold/gold_box_game.h"

// Finding a game's folder by its files (identify_game), wherever it is and
// whatever it is called (docs/TECH.md, "Finding the games' folders"). The game,
// the tools and the tests all look the same way.
namespace opengold
{

struct GameSearch
{
    // The folder the player chose for the game, taken as it is.
    std::filesystem::path chosen;
    // The folders OPENGOLD_GAME_DIR and OPENGOLD_GOLDBOX_DIR name, in that
    // order: each is looked in itself, then its children and grandchildren.
    std::vector<std::filesystem::path> named;
    // The game's own folder, where the upward search starts, and the player's
    // home folder, after which it stops. Empty home: no upward search.
    std::filesystem::path start, home;
};

// The game's folder: the chosen one, else the first named folder (or child or
// grandchild of one) whose files are that game's, else the upward search's.
// The upward search looks in start's parent, then its parent's parent and so
// on up to and including home (home only when start is not under it); at each
// it looks at the children and grandchildren, the first in sorted order
// winning. It never lists a hidden folder or home's Desktop, Documents,
// Downloads or Library: macOS asks the player before a program reads those.
[[nodiscard]] std::optional<std::filesystem::path> find_game_folder(GoldBoxGame game,
                                                                    const GameSearch &search);

// Every game the named folders and the upward search find, each where
// find_game_folder finds it (chosen is not used).
[[nodiscard]] std::map<GoldBoxGame, std::filesystem::path>
find_game_folders(const GameSearch &search);

// The search the process's environment describes: OPENGOLD_GAME_DIR,
// OPENGOLD_GOLDBOX_DIR and the home folder (HOME, or USERPROFILE on Windows),
// starting at start.
[[nodiscard]] GameSearch environment_game_search(const std::filesystem::path &start);

// The source checkout holding folder (the nearest folder with a .git), or
// folder itself outside one: developers' and tests' runs start their search there.
[[nodiscard]] std::filesystem::path checkout_folder(const std::filesystem::path &folder);

} // namespace opengold

#endif
