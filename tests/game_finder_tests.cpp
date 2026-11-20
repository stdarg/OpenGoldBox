// Finding a game's folder by its files (opengold/game_finder.h), in a made-up
// home folder: never the player's own, so no check lists the real Desktop,
// Documents or Downloads. Each game's folder holds only the empty file
// identify_game tells it by.
#include "opengold/game_finder.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace
{
namespace fs = std::filesystem;
using opengold::find_game_folder;
using opengold::GameSearch;
using opengold::GoldBoxGame;
using opengold::test::check;

constexpr auto pool = GoldBoxGame::pool_of_radiance;
constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
constexpr auto secret = GoldBoxGame::secret_of_the_silver_blades;

// A folder of its own for each check, removed afterwards.
class Scratch
{
  public:
    explicit Scratch(const std::string &name)
        : root_(fs::temp_directory_path() / ("opengold-game-finder-" + name))
    {
        fs::remove_all(root_);
        fs::create_directories(root_ / "home");
        root_ = fs::canonical(root_);
    }

    Scratch(const Scratch &) = delete;
    Scratch &operator=(const Scratch &) = delete;

    ~Scratch()
    {
        std::error_code ignored;
        fs::permissions(root_ / "home" / "locked", fs::perms::owner_all, ignored);
        fs::remove_all(root_, ignored);
    }

    [[nodiscard]] const fs::path &root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] fs::path home() const
    {
        return root_ / "home";
    }

    // A folder whose files say it is game's, wherever and whatever its name.
    fs::path game(const fs::path &folder, GoldBoxGame game) const
    {
        const auto path = root_ / folder;
        fs::create_directories(path);
        std::ofstream(path / marker(game)).put('\0');
        return path;
    }

    fs::path folder(const fs::path &folder) const
    {
        fs::create_directories(root_ / folder);
        return root_ / folder;
    }

    // A search from start (under root) to the made-up home.
    [[nodiscard]] GameSearch from(const fs::path &start) const
    {
        GameSearch search;
        search.start = this->folder(start);
        search.home = home();
        return search;
    }

  private:
    static const char *marker(GoldBoxGame game)
    {
        switch (game)
        {
        case GoldBoxGame::pool_of_radiance:
            return "SQRPACI.DAX";
        case GoldBoxGame::curse_of_the_azure_bonds:
            return "TILES.DAX";
        default:
            return "SOUND.DAX";
        }
    }

    fs::path root_;
};

void finds_games_two_levels_below_home()
{
    const Scratch scratch("two-levels");
    const auto pool_folder = scratch.game("home/Pool of Radiance", pool);
    const auto curse_folder = scratch.game("home/GoldBox/my curse copy", curse);
    scratch.game("home/games/gold/box/SECRET", secret);
    const auto search = scratch.from("home/src/OpenGold");
    check(find_game_folder(pool, search) == pool_folder,
          "A child of home holding Pool of Radiance's files is found, whatever its name");
    check(find_game_folder(curse, search) == curse_folder, "A grandchild of home is found");
    check(!find_game_folder(secret, search), "A folder deeper than two levels is not found");
}

void never_lists_private_or_hidden_folders()
{
    const Scratch scratch("private");
    scratch.game("home/Documents", pool);
    scratch.game("home/Desktop/POOLRAD", pool);
    scratch.game("home/Downloads/POOLRAD", pool);
    scratch.game("home/Library/POOLRAD", pool);
    scratch.game("home/.games/POOLRAD", pool);
    scratch.game("home/games/.POOLRAD", pool);
    scratch.game("home/Documents/CURSE", curse);
    check(!find_game_folder(pool, scratch.from("home/src/OpenGold")),
          "Desktop, Documents, Downloads, Library and hidden folders are not looked in");
    check(!find_game_folder(curse, scratch.from("home/Documents/OpenGold/build")),
          "A start inside Documents does not list Documents on the way up");
    const auto found = scratch.game("home/POOLRAD", pool);
    check(find_game_folder(pool, scratch.from("home/Documents/OpenGold/build")) == found,
          "A start inside Documents still searches home");
}

void the_nearest_folder_wins()
{
    const Scratch scratch("nearest");
    scratch.game("home/POOLRAD", pool);
    const auto near = scratch.game("home/src/POOLRAD copy", pool);
    check(find_game_folder(pool, scratch.from("home/src/OpenGold")) == near,
          "A game beside the checkout wins over one in home");
    const auto first = scratch.game("home/A/CURSE copy", curse);
    scratch.game("home/B", curse);
    check(find_game_folder(curse, scratch.from("home/src/OpenGold")) == first,
          "Folders as near as each other are taken in sorted order");
}

void a_start_outside_home_searches_home_only()
{
    const Scratch scratch("outside");
    scratch.game("Applications/POOLRAD", pool);
    check(!find_game_folder(pool, scratch.from("Applications/OpenGoldBox")),
          "Outside home, only home is searched");
    const auto found = scratch.game("home/GoldBox/Pool", pool);
    check(find_game_folder(pool, scratch.from("Applications/OpenGoldBox")) == found,
          "Outside home, home's games are found");
    auto search = scratch.from("Applications/OpenGoldBox");
    search.home.clear();
    check(!find_game_folder(pool, search), "Without a home there is no upward search");
}

void named_folders_are_matched_by_their_files()
{
    const Scratch scratch("named");
    scratch.game("home/POOLRAD", pool);
    const auto collection = scratch.folder("collection");
    const auto pool_copy = scratch.game("collection/Pool copy", pool);
    const auto curse_copy = scratch.game("collection/old/Azure Bonds", curse);
    auto search = scratch.from("home/src/OpenGold");
    search.named = {collection};
    check(find_game_folder(pool, search) == pool_copy,
          "A child of the named folder holding the game's files wins over the search");
    check(find_game_folder(curse, search) == curse_copy, "So does a grandchild");
    search.named = {pool_copy};
    check(find_game_folder(pool, search) == pool_copy, "The named folder may be the game's own");
    search.named = {curse_copy};
    check(find_game_folder(pool, search) == scratch.home() / "POOLRAD",
          "A named folder holding another game leaves the search to find this one");
}

void the_chosen_folder_wins()
{
    const Scratch scratch("chosen");
    const auto found = scratch.game("home/POOLRAD", pool);
    auto search = scratch.from("home/src/OpenGold");
    search.named = {found};
    search.chosen = scratch.root() / "chosen by the player";
    check(find_game_folder(pool, search) == search.chosen,
          "The folder the player chose is taken as it is");
}

void loops_and_locked_folders_are_passed_over()
{
#ifndef _WIN32
    const Scratch scratch("loops");
    const auto home = scratch.home();
    fs::create_directory_symlink(home, home / "loop");
    fs::create_directory_symlink(scratch.root(), home / "games");
    fs::create_directory_symlink(home / "nowhere", home / "broken");
    scratch.game("home/locked/POOLRAD", pool);
    fs::permissions(home / "locked", fs::perms::none);
    check(!find_game_folder(pool, scratch.from("home/src/OpenGold")),
          "Links back up and folders that cannot be read are passed over");
    const auto found = scratch.game("elsewhere/POOLRAD", pool);
    fs::create_directory_symlink(found, home / "Pool link");
    check(find_game_folder(pool, scratch.from("home/src/OpenGold")) == home / "Pool link",
          "A link to a game's folder elsewhere is followed");
#endif
}

void the_checkout_is_the_folder_with_git()
{
    const Scratch scratch("checkout");
    const auto checkout = scratch.folder("home/src/OpenGold");
    scratch.folder("home/src/OpenGold/.git");
    check(opengold::checkout_folder(scratch.folder("home/src/OpenGold/build/macos")) == checkout,
          "A build folder's search starts at its checkout");
    const auto loose = scratch.folder("loose/folder");
    check(opengold::checkout_folder(loose) == loose, "Outside a checkout, the folder itself");
}

} // namespace

int main()
{
    try
    {
        finds_games_two_levels_below_home();
        never_lists_private_or_hidden_folders();
        the_nearest_folder_wins();
        a_start_outside_home_searches_home_only();
        named_folders_are_matched_by_their_files();
        the_chosen_folder_wins();
        loops_and_locked_folders_are_passed_over();
        the_checkout_is_the_folder_with_git();
        std::cout << "Game finder passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
