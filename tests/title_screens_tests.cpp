// Each game's title pictures, drawn from its own TITLE.DAX as its original's
// Intro unit shows them (docs/GAMES.md, phase 3 step 1.7).
#include "opengold/title_screens.h"
#include "opengold/gateway_formats.h"
#include "opengold/vace_film.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

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

// Where a key leads from each screen: the next screen's index, the number of
// screens to end the title.
bool keys_lead_to(const opengold::TitleScreens &title, const std::vector<std::size_t> &after_keys)
{
    return std::ranges::equal(title.screens, after_keys, {},
                              [](const opengold::TitleScreen &screen)
                              {
                                  return screen.after_key.value_or(0xFFFF);
                              });
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
    check(title.screens.size() == 2 && keys_lead_to(title, {1, 2}),
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
    check(title.screens.size() == 3 && keys_lead_to(title, {1, 2, 3}),
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
    check(title.screens.size() == 4 && keys_lead_to(title, {4, 4, 4, 4}),
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

// Gateway's pictures have its own color header; each is shown alone on a
// cleared screen.
void gateway_shows_five_pictures_alone(const std::filesystem::path &folder)
{
    const auto title =
        opengold::load_title_screens(folder, opengold::GoldBoxGame::gateway_to_the_savage_frontier);
    check(title.screens.size() == 5 && keys_lead_to(title, {5, 2, 3, 4, 5}) &&
              title.music_screen == 1,
          "Gateway shows five screens: a key on the first ends them all, a later one passes "
          "one; its music starts with the second");
    const unsigned seconds[]{3, 3, 10, 20, 20};
    std::ifstream in(folder / "TITLE.DAX", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    const auto archive = opengold::decode_dax_archive(bytes);
    for (std::size_t i = 0; i < 5; ++i)
    {
        check(title.screens[i].seconds == seconds[i],
              "Gateway's screens stay 3, 3, 10, 20 and 20 s");
        const auto record = std::ranges::find(archive.records, static_cast<std::uint8_t>(i + 1),
                                              &opengold::DaxRecord::id);
        check(record != archive.records.end(), "Gateway's TITLE.DAX holds pictures 1 to 5");
        const auto picture = opengold::gateway::decode_picture(record->bytes);
        check(picture &&
                  shows(title.screens[i].screen, opengold::gateway::picture_image(*picture), 0, 0),
              "Gateway's screen " + std::to_string(i + 1) + " is its picture, in its own colors");
    }
}

// A 256-color game's TITLE.DAX picture in the colors it sets.
opengold::Image vga_title_picture(const std::filesystem::path &folder, std::uint8_t id)
{
    std::ifstream in(folder / "TITLE.DAX", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    const auto archive = opengold::decode_dax_archive(bytes);
    const auto record = std::ranges::find(archive.records, id, &opengold::DaxRecord::id);
    check(record != archive.records.end(), "No TITLE.DAX picture " + std::to_string(id));
    const auto picture = opengold::decode_vga_picture(record->bytes);
    check(picture && picture->frames.size() == 1, "A TITLE.DAX picture is a VGA picture");
    return opengold::vga_rgba(picture->frames.front(), opengold::vga_color_table(picture->palette));
}

std::vector<std::uint8_t> file_bytes(const std::filesystem::path &file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// Pools of Darkness opens with its film, INTRO, as VACE plays it before the
// game: its 133 frames each for its time, a key ending it once the title card
// has passed, and its song P1.SMS. Then pictures 1 and 2 for 5 seconds and 3
// for 20, a key on 1 or 2 going on to 3, with the title music from picture 1.
void darkness_plays_its_film_then_three_pictures(const std::filesystem::path &folder)
{
    const auto title =
        opengold::load_title_screens(folder, opengold::GoldBoxGame::pools_of_darkness);
    static constexpr std::size_t film = 133;
    check(title.screens.size() == film + 3,
          "Pools of Darkness shows its film's 133 frames and three pictures");
    const auto opening = opengold::decode_vace_film(file_bytes(folder / "INTRO.ACF"),
                                                    file_bytes(folder / "INTRO.BMA"));
    check(opening && opening->size() == film, "INTRO decodes");
    for (const std::size_t frame : {std::size_t{0}, std::size_t{45}, film - 1})
    {
        const auto &shown = (*opening)[frame];
        const opengold::VgaImage pixels{320, 200, shown.pixels};
        check(shows(title.screens[frame].screen, opengold::vga_rgba(pixels, shown.palette), 0, 0),
              "The title's screen " + std::to_string(frame) +
                  " is the film's frame, in its colors");
        check(title.screens[frame].seconds == shown.hold_milliseconds / 1000.0,
              "A film frame stays its time");
    }
    check(!title.screens[0].after_key &&
              std::all_of(title.screens.begin() + 1, title.screens.begin() + film,
                          [](const opengold::TitleScreen &screen)
                          {
                              return screen.after_key == film;
                          }),
          "A key is ignored on the film's title card, then ends the film");
    check(title.film_song && title.film_song->file.filename() == "P1.SMS" &&
              title.film_song->number == 0,
          "The film plays P1.SMS's song 0");
    check(title.music_screen == film, "The title music starts with picture 1");
    const unsigned seconds[]{5, 5, 20};
    const std::size_t after_key[]{film + 2, film + 2, film + 3};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const auto &screen = title.screens[film + i];
        check(screen.seconds == seconds[i] && screen.after_key == after_key[i],
              "Pictures 1 and 2 stay 5 s and a key goes on to 3, which stays 20 s");
        check(
            shows(screen.screen, vga_title_picture(folder, static_cast<std::uint8_t>(i + 1)), 0, 0),
            "Pools of Darkness' picture " + std::to_string(i + 1) + " is in its own colors");
    }
}

// Treasures shows pictures 1 to 3 for 25 seconds, the VGA's picture 4 for 50
// and the credits, picture 6, for 50; a key passes one, and the title music
// plays from the first.
void treasures_shows_five_pictures(const std::filesystem::path &folder)
{
    const auto title = opengold::load_title_screens(
        folder, opengold::GoldBoxGame::treasures_of_the_savage_frontier);
    check(title.screens.size() == 5 && keys_lead_to(title, {1, 2, 3, 4, 5}) &&
              title.music_screen == 0 && !title.film_song,
          "Treasures shows five screens, a key passing one, its music from the first");
    const unsigned seconds[]{25, 25, 25, 50, 50};
    const std::uint8_t pictures[]{1, 2, 3, 4, 6};
    for (std::size_t i = 0; i < 5; ++i)
    {
        check(title.screens[i].seconds == seconds[i],
              "Treasures' screens stay 25, 25, 25, 50 and 50 s");
        check(shows(title.screens[i].screen, vga_title_picture(folder, pictures[i]), 0, 0),
              "Treasures' screen " + std::to_string(i + 1) + " is picture " +
                  std::to_string(pictures[i]) + " in its own colors");
    }
}

void unlimited_adventures_has_no_title(const std::filesystem::path &games)
{
    check(opengold::load_title_screens(games / "UA", opengold::GoldBoxGame::unlimited_adventures)
              .screens.empty(),
          "Unlimited Adventures shows no title pictures");
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
        if (std::filesystem::exists(root / "GATEWAY" / "TITLE.DAX"))
        {
            gateway_shows_five_pictures_alone(root / "GATEWAY");
        }
        if (std::filesystem::exists(root / "DARKNESS" / "INTRO.ACF"))
        {
            darkness_plays_its_film_then_three_pictures(root / "DARKNESS");
        }
        if (std::filesystem::exists(root / "TREASURE" / "TITLE.DAX"))
        {
            treasures_shows_five_pictures(root / "TREASURE");
        }
        unlimited_adventures_has_no_title(root);
        a_folder_without_titles_is_refused(root);
        std::cout << "Title screens passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
