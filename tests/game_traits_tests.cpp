// Every Gold Box game has a row of traits, and the rows hold what the games'
// files and reconstructions say (docs/games/*.md).
#include "opengold/game_traits.h"
#include "test_support.h"
#include <iostream>
#include <string>

namespace
{
using opengold::CampaignLoader;
using opengold::CombatIconFormat;
using opengold::GoldBoxGame;
using opengold::traits;
using opengold::test::check;

void every_game_has_a_row()
{
    for (const auto game : opengold::gold_box_games)
    {
        const auto &row = traits(game);
        const std::string title(opengold::game_title(game));
        check(row.gem_experience > 0 && row.picture_disks > 0,
              title + " has its loot and picture facts");
        check((row.loader == CampaignLoader::none) == (game == GoldBoxGame::unlimited_adventures),
              title + " has a loader unless it cannot be played yet");
        check(row.later_campaign.has_value() == (row.loader == CampaignLoader::later),
              title + " has a later campaign exactly when the later loader plays it");
        for (const auto &step : row.title.steps)
        {
            check(!step.draws.empty() && step.seconds > 0, title + "'s title steps draw and stay");
        }
    }
}

void rows_hold_the_games_facts()
{
    check(traits(GoldBoxGame::pool_of_radiance).loader == CampaignLoader::pool_of_radiance &&
              traits(GoldBoxGame::dark_queen_of_krynn).loader == CampaignLoader::dark_queen,
          "Pool of Radiance and The Dark Queen of Krynn have loaders of their own");
    const auto &curse = traits(GoldBoxGame::curse_of_the_azure_bonds).later_campaign.value();
    check(curse.opening_script == 1 && curse.opening_disk == 2 && curse.root_disk == 1 &&
              curse.wilderness,
          "Curse opens with script 1 of disk 2 and fights its wilderness at the root");
    const auto &gateway =
        traits(GoldBoxGame::gateway_to_the_savage_frontier).later_campaign.value();
    check(gateway.root_disk == 6 && !gateway.wilderness, "Gateway's overland script is on disk 6");
    check(traits(GoldBoxGame::secret_of_the_silver_blades).won_cell ==
                  opengold::por::EclAddress{0x4CD9} &&
              traits(GoldBoxGame::treasures_of_the_savage_frontier).won_cell ==
                  opengold::por::EclAddress{0x25C} &&
              traits(GoldBoxGame::champions_of_krynn).won_cell ==
                  opengold::por::EclAddress{0x4C3A} &&
              !traits(GoldBoxGame::pool_of_radiance).won_cell,
          "The games with no ending of their own win on their cell");
    check(traits(GoldBoxGame::secret_of_the_silver_blades).title_music_piece == 2U &&
              traits(GoldBoxGame::death_knights_of_krynn).title_music_piece == 1U &&
              traits(GoldBoxGame::countdown_to_doomsday).title_music_piece == 2U &&
              !traits(GoldBoxGame::matrix_cubed).title_music_piece,
          "Title music pieces");
    check(traits(GoldBoxGame::gateway_to_the_savage_frontier).combat_icons ==
                  CombatIconFormat::gateway &&
              traits(GoldBoxGame::matrix_cubed).combat_icons == CombatIconFormat::vga &&
              traits(GoldBoxGame::death_knights_of_krynn).combat_icons == CombatIconFormat::ega,
          "Combat icon formats");
    check(traits(GoldBoxGame::dark_queen_of_krynn).gem_experience == 2 &&
              traits(GoldBoxGame::pools_of_darkness).gem_experience == 5,
          "A gem is worth 2 experience in The Dark Queen of Krynn and 5 elsewhere");
    check(traits(GoldBoxGame::death_knights_of_krynn).picture_disks == 7 &&
              traits(GoldBoxGame::countdown_to_doomsday).picture_disks == 9 &&
              traits(GoldBoxGame::curse_of_the_azure_bonds).picture_disks == 6,
          "Pictures run to PIC7 in Death Knights, PIC9 in Buck Rogers and PIC6 elsewhere");
    check(traits(GoldBoxGame::pool_of_radiance).title.steps.size() == 2 &&
              traits(GoldBoxGame::gateway_to_the_savage_frontier).title.music_screen == 1 &&
              traits(GoldBoxGame::pools_of_darkness).title.opens_with_film &&
              traits(GoldBoxGame::champions_of_krynn).title.steps.empty(),
          "Title scripts");
}

} // namespace

int main()
{
    try
    {
        every_game_has_a_row();
        rows_hold_the_games_facts();
        std::cout << "Game traits passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
