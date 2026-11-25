#ifndef OPENGOLD_GOLD_BOX_GAME_H
#define OPENGOLD_GOLD_BOX_GAME_H

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace opengold
{

// Stable campaign-save IDs. Append new games without renumbering old ones.
// Native is 9, followed by the Buck Rogers games at 10 and 11.
enum class GoldBoxGame : std::uint8_t
{
    pool_of_radiance,
    curse_of_the_azure_bonds,
    secret_of_the_silver_blades,
    gateway_to_the_savage_frontier,
    pools_of_darkness,
    treasures_of_the_savage_frontier,
    unlimited_adventures,
    champions_of_krynn,
    death_knights_of_krynn,
    dark_queen_of_krynn = 9,
    countdown_to_doomsday = 10,
    matrix_cubed = 11
};

inline constexpr std::array<GoldBoxGame, 12> gold_box_games{
    GoldBoxGame::pool_of_radiance,
    GoldBoxGame::curse_of_the_azure_bonds,
    GoldBoxGame::secret_of_the_silver_blades,
    GoldBoxGame::gateway_to_the_savage_frontier,
    GoldBoxGame::pools_of_darkness,
    GoldBoxGame::treasures_of_the_savage_frontier,
    GoldBoxGame::unlimited_adventures,
    GoldBoxGame::champions_of_krynn,
    GoldBoxGame::death_knights_of_krynn,
    GoldBoxGame::dark_queen_of_krynn,
    GoldBoxGame::countdown_to_doomsday,
    GoldBoxGame::matrix_cubed};

// The game a save's number names, or none for a number no game has.
[[nodiscard]] constexpr std::optional<GoldBoxGame> game_numbered(unsigned number) noexcept
{
    for (const auto game : gold_box_games)
    {
        if (static_cast<unsigned>(game) == number)
        {
            return game;
        }
    }
    return std::nullopt;
}

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
    case GoldBoxGame::champions_of_krynn:
        return "Champions of Krynn";
    case GoldBoxGame::death_knights_of_krynn:
        return "Death Knights of Krynn";
    case GoldBoxGame::dark_queen_of_krynn:
        return "The Dark Queen of Krynn";
    case GoldBoxGame::countdown_to_doomsday:
        return "Buck Rogers: Countdown to Doomsday";
    case GoldBoxGame::matrix_cubed:
        return "Buck Rogers: Matrix Cubed";
    }
    return {};
}

// The folder the game installs into, as in ~/GoldBox/game_files/<folder>.
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
    case GoldBoxGame::champions_of_krynn:
        return "COK";
    case GoldBoxGame::death_knights_of_krynn:
        return "DKK";
    case GoldBoxGame::dark_queen_of_krynn:
        return "DQK";
    case GoldBoxGame::countdown_to_doomsday:
        return "buck-rogers-countdown-to-doomsday";
    case GoldBoxGame::matrix_cubed:
        return "buck-rogers-matrix-cubed";
    }
    return {};
}

// Whether the game runs on the 256-color engine Pools of Darkness began and
// Treasures of the Savage Frontier kept: the same script machine, memory,
// records and pictures (docs/games/darkness.md, docs/games/treasure.md).
[[nodiscard]] constexpr bool vga_engine(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::pools_of_darkness ||
           game == GoldBoxGame::treasures_of_the_savage_frontier ||
           game == GoldBoxGame::dark_queen_of_krynn;
}

// Whether the game is one of the three Krynn games, which share the Dragonlance
// races, classes, steel coins and deities (docs/KRYNN.md).
[[nodiscard]] constexpr bool krynn_game(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::champions_of_krynn || game == GoldBoxGame::death_knights_of_krynn ||
           game == GoldBoxGame::dark_queen_of_krynn;
}

// Whether the game is one of the two Buck Rogers games, which run the same
// program with different data (docs/BUCK-ROGERS.md).
[[nodiscard]] constexpr bool is_buck_rogers(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::countdown_to_doomsday || game == GoldBoxGame::matrix_cubed;
}

// Whether the game's pictures, animations, sprites, icons, tiles and walls are
// the 256-color layout Pools of Darkness began: its engine's, and the Buck
// Rogers games', which keep the Curse engine's scripts and memory
// (docs/games/countdown.md, "The census").
[[nodiscard]] constexpr bool vga_art(GoldBoxGame game) noexcept
{
    return vga_engine(game) || is_buck_rogers(game);
}

// Death Knights introduced these script commands while retaining EGA art and
// the earlier memory zones. The VGA games inherit the command variants.
[[nodiscard]] constexpr bool later_ecl_commands(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::death_knights_of_krynn || vga_engine(game);
}

// The file of the game's item types: Gateway to the Savage Frontier names it
// ITEMS.1.
[[nodiscard]] constexpr std::string_view item_types_file(GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return "ITEMS.DAT";
    }
    return game == GoldBoxGame::gateway_to_the_savage_frontier ? "ITEMS.1" : "ITEMS";
}

// The program that holds the game's item words: Gateway to the Savage
// Frontier and the 256-color games keep them in their GAME.EXE, the earlier
// games in START.EXE.
[[nodiscard]] constexpr std::string_view item_words_program(GoldBoxGame game) noexcept
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return "DQK.EXE";
    }
    return game == GoldBoxGame::gateway_to_the_savage_frontier || vga_engine(game) ? "GAME.EXE"
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
