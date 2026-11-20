#ifndef OPENGOLDBOX_GODOT_GAME_SEARCH_H
#define OPENGOLDBOX_GODOT_GAME_SEARCH_H

#include "godot_path.h"
#include "opengold/game_finder.h"
#include <filesystem>
#include <godot_cpp/classes/os.hpp>

namespace presentation
{
// The search for the games' folders (opengold/game_finder.h) that the
// environment describes, starting at start: OPENGOLD_GAME_DIR,
// OPENGOLD_GOLDBOX_DIR and the home folder (HOME, or USERPROFILE on Windows).
// Read through Godot, so a variable a script sets is seen on Windows too, where
// the C runtime's copy of the environment does not follow it.
[[nodiscard]] inline opengold::GameSearch godot_game_search(std::filesystem::path start)
{
    auto *os = godot::OS::get_singleton(); // borrowed engine singleton
    opengold::GameSearch search;
    for (const char *variable : {"OPENGOLD_GAME_DIR", "OPENGOLD_GOLDBOX_DIR"})
    {
        const auto folder = os->get_environment(variable).strip_edges();
        if (!folder.is_empty())
        {
            search.named.push_back(path_from_godot(folder));
        }
    }
    auto home = os->get_environment("HOME").strip_edges();
    if (home.is_empty())
    {
        home = os->get_environment("USERPROFILE").strip_edges();
    }
    search.home = path_from_godot(home);
    search.start = std::move(start);
    return search;
}
} // namespace presentation

#endif
