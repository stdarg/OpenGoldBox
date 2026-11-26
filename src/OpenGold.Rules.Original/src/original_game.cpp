#include "opengold/original_game.h"
#include "history_entries.h"

#include <string>
#include <string_view>

namespace opengold::original
{
namespace
{
constexpr std::string_view pool_edition = "pool-of-radiance-1.3";
constexpr std::string_view curse_edition = "curse-of-the-azure-bonds";
constexpr std::string_view secret_edition = "secret-of-the-silver-blades";
constexpr std::string_view gateway_edition = "gateway-to-the-savage-frontier";
constexpr std::string_view champions_edition = "champions-of-krynn";
constexpr std::string_view dark_queen_edition = "dark-queen-of-krynn";
constexpr std::string_view death_knights_edition = "death-knights-of-krynn";
constexpr std::string_view ua_edition = "unlimited-adventures";
constexpr std::string_view darkness_edition = "pools-of-darkness";
constexpr std::string_view treasures_edition = "treasures-of-the-savage-frontier";
constexpr std::string_view countdown_edition = "countdown-to-doomsday";
constexpr std::string_view matrix_edition = "matrix-cubed";
} // namespace

rules::Identity identity(GoldBoxGame game)
{
    const auto edition = game == GoldBoxGame::dark_queen_of_krynn           ? dark_queen_edition
                         : game == GoldBoxGame::death_knights_of_krynn      ? death_knights_edition
                         : game == GoldBoxGame::champions_of_krynn          ? champions_edition
                         : game == GoldBoxGame::curse_of_the_azure_bonds    ? curse_edition
                         : game == GoldBoxGame::secret_of_the_silver_blades ? secret_edition
                         : game == GoldBoxGame::gateway_to_the_savage_frontier   ? gateway_edition
                         : game == GoldBoxGame::unlimited_adventures             ? ua_edition
                         : game == GoldBoxGame::pools_of_darkness                ? darkness_edition
                         : game == GoldBoxGame::treasures_of_the_savage_frontier ? treasures_edition
                         : game == GoldBoxGame::countdown_to_doomsday            ? countdown_edition
                         : game == GoldBoxGame::matrix_cubed                     ? matrix_edition
                                                                                 : pool_edition;
    return {"opengold.original", "0.1.0", std::string(edition)};
}

GoldBoxGame game_of(const rules::Identity &identity)
{
    if (identity.content == dark_queen_edition)
    {
        return GoldBoxGame::dark_queen_of_krynn;
    }
    if (identity.content == death_knights_edition)
    {
        return GoldBoxGame::death_knights_of_krynn;
    }
    if (identity.content == champions_edition)
    {
        return GoldBoxGame::champions_of_krynn;
    }
    if (identity.content == curse_edition)
    {
        return GoldBoxGame::curse_of_the_azure_bonds;
    }
    if (identity.content == gateway_edition)
    {
        return GoldBoxGame::gateway_to_the_savage_frontier;
    }
    if (identity.content == ua_edition)
    {
        return GoldBoxGame::unlimited_adventures;
    }
    if (identity.content == darkness_edition)
    {
        return GoldBoxGame::pools_of_darkness;
    }
    if (identity.content == treasures_edition)
    {
        return GoldBoxGame::treasures_of_the_savage_frontier;
    }
    if (identity.content == countdown_edition)
    {
        return GoldBoxGame::countdown_to_doomsday;
    }
    if (identity.content == matrix_edition)
    {
        return GoldBoxGame::matrix_cubed;
    }
    return identity.content == secret_edition ? GoldBoxGame::secret_of_the_silver_blades
                                              : GoldBoxGame::pool_of_radiance;
}

rules::AdvancementChoice romance_refused()
{
    rules::AdvancementChoice change;
    change.rules_data = std::string(history::romance_refused_entry);
    return change;
}
} // namespace opengold::original
