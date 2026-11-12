#ifndef OPENGOLD_GOLD_BOX_GAME_H
#define OPENGOLD_GOLD_BOX_GAME_H

#include <array>
#include <cstdint>
#include <string_view>

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

} // namespace opengold

#endif
