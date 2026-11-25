#include "opengold/game_traits.h"
#include <array>

namespace opengold
{
namespace
{

using por::EclAddress;

constexpr auto ends_title = TitleKey::ends_title;

// The campaigns of one disk: a new game runs script 16 (its GameLoop), New
// Phlan's docks in Pools of Darkness, the waking in Llorkh's fortress in
// Treasures. The Buck Rogers games too (eclvm.pas START_ECL_SCRIPT).
constexpr LaterCampaign one_disk_campaign{16, 1, 1, false};

// Secret of the Silver Blades' cell for Eldamar's soul: script 99 sets it to 1
// as the gem holding the soul shatters; New Verdigris's mayor sets it to 255
// once he has thanked the party (a cell of Secret's own, not translated).
constexpr EclAddress secret_soul_freed{0x4CD9};
// Treasures of the Savage Frontier's cell for Freezefire: script 38 sets it
// to 1 as the dragon dies in its lair, before the party is carried to
// Icewolf and honored in Waterdeep.
constexpr EclAddress treasures_freezefire_slain{0x25C};
// Champions of Krynn's ECL3:98 8E25h, after Myrtani, the three messengers and
// the honors.
constexpr EclAddress champions_honored{0x4C3A};

// The worth of a gem in experience: 5 in every game but The Dark Queen of
// Krynn, where it is 2.
constexpr unsigned gem_experience = 5, dark_queen_gem_experience = 2;

// Pictures run to PIC6 in most games, to PIC7 in Death Knights and to PIC9 in
// the Buck Rogers games.
constexpr unsigned six_picture_disks = 6;

static_assert(static_cast<std::size_t>(GoldBoxGame::matrix_cubed) + 1 == gold_box_games.size(),
              "the traits table is indexed by GoldBoxGame");

std::array<GameTraits, gold_box_games.size()> make_traits()
{
    std::array<GameTraits, gold_box_games.size()> table{};
    const auto row = [&table](GoldBoxGame of) -> GameTraits &
    {
        return table[static_cast<std::size_t>(of)];
    };
    row(GoldBoxGame::pool_of_radiance) = {
        .loader = CampaignLoader::pool_of_radiance,
        .title = {.steps{{{{1, 0, 0}}, 5, {}}, {{{2, 0, 0}}, 5, {}}}},
        .combat_icons = CombatIconFormat::ega,
        .gem_experience = gem_experience,
        .picture_disks = six_picture_disks};
    {
        // Picture 4 starts the sound effect sndIntro (13).
        constexpr unsigned title_sound = 13;
        row(GoldBoxGame::curse_of_the_azure_bonds) = {
            .loader = CampaignLoader::later,
            .later_campaign = LaterCampaign{1, 2, 1, true},
            .title = {.steps{{{{1, 0, 0}}, 5, {}},
                             {{{2, 0, 0}, {3, 6, 11}}, 10, {}},
                             {{{4, 0, 11}}, 10, title_sound}}},
            .combat_icons = CombatIconFormat::ega,
            .gem_experience = gem_experience,
            .picture_disks = six_picture_disks};
    }
    row(GoldBoxGame::secret_of_the_silver_blades) = {
        .loader = CampaignLoader::later,
        // The opening's vision of the valley has no map.
        .later_campaign = LaterCampaign{17, 2, 2, false},
        .won_cell = secret_soul_freed,
        .title_music_piece = 2,
        .title = {.steps{{{{1, 0, 0}}, 15, {}, ends_title},
                         {{{2, 0, 0}}, 50, {}, ends_title},
                         {{{3, 2, 5}}, 50, {}, ends_title},
                         {{{2, 0, 0}, {4, 5, 5}}, 60, {}, ends_title}}},
        .combat_icons = CombatIconFormat::ega,
        .gem_experience = gem_experience,
        .picture_disks = six_picture_disks};
    row(GoldBoxGame::gateway_to_the_savage_frontier) = {
        .loader = CampaignLoader::later,
        // A new game runs script 1 of disk 2 (Yartar); the overland's script
        // is on disk 6.
        .later_campaign = LaterCampaign{1, 2, 6, false},
        .title_music_piece = 1,
        // The last picture stays 20 seconds, or until the title music ends.
        .title = {.steps{{{{1, 0, 0}}, 3, {}, ends_title},
                         {{{2, 0, 0}}, 3, {}},
                         {{{3, 0, 0}}, 10, {}},
                         {{{4, 0, 0}}, 20, {}},
                         {{{5, 0, 0}}, 20, {}}},
                  .clears_between = true,
                  .music_screen = 1},
        .combat_icons = CombatIconFormat::gateway,
        .gem_experience = gem_experience,
        .picture_disks = six_picture_disks};
    row(GoldBoxGame::pools_of_darkness) = {
        .loader = CampaignLoader::later,
        .later_campaign = one_disk_campaign,
        .title_music_piece = 1,
        // The third picture is the title menu's backdrop.
        .title = {.steps{{{{1, 0, 0}}, 5, {}, TitleKey::skips_to_last},
                         {{{2, 0, 0}}, 5, {}, TitleKey::skips_to_last},
                         {{{3, 0, 0}}, 20, {}}},
                  .clears_between = true,
                  .opens_with_film = true},
        .combat_icons = CombatIconFormat::vga,
        .gem_experience = gem_experience,
        .picture_disks = six_picture_disks};
    row(GoldBoxGame::treasures_of_the_savage_frontier) = {
        .loader = CampaignLoader::later,
        .later_campaign = one_disk_campaign,
        .won_cell = treasures_freezefire_slain,
        .title_music_piece = 1,
        // Picture 4 is the VGA's (5 the EGA's); picture 6 holds the credits.
        .title = {.steps{{{{1, 0, 0}}, 25, {}},
                         {{{2, 0, 0}}, 25, {}},
                         {{{3, 0, 0}}, 25, {}},
                         {{{4, 0, 0}}, 50, {}},
                         {{{6, 0, 0}}, 50, {}}},
                  .clears_between = true},
        .combat_icons = CombatIconFormat::vga,
        .gem_experience = gem_experience,
        .picture_disks = six_picture_disks};
    // Unlimited Adventures cannot be played yet: no loader.
    row(GoldBoxGame::unlimited_adventures) = {.combat_icons = CombatIconFormat::ega,
                                              .gem_experience = gem_experience,
                                              .picture_disks = six_picture_disks};
    row(GoldBoxGame::champions_of_krynn) = {.loader = CampaignLoader::later,
                                            .later_campaign = LaterCampaign{36, 1, 1, false},
                                            .won_cell = champions_honored,
                                            .title_music_piece = 1,
                                            .combat_icons = CombatIconFormat::ega,
                                            .gem_experience = gem_experience,
                                            .picture_disks = six_picture_disks};
    row(GoldBoxGame::death_knights_of_krynn) = {
        .loader = CampaignLoader::later,
        .later_campaign = LaterCampaign{16, 1, 1, false},
        .title_music_piece = 1,
        .title = {.steps{{{{1, 0, 0}}, 5, {}, ends_title},
                         {{{2, 0, 0}}, 5, {}, ends_title},
                         {{{2, 0, 0}, {3, 0, 11}}, 5, {}, ends_title},
                         {{{2, 0, 0}, {4, 0, 11}}, 20, {}, ends_title}},
                  .clears_between = true},
        .combat_icons = CombatIconFormat::ega,
        .gem_experience = gem_experience,
        .picture_disks = 7};
    row(GoldBoxGame::dark_queen_of_krynn) = {.loader = CampaignLoader::dark_queen,
                                             .combat_icons = CombatIconFormat::ega,
                                             .gem_experience = dark_queen_gem_experience,
                                             .picture_disks = six_picture_disks};
    // Countdown plays its bank, MUSIC.DAX record 2, over its introduction
    // alone: no script starts music (intro.pas FinishIntroduction).
    row(GoldBoxGame::countdown_to_doomsday) = {.loader = CampaignLoader::later,
                                               .later_campaign = one_disk_campaign,
                                               .title_music_piece = 2,
                                               .combat_icons = CombatIconFormat::vga,
                                               .gem_experience = gem_experience,
                                               .picture_disks = 9};
    row(GoldBoxGame::matrix_cubed) = {.loader = CampaignLoader::later,
                                      .later_campaign = one_disk_campaign,
                                      .combat_icons = CombatIconFormat::vga,
                                      .gem_experience = gem_experience,
                                      .picture_disks = 9};
    return table;
}

} // namespace

const GameTraits &traits(GoldBoxGame game)
{
    static const auto table = make_traits();
    return table[static_cast<std::size_t>(game)];
}

} // namespace opengold
