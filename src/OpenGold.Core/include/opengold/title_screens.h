#ifndef OPENGOLD_TITLE_SCREENS_H
#define OPENGOLD_TITLE_SCREENS_H

#include "opengold/formats.h"
#include "opengold/gold_box_game.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

// A game's title pictures (TITLE.DAX), shown as its original shows them when
// it starts (its Intro unit), before its menu (docs/GAMES.md). Pools of
// Darkness plays its opening film (INTRO) before them.
namespace opengold
{

// One screen of the title: the whole 320 x 200 screen as it stands, how long
// it stays unless the player presses a key, the game's sound effect it
// starts, if any, and where a key on it leads.
struct TitleScreen
{
    Image screen;
    double seconds{};
    std::optional<unsigned> sound;
    // The screen a key shows next: the number of screens ends the title, and
    // nullopt leaves the key unanswered (the opening film's title card).
    std::optional<std::size_t> after_key;
};

// A song an opening film plays from its own song file: Pools of Darkness'
// P1.SMS.
struct TitleSong
{
    std::filesystem::path file;
    unsigned number{};
};

struct TitleScreens
{
    std::vector<TitleScreen> screens;
    // The screen the game's title music starts with (the later games'):
    // Gateway's starts with its second picture, Secret's with its first.
    std::size_t music_screen{};
    // The song an opening film plays from the first screen until the title
    // music starts.
    std::optional<TitleSong> film_song;
};

// The full introduction's pictures (the installers' "F"); none for Unlimited
// Adventures. Throws when the
// folder's TITLE.DAX is missing or unreadable, or its opening film does not
// decode.
[[nodiscard]] TitleScreens load_title_screens(const std::filesystem::path &folder,
                                              GoldBoxGame game);

} // namespace opengold

#endif
