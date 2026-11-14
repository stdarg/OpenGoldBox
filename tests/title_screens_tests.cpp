// Each game's title pictures, drawn from its own TITLE.DAX as its original's
// Intro unit shows them (docs/GAMES.md, phase 3 step 1.7).
#include "opengold/title_screens.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{

void check(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

opengold::Image title_picture(const std::filesystem::path &folder, std::uint8_t id)
{
    std::ifstream in(folder / "TITLE.DAX", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    for (const auto &record : opengold::decode_dax_archive(bytes).records)
    {
        if (record.id == id)
        {
            return opengold::decode_ega_picture(record.bytes).image;
        }
    }
    throw std::runtime_error("No TITLE.DAX picture " + std::to_string(id));
}

// Whether the screen's rectangle at (left, top) holds the picture, pixel for pixel.
bool shows(const opengold::Image &screen, const opengold::Image &picture, int left, int top)
{
    for (int y = 0; y < picture.height; ++y)
    {
        for (int x = 0; x < picture.width * 4; ++x)
        {
            if (screen.rgba[(static_cast<std::size_t>(top + y) * screen.width) * 4 + left * 4 +
                            x] != picture.rgba[static_cast<std::size_t>(y) * picture.width * 4 + x])
            {
                return false;
            }
        }
    }
    return true;
}

// Whether the screen's rows from `top` match the same rows of a full picture.
bool shows_rows(const opengold::Image &screen, const opengold::Image &full, int top, int bottom)
{
    const auto row = static_cast<std::ptrdiff_t>(screen.width) * 4;
    return std::equal(screen.rgba.begin() + top * row, screen.rgba.begin() + bottom * row,
                      full.rgba.begin() + top * row);
}

void pool_of_radiance_shows_its_two_pictures(const std::filesystem::path &folder)
{
    const auto title =
        opengold::load_title_screens(folder, opengold::GoldBoxGame::pool_of_radiance);
    check(title.screens.size() == 2 && !title.key_ends_all,
          "Pool of Radiance shows two pictures, a key ending one");
    for (std::size_t i = 0; i < 2; ++i)
    {
        const auto &screen = title.screens[i].screen;
        check(screen.width == 320 && screen.height == 200 && title.screens[i].seconds == 5 &&
                  !title.screens[i].sound,
              "Each of Pool of Radiance's pictures stays 5 seconds, silent");
        check(shows(screen, title_picture(folder, static_cast<std::uint8_t>(i + 1)), 0, 0),
              "Pool of Radiance's screen " + std::to_string(i + 1) + " is its picture");
    }
}

void curse_draws_its_name_and_hero_over_the_second(const std::filesystem::path &folder)
{
    const auto title =
        opengold::load_title_screens(folder, opengold::GoldBoxGame::curse_of_the_azure_bonds);
    check(title.screens.size() == 3 && !title.key_ends_all,
          "Curse shows three screens, a key ending one");
    check(title.screens[0].seconds == 5 && title.screens[1].seconds == 10 &&
              title.screens[2].seconds == 10,
          "Curse's screens stay 5, 10 and 10 seconds");
    check(shows(title.screens[0].screen, title_picture(folder, 1), 0, 0),
          "Curse's first screen is picture 1");
    const auto second = title_picture(folder, 2);
    check(shows(title.screens[1].screen, title_picture(folder, 3), 6 * 8, 11 * 8) &&
              shows_rows(title.screens[1].screen, second, 0, 11 * 8),
          "Curse's second screen draws picture 3 at column 6, row 11 over picture 2");
    check(shows(title.screens[2].screen, title_picture(folder, 4), 0, 11 * 8) &&
              shows_rows(title.screens[2].screen, second, 0, 11 * 8),
          "Curse's third screen draws picture 4 at row 11 over the second");
    check(!title.screens[1].sound && title.screens[2].sound == 13u,
          "Picture 4 starts Curse's title sound (13)");
}

void secret_redraws_its_backdrop_for_the_last(const std::filesystem::path &folder)
{
    const auto title =
        opengold::load_title_screens(folder, opengold::GoldBoxGame::secret_of_the_silver_blades);
    check(title.screens.size() == 4 && title.key_ends_all,
          "Secret shows four screens, and a key ends them all");
    const unsigned seconds[]{15, 50, 50, 60};
    for (std::size_t i = 0; i < 4; ++i)
    {
        check(title.screens[i].seconds == seconds[i], "Secret's screens stay 15, 50, 50 and 60 s");
    }
    const auto backdrop = title_picture(folder, 2);
    check(shows(title.screens[1].screen, backdrop, 0, 0), "Secret's second screen is picture 2");
    check(shows(title.screens[2].screen, title_picture(folder, 3), 2 * 8, 5 * 8),
          "Secret's third screen draws picture 3 at column 2, row 5");
    // Picture 2 is drawn again before picture 4, so picture 3 leaves no trace.
    check(shows(title.screens[3].screen, title_picture(folder, 4), 5 * 8, 5 * 8) &&
              shows_rows(title.screens[3].screen, backdrop, 0, 5 * 8) &&
              shows_rows(title.screens[3].screen, backdrop, 16 * 8, 200),
          "Secret's last screen draws picture 4 at column 5, row 5 over picture 2 again");
}

void later_games_have_no_title_yet(const std::filesystem::path &games)
{
    for (const auto &[game, folder] :
         {std::pair{opengold::GoldBoxGame::gateway_to_the_savage_frontier, "GATEWAY"},
          std::pair{opengold::GoldBoxGame::pools_of_darkness, "DARKNESS"},
          std::pair{opengold::GoldBoxGame::treasures_of_the_savage_frontier, "TREASURE"}})
    {
        check(opengold::load_title_screens(games / folder, game).screens.empty(),
              "A game whose pictures are not drawn yet shows no title");
    }
}

void a_folder_without_titles_is_refused(const std::filesystem::path &games)
{
    try
    {
        static_cast<void>(opengold::load_title_screens(games / "NOWHERE",
                                                       opengold::GoldBoxGame::pool_of_radiance));
    }
    catch (const std::exception &)
    {
        return;
    }
    throw std::runtime_error("A folder without TITLE.DAX must be refused");
}

} // namespace

int main()
{
    try
    {
        const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
        const std::filesystem::path root = games ? games : "";
        if (!games || !std::filesystem::exists(root / "POOLRAD" / "TITLE.DAX") ||
            !std::filesystem::exists(root / "CURSE" / "TITLE.DAX") ||
            !std::filesystem::exists(root / "SECRET" / "TITLE.DAX"))
        {
            std::cout << "Skipped: the games' TITLE.DAX files are not here.\n";
            return 0;
        }
        pool_of_radiance_shows_its_two_pictures(root / "POOLRAD");
        curse_draws_its_name_and_hero_over_the_second(root / "CURSE");
        secret_redraws_its_backdrop_for_the_last(root / "SECRET");
        later_games_have_no_title_yet(root);
        a_folder_without_titles_is_refused(root);
        std::cout << "Title screens passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
