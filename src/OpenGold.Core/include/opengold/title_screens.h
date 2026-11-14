#ifndef OPENGOLD_TITLE_SCREENS_H
#define OPENGOLD_TITLE_SCREENS_H

#include "opengold/formats.h"
#include "opengold/gold_box_game.h"

#include <filesystem>
#include <optional>
#include <vector>

// A game's title pictures (TITLE.DAX), shown as its original shows them when
// it starts (its Intro unit), before its menu (docs/GAMES.md).
namespace opengold
{

// One screen of the title: the whole 320 x 200 screen as it stands, how long
// it stays unless the player presses a key, and the game's sound effect it
// starts, if any.
struct TitleScreen
{
    Image screen;
    unsigned seconds{};
    std::optional<unsigned> sound;
};

struct TitleScreens
{
    std::vector<TitleScreen> screens;
    // Secret of the Silver Blades' title ends at the first key; the earlier
    // games' key ends only the screen shown.
    bool key_ends_all{};
};

// The full introduction's pictures (the installers' "F"). Empty for a game
// whose title is not drawn yet: the 256-color games and the color-header
// pictures of Gateway to the Savage Frontier. Throws when the folder's
// TITLE.DAX is missing or unreadable.
[[nodiscard]] TitleScreens load_title_screens(const std::filesystem::path &folder,
                                              GoldBoxGame game);

} // namespace opengold

#endif
