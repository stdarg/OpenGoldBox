#include "opengold/title_screens.h"

#include "area_loading.h"
#include "opengold/gateway_formats.h"
#include "opengold/vace_film.h"
#include "opengold/vga_picture.h"
#include "opengold/ua_view.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace opengold
{
namespace
{

constexpr int screen_width = 320, screen_height = 200, cell = 8;

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

// Each game's Intro.Introduction with its full-introduction flag on, without
// the credits that follow the pictures.
TitleScript title_script(GoldBoxGame game)
{
    constexpr auto ends_title = TitleKey::ends_title;
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return {.steps{{{{1, 0, 0}}, 5, {}}, {{{2, 0, 0}}, 5, {}}}};
    case GoldBoxGame::curse_of_the_azure_bonds:
    {
        // Picture 4 starts the sound effect sndIntro (13).
        constexpr unsigned title_sound = 13;
        return {.steps{{{{1, 0, 0}}, 5, {}},
                       {{{2, 0, 0}, {3, 6, 11}}, 10, {}},
                       {{{4, 0, 11}}, 10, title_sound}}};
    }
    case GoldBoxGame::death_knights_of_krynn:
        return {.steps{{{{1, 0, 0}}, 5, {}, ends_title},
                       {{{2, 0, 0}}, 5, {}, ends_title},
                       {{{2, 0, 0}, {3, 0, 11}}, 5, {}, ends_title},
                       {{{2, 0, 0}, {4, 0, 11}}, 20, {}, ends_title}},
                .clears_between = true};
    case GoldBoxGame::secret_of_the_silver_blades:
        return {.steps{{{{1, 0, 0}}, 15, {}, ends_title},
                       {{{2, 0, 0}}, 50, {}, ends_title},
                       {{{3, 2, 5}}, 50, {}, ends_title},
                       {{{2, 0, 0}, {4, 5, 5}}, 60, {}, ends_title}}};
    case GoldBoxGame::gateway_to_the_savage_frontier:
        // The last picture stays 20 seconds, or until the title music ends.
        return {.steps{{{{1, 0, 0}}, 3, {}, ends_title},
                       {{{2, 0, 0}}, 3, {}},
                       {{{3, 0, 0}}, 10, {}},
                       {{{4, 0, 0}}, 20, {}},
                       {{{5, 0, 0}}, 20, {}}},
                .clears_between = true,
                .music_screen = 1};
    case GoldBoxGame::pools_of_darkness:
        // The third picture is the title menu's backdrop.
        return {.steps{{{{1, 0, 0}}, 5, {}, TitleKey::skips_to_last},
                       {{{2, 0, 0}}, 5, {}, TitleKey::skips_to_last},
                       {{{3, 0, 0}}, 20, {}}},
                .clears_between = true,
                .opens_with_film = true};
    case GoldBoxGame::treasures_of_the_savage_frontier:
        // Picture 4 is the VGA's (5 the EGA's); picture 6 holds the credits.
        return {.steps{{{{1, 0, 0}}, 25, {}},
                       {{{2, 0, 0}}, 25, {}},
                       {{{3, 0, 0}}, 25, {}},
                       {{{4, 0, 0}}, 50, {}},
                       {{{6, 0, 0}}, 50, {}}},
                .clears_between = true};
    default:
        return {};
    }
}

Image black_screen()
{
    Image screen;
    screen.width = screen_width;
    screen.height = screen_height;
    screen.rgba.assign(std::size_t{screen_width} * screen_height * 4, 0);
    for (std::size_t alpha = 3; alpha < screen.rgba.size(); alpha += 4)
    {
        screen.rgba[alpha] = 255;
    }
    return screen;
}

void draw(Image &screen, const Image &picture, int left, int top)
{
    const int width = std::min<int>(picture.width, screen_width - left);
    const int height = std::min<int>(picture.height, screen_height - top);
    for (int y = 0; y < height; ++y)
    {
        const auto from = picture.rgba.begin() + std::ptrdiff_t{4} * y * picture.width;
        const auto to = screen.rgba.begin() + std::ptrdiff_t{4} * ((top + y) * screen_width + left);
        std::copy_n(from, std::ptrdiff_t{4} * width, to);
    }
}

// A TITLE.DAX record's picture in its own colors: the 16-color games' EGA
// picture, Gateway's own layout, or the 256-color games' VGA picture.
std::optional<Image> title_picture(const DaxRecord &record, GoldBoxGame game)
{
    if (game == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        const auto picture = gateway::decode_picture(record.bytes);
        if (!picture || picture->frames.empty())
        {
            return std::nullopt;
        }
        return gateway::picture_image(*picture);
    }
    if (game == GoldBoxGame::pools_of_darkness ||
        game == GoldBoxGame::treasures_of_the_savage_frontier)
    {
        const auto picture = decode_vga_picture(record.bytes);
        if (!picture || picture->frames.empty())
        {
            return std::nullopt;
        }
        return vga_rgba(picture->frames.front(), vga_color_table(picture->palette));
    }
    auto picture = decode_ega_picture(record.bytes);
    if (!picture)
    {
        return std::nullopt;
    }
    return std::move(picture.image);
}

std::map<std::uint8_t, Image> title_pictures(const std::filesystem::path &folder, GoldBoxGame game)
{
    const auto archive = decode_dax_archive(
        por::detail::read_archive(por::detail::resolve_archive(folder, "TITLE.DAX")));
    if (!archive)
    {
        throw std::runtime_error("Invalid TITLE.DAX");
    }
    std::map<std::uint8_t, Image> pictures;
    for (const auto &record : archive.records)
    {
        if (auto picture = title_picture(record, game))
        {
            pictures.emplace(record.id, std::move(*picture));
        }
    }
    return pictures;
}

// One of Pools of Darkness' films (INTRO or FINAL) as the original's VACE
// plays it: each frame a screen for its time. A key ends the film once a frame
// lets it (after the opening's title card), going on past the film.
void add_film(const std::filesystem::path &folder, const std::string &name, TitleScreens &title)
{
    const auto film = decode_vace_film(
        por::detail::read_archive(por::detail::resolve_archive(folder, name + ".ACF")),
        por::detail::read_archive(por::detail::resolve_archive(folder, name + ".BMA")));
    if (!film || film->empty())
    {
        throw std::runtime_error("Invalid film " + name);
    }
    for (const auto &frame : *film)
    {
        const VgaImage pixels{vace_film_width, vace_film_height, frame.pixels};
        title.screens.push_back({vga_rgba(pixels, frame.palette), frame.hold_milliseconds / 1000.0,
                                 std::nullopt,
                                 frame.key_ends_film ? std::optional(film->size()) : std::nullopt});
        if (frame.starts_song)
        {
            title.film_song =
                TitleSong{por::detail::resolve_archive(folder, frame.starts_song->file),
                          frame.starts_song->number};
        }
    }
}

// The screen a key on a step's screen leads to; `screen` is its own, `last`
// the title's last.
std::size_t screen_after_key(TitleKey key, std::size_t screen, std::size_t last)
{
    switch (key)
    {
    case TitleKey::ends_title:
        return last + 1;
    case TitleKey::skips_to_last:
        return last;
    case TitleKey::passes_screen:
        break;
    }
    return screen + 1;
}

} // namespace

TitleScreens load_title_screens(const std::filesystem::path &folder, GoldBoxGame game)
{
    const auto script = title_script(game);
    if (script.steps.empty())
    {
        return {};
    }
    const auto pictures = title_pictures(folder, game);
    TitleScreens title;
    if (script.opens_with_film)
    {
        add_film(folder, "INTRO", title);
    }
    const auto first_picture = title.screens.size();
    const auto last_screen = first_picture + script.steps.size() - 1;
    title.music_screen = first_picture + script.music_screen;
    auto screen = black_screen();
    for (const auto &step : script.steps)
    {
        if (script.clears_between)
        {
            screen = black_screen();
        }
        for (const auto &[record, column, row] : step.draws)
        {
            const auto picture = pictures.find(record);
            if (picture == pictures.end())
            {
                throw std::runtime_error("TITLE.DAX has no picture " + std::to_string(record));
            }
            draw(screen, picture->second, column * cell, row * cell);
        }
        const auto shown = title.screens.size();
        title.screens.push_back({screen, static_cast<double>(step.seconds), step.sound,
                                 screen_after_key(step.key, shown, last_screen)});
    }
    return title;
}

namespace
{
// Native ending-show facts: three FINAL.TLB records, in order 1, 3, 2.
// Only draws followed by a pause become visible; every overlay redraws
// glyph 1 first. The original's pause unit is approximately two seconds.
TitleScreens dark_queen_closing_film(const std::filesystem::path &folder)
{
    const ua::DesignFiles files(folder, {});
    const auto library = files.library("FINAL.TLB");
    if (!library)
    {
        throw std::runtime_error("Dark Queen needs FINAL.TLB for its ending");
    }
    TitleScreens film;
    const auto scene = [&](unsigned record, const std::vector<std::pair<unsigned, unsigned>> &steps)
    {
        const auto tiles = ua::nested_tiles(*library, record, "Dark Queen closing scene");
        auto palette = vga_color_table({});
        ua::apply_palette_glyph(palette, tiles);
        const auto draw_glyph = [&](Image &screen, unsigned number)
        {
            if (number >= tiles.glyphs.size() || !tiles.glyphs[number] ||
                tiles.glyphs[number]->kind == ua::GlyphKind::palette)
            {
                throw std::runtime_error("Dark Queen ending has no required glyph");
            }
            const auto &glyph = *tiles.glyphs[number];
            const auto picture = ua::glyph_image(glyph, palette);
            const int left = cell - glyph.origin_column, top = cell - glyph.origin_row;
            for (int y = 0; y < picture.height; ++y)
            {
                for (int x = 0; x < picture.width; ++x)
                {
                    const int dx = left + x, dy = top + y;
                    const auto source = (static_cast<std::size_t>(y) * picture.width + x) * 4;
                    if (dx >= 0 && dx < screen_width && dy >= 0 && dy < screen_height &&
                        picture.rgba[source + 3])
                    {
                        const auto destination =
                            (static_cast<std::size_t>(dy) * screen_width + dx) * 4;
                        std::copy_n(picture.rgba.begin() + source, 4,
                                    screen.rgba.begin() + destination);
                    }
                }
            }
        };
        for (const auto [overlay, units] : steps)
        {
            auto screen = black_screen();
            draw_glyph(screen, 1);
            if (overlay > 1)
            {
                draw_glyph(screen, overlay);
            }
            film.screens.push_back({std::move(screen), units * 2.0, {}, film.screens.size() + 1});
        }
    };
    std::vector<std::pair<unsigned, unsigned>> first{{1, 1}};
    for (unsigned frame = 2; frame <= 16; ++frame)
    {
        first.emplace_back(frame, frame == 16 ? 2 : 1);
    }
    scene(1, first);
    std::vector<std::pair<unsigned, unsigned>> second{{1, 2},  {2, 2},  {3, 2},  {4, 2},  {5, 2},
                                                      {6, 2},  {5, 1},  {7, 2},  {16, 1}, {17, 1},
                                                      {16, 1}, {17, 1}, {16, 1}, {17, 1}, {16, 3}};
    scene(3, second);
    std::vector<std::pair<unsigned, unsigned>> third;
    for (unsigned cycle = 0; cycle < 6; ++cycle)
    {
        for (unsigned frame = 1; frame <= 3; ++frame)
        {
            third.emplace_back(frame, 1);
        }
    }
    for (const unsigned frame :
         {4U, 3U, 1U, 5U, 5U, 6U, 6U, 7U, 7U, 8U, 8U, 9U, 9U, 10U, 10U, 11U, 12U})
    {
        third.emplace_back(frame, 1);
    }
    third.insert(third.end(), {{13, 2}, {14, 2}, {13, 1}, {14, 2}});
    scene(2, third);
    film.screens.front().sound = 39;
    return film;
}
} // namespace

TitleScreens load_closing_film(const std::filesystem::path &folder, GoldBoxGame game)
{
    if (game == GoldBoxGame::dark_queen_of_krynn)
    {
        return dark_queen_closing_film(folder);
    }
    TitleScreens film;
    if (game == GoldBoxGame::pools_of_darkness)
    {
        add_film(folder, "FINAL", film);
    }
    return film;
}

} // namespace opengold
