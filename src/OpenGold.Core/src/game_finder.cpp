#include "opengold/game_finder.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <string_view>
#include <system_error>
#include <utility>

#include "opengold/game_census.h"
#include "opengold/path_text.h"

namespace opengold
{
namespace
{

namespace fs = std::filesystem;

// Home's folders macOS asks the player about before a program lists them
// (iCloud's are under Library).
constexpr std::array<std::string_view, 4> private_home_folders{"Desktop", "Documents", "Downloads",
                                                               "Library"};

// An absolute path without symbolic links, "..", or a trailing separator, so
// paths compare as the folders they name; empty stays empty.
fs::path tidy(const fs::path &folder)
{
    if (folder.empty())
    {
        return {};
    }
    std::error_code error;
    auto full = fs::absolute(folder, error);
    if (error)
    {
        return {};
    }
    if (auto real = fs::weakly_canonical(full, error); !error)
    {
        full = std::move(real);
    }
    full = full.lexically_normal();
    if (!full.has_filename() && full != full.root_path())
    {
        full = full.parent_path();
    }
    return full;
}

bool hidden(const fs::path &name)
{
    const auto text = path_text(name);
    return !text.empty() && text.front() == '.';
}

bool private_to_home(const fs::path &folder, const fs::path &home)
{
    return !home.empty() && folder.parent_path() == home &&
           std::ranges::find(private_home_folders, path_text(folder.filename())) !=
               private_home_folders.end();
}

// Whether ancestor is folder or one of the folders holding it.
bool holds(const fs::path &ancestor, const fs::path &folder)
{
    const auto [ancestor_end, folder_end] = std::ranges::mismatch(ancestor, folder);
    return ancestor_end == ancestor.end();
}

// A symbolic link back to folder or above it would show the same folders again.
bool loops_back(const fs::path &link, const fs::path &folder)
{
    std::error_code error;
    const auto target = fs::canonical(link, error);
    return error || holds(target, tidy(folder));
}

// The folders directly in folder that the search may enter, sorted. A folder
// that cannot be read has none.
std::vector<fs::path> subfolders(const fs::path &folder, const fs::path &home)
{
    std::vector<fs::path> found;
    std::error_code error;
    fs::directory_iterator entry(folder, fs::directory_options::skip_permission_denied, error);
    for (; !error && entry != fs::directory_iterator(); entry.increment(error))
    {
        const auto &path = entry->path();
        if (hidden(path.filename()) || private_to_home(path, home))
        {
            continue;
        }
        std::error_code kind_error;
        if (!entry->is_directory(kind_error) || kind_error)
        {
            continue;
        }
        if (entry->is_symlink(kind_error) && loops_back(path, folder))
        {
            continue;
        }
        found.push_back(path);
    }
    std::ranges::sort(found);
    return found;
}

using FoundGame = std::pair<fs::path, GoldBoxGame>;

// The games in root's children and grandchildren, in sorted order.
std::vector<FoundGame> games_below(const fs::path &root, const fs::path &home)
{
    std::vector<fs::path> folders;
    for (const auto &child : subfolders(root, home))
    {
        folders.push_back(child);
        std::ranges::move(subfolders(child, home), std::back_inserter(folders));
    }
    std::ranges::sort(folders);
    std::vector<FoundGame> games;
    for (const auto &folder : folders)
    {
        if (const auto game = identify_game(folder))
        {
            games.emplace_back(folder, *game);
        }
    }
    return games;
}

// Whether the upward search may list folder: not hidden, and not one of the
// folders macOS guards, nor inside one.
bool searchable(const fs::path &folder, const fs::path &home)
{
    const auto below_home = folder.lexically_relative(home);
    if (below_home.empty() || below_home == ".")
    {
        return true;
    }
    if (std::ranges::find(private_home_folders, path_text(*below_home.begin())) !=
        private_home_folders.end())
    {
        return false;
    }
    return std::ranges::none_of(below_home,
                                [](const fs::path &part)
                                {
                                    return hidden(part);
                                });
}

// The folders the upward search looks in, nearest first.
std::vector<fs::path> upward_folders(const fs::path &start, const fs::path &home)
{
    if (home.empty())
    {
        return {};
    }
    if (start.empty() || start == home || !holds(home, start))
    {
        return {home};
    }
    std::vector<fs::path> folders;
    for (auto folder = start.parent_path();; folder = folder.parent_path())
    {
        if (searchable(folder, home))
        {
            folders.push_back(folder);
        }
        if (folder == home)
        {
            return folders;
        }
    }
}

// Shows each game folder the search finds to visit, in the search's order,
// until visit returns true.
void visit_found_games(const GameSearch &search,
                       const std::function<bool(const FoundGame &)> &visit)
{
    const auto home = tidy(search.home);
    for (const auto &named : search.named)
    {
        const auto root = tidy(named);
        if (root.empty())
        {
            continue;
        }
        if (const auto game = identify_game(root); game && visit({root, *game}))
        {
            return;
        }
        for (const auto &found : games_below(root, home))
        {
            if (visit(found))
            {
                return;
            }
        }
    }
    for (const auto &folder : upward_folders(tidy(search.start), home))
    {
        for (const auto &found : games_below(folder, home))
        {
            if (visit(found))
            {
                return;
            }
        }
    }
}

fs::path environment_folder(const char *variable)
{
    const char *value = std::getenv(variable); // borrowed from the environment
    return value ? fs::path(value) : fs::path();
}

} // namespace

std::optional<GameFolder> locate_game_folder(GoldBoxGame game, const GameSearch &search)
{
    // A chosen folder whose files are gone (or another game's) is looked for again.
    if (!search.chosen.empty() && identify_game(search.chosen) == game)
    {
        return GameFolder{search.chosen, GameFolderSource::chosen};
    }
    std::optional<GameFolder> folder;
    visit_found_games(search,
                      [&](const FoundGame &found)
                      {
                          if (found.second == game)
                          {
                              folder = GameFolder{found.first, GameFolderSource::found};
                          }
                          return folder.has_value();
                      });
    return folder;
}

std::optional<fs::path> find_game_folder(GoldBoxGame game, const GameSearch &search)
{
    if (auto located = locate_game_folder(game, search))
    {
        return std::move(located->folder);
    }
    return std::nullopt;
}

std::map<GoldBoxGame, fs::path> find_game_folders(const GameSearch &search)
{
    std::map<GoldBoxGame, fs::path> folders;
    visit_found_games(search,
                      [&](const FoundGame &found)
                      {
                          folders.emplace(found.second, found.first);
                          return folders.size() == gold_box_games.size();
                      });
    return folders;
}

GameSearch environment_game_search(const fs::path &start)
{
    GameSearch search;
    for (const auto *variable : {"OPENGOLD_GAME_DIR", "OPENGOLD_GOLDBOX_DIR"})
    {
        if (auto folder = environment_folder(variable); !folder.empty())
        {
            search.named.push_back(std::move(folder));
        }
    }
    search.start = start;
    search.home = environment_folder("HOME");
    if (search.home.empty())
    {
        search.home = environment_folder("USERPROFILE");
    }
    return search;
}

fs::path checkout_folder(const fs::path &folder)
{
    const auto full = tidy(folder);
    for (auto candidate = full; !candidate.empty(); candidate = candidate.parent_path())
    {
        std::error_code error;
        if (fs::exists(candidate / ".git", error))
        {
            return candidate;
        }
        if (candidate == candidate.root_path())
        {
            break;
        }
    }
    return full;
}

} // namespace opengold
