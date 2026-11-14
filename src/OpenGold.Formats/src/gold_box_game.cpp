#include "opengold/gold_box_game.h"

#include <stdexcept>
#include <string>

namespace opengold
{

GameDisks game_disks(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return {
            {1, 2, 3, 4, 5, 6, 7, 8}, {1, 3}, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}};
    case GoldBoxGame::curse_of_the_azure_bonds:
        return {{1, 2, 3, 4, 5, 6}, {}, {1, 2, 3, 4, 5, 6}, {}};
    case GoldBoxGame::secret_of_the_silver_blades:
        // One creature file serves every disk.
        return {{1}, {}, {1, 2, 3}, {}};
    case GoldBoxGame::gateway_to_the_savage_frontier:
        // Its HEAD and BODY files have no disk number.
        return {{2, 3, 4, 5, 6}, {}, {2, 3, 4, 5, 6}, {}};
    default:
        throw std::invalid_argument("The files of " + std::string(game_title(game)) +
                                    " are not mapped yet");
    }
}

} // namespace opengold
