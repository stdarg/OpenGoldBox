#ifndef OPENGOLD_GOLD_BOX_GAME_H
#define OPENGOLD_GOLD_BOX_GAME_H

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace opengold
{

// The Forgotten Realms Gold Box games, in release order (docs/GAMES.md).
enum class GoldBoxGame : std::uint8_t
{
    pool_of_radiance,
    curse_of_the_azure_bonds,
    secret_of_the_silver_blades,
    gateway_to_the_savage_frontier,
    pools_of_darkness,
    treasures_of_the_savage_frontier,
    unlimited_adventures
};

inline constexpr std::array<GoldBoxGame, 7> gold_box_games{
    GoldBoxGame::pool_of_radiance,
    GoldBoxGame::curse_of_the_azure_bonds,
    GoldBoxGame::secret_of_the_silver_blades,
    GoldBoxGame::gateway_to_the_savage_frontier,
    GoldBoxGame::pools_of_darkness,
    GoldBoxGame::treasures_of_the_savage_frontier,
    GoldBoxGame::unlimited_adventures};

[[nodiscard]] constexpr std::string_view game_title(GoldBoxGame game) noexcept
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return "Pool of Radiance";
    case GoldBoxGame::curse_of_the_azure_bonds:
        return "Curse of the Azure Bonds";
    case GoldBoxGame::secret_of_the_silver_blades:
        return "Secret of the Silver Blades";
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return "Gateway to the Savage Frontier";
    case GoldBoxGame::pools_of_darkness:
        return "Pools of Darkness";
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return "Treasures of the Savage Frontier";
    case GoldBoxGame::unlimited_adventures:
        return "Unlimited Adventures";
    }
    return {};
}

// The folder the game installs into, as in /Users/edmond/GoldBox/<folder>.
[[nodiscard]] constexpr std::string_view game_folder(GoldBoxGame game) noexcept
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return "POOLRAD";
    case GoldBoxGame::curse_of_the_azure_bonds:
        return "CURSE";
    case GoldBoxGame::secret_of_the_silver_blades:
        return "SECRET";
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return "GATEWAY";
    case GoldBoxGame::pools_of_darkness:
        return "DARKNESS";
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return "TREASURE";
    case GoldBoxGame::unlimited_adventures:
        return "UA";
    }
    return {};
}

// Whether the game runs on the 256-color engine Pools of Darkness began and
// Treasures of the Savage Frontier kept: the same script machine, memory,
// records and pictures (docs/games/darkness.md, docs/games/treasure.md).
[[nodiscard]] constexpr bool vga_engine(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::pools_of_darkness ||
           game == GoldBoxGame::treasures_of_the_savage_frontier;
}

// The file of the game's item types: Gateway to the Savage Frontier names it
// ITEMS.1.
[[nodiscard]] constexpr std::string_view item_types_file(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::gateway_to_the_savage_frontier ? "ITEMS.1" : "ITEMS";
}

// The program that holds the game's item words: Gateway to the Savage
// Frontier and Pools of Darkness keep them in their GAME.EXE, the earlier
// games in START.EXE.
[[nodiscard]] constexpr std::string_view item_words_program(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::gateway_to_the_savage_frontier ||
                   game == GoldBoxGame::pools_of_darkness
               ? "GAME.EXE"
               : "START.EXE";
}

// The disk numbers a game's files carry, as in MON3CHA.DAX or HEAD2.DAX.
struct GameDisks
{
    // MONnCHA.DAX and MONnITM.DAX, and MONnSPC.DAX but on the disks listed after.
    std::vector<unsigned> monsters, monsters_without_effects;
    std::vector<unsigned> item_lists; // ITEMn.DAX
    // The HEADn.DAX and BODYn.DAX the player's portraits come from. Pool of
    // Radiance repeats them on every disk; Curse has none for the player, and
    // its HEAD and BODY files hold each disk's pictures for the scripts.
    std::vector<unsigned> player_portraits;
};

// Throws std::invalid_argument for a game whose files are not mapped yet.
[[nodiscard]] GameDisks game_disks(GoldBoxGame game);

} // namespace opengold

#endif
