#ifndef OPENGOLD_GAME_TRAITS_H
#define OPENGOLD_GAME_TRAITS_H
#include "opengold/ecl_address.h"
#include "opengold/gold_box_game.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace opengold
{

// How a game's campaign is loaded (campaign_session.cpp, load_campaign).
enum class CampaignLoader : std::uint8_t
{
    none, // The game cannot be played yet.
    pool_of_radiance,
    later, // The campaigns after Pool of Radiance, by their LaterCampaign.
    dark_queen
};

// What sets the campaigns after Pool of Radiance apart (docs/games/curse.md,
// docs/games/secret.md, docs/games/gateway.md).
struct LaterCampaign
{
    // A new game runs this script, on this disk.
    unsigned opening_script{}, opening_disk{};
    // The disk whose scripts play at the root, out of any map.
    unsigned root_disk{};
    // Curse's root disk 1 has no map: its scripts (80 to 82) are the
    // wilderness, fought on its own battlefield. Secret has none, and
    // Gateway's overland (script 21) loads its maps by cells.
    bool wilderness{};
};

// Which decoder reads a game's combat icons (creature_art.cpp): Gateway's
// icons are pictures of its own format, the 256-color games' VGA pictures.
enum class CombatIconFormat : std::uint8_t
{
    ega,
    gateway,
    vga
};

// A TITLE.DAX picture drawn at a character cell, as the original's ShowPic
// draws it: whole, over what is on the screen.
struct TitleDraw
{
    std::uint8_t record{};
    int column{}, row{};
};

// What a key does while a step's screen shows.
enum class TitleKey
{
    passes_screen,
    ends_title,
    skips_to_last // To the last screen, which stays its own time.
};

struct TitleStep
{
    std::vector<TitleDraw> draws;
    unsigned seconds{};
    std::optional<unsigned> sound;
    TitleKey key{TitleKey::passes_screen};
};

struct TitleScript
{
    std::vector<TitleStep> steps;
    // Each step's pictures are drawn on a cleared screen, not over the last.
    bool clears_between{};
    std::size_t music_screen{};
    // The opening film (INTRO.ACF and INTRO.BMA) plays before the steps.
    bool opens_with_film{};
};

// The facts about a game that Core reads to play it, one row per GoldBoxGame
// (game_traits.cpp).
struct GameTraits
{
    CampaignLoader loader{};
    // Set for the games the later loader plays.
    std::optional<LaterCampaign> later_campaign;
    // The cell whose change from 0 to 1 wins a game with no ending of its own
    // (docs/DEVIATIONS.md): the event that sets it wins the game as it ends.
    std::optional<por::EclAddress> won_cell;
    // The piece of the game's music bank its title plays.
    std::optional<unsigned> title_music_piece;
    // The game's Intro.Introduction with its full-introduction flag on, without
    // the credits that follow the pictures; no steps for a game whose title
    // OpenGoldBox does not show.
    TitleScript title;
    CombatIconFormat combat_icons{};
    // The experience each gem among the monsters' loot is worth (postcbt.pas,
    // DetermineLootExp).
    unsigned gem_experience{};
    // The PICn.DAX and BIGPICn.DAX a campaign's pictures run to.
    unsigned picture_disks{};
};

[[nodiscard]] const GameTraits &traits(GoldBoxGame game);

} // namespace opengold

#endif
