#include "opengold/title_screens.h"

#include "area_loading.h"

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

struct TitleStep
{
    std::vector<TitleDraw> draws;
    unsigned seconds{};
    std::optional<unsigned> sound;
};

struct TitleScript
{
    std::vector<TitleStep> steps;
    bool key_ends_all{};
};

// Each game's Intro.Introduction with its full-introduction flag on, without
// the credits that follow the pictures.
TitleScript title_script(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return {{{{{1, 0, 0}}, 5, {}}, {{{2, 0, 0}}, 5, {}}}, false};
    case GoldBoxGame::curse_of_the_azure_bonds:
    {
        // Picture 4 starts the sound effect sndIntro (13).
        constexpr unsigned title_sound = 13;
        return {{{{{1, 0, 0}}, 5, {}},
                 {{{2, 0, 0}, {3, 6, 11}}, 10, {}},
                 {{{4, 0, 11}}, 10, title_sound}},
                false};
    }
    case GoldBoxGame::secret_of_the_silver_blades:
        return {{{{{1, 0, 0}}, 15, {}},
                 {{{2, 0, 0}}, 50, {}},
                 {{{3, 2, 5}}, 50, {}},
                 {{{2, 0, 0}, {4, 5, 5}}, 60, {}}},
                true};
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

} // namespace

TitleScreens load_title_screens(const std::filesystem::path &folder, GoldBoxGame game)
{
    const auto script = title_script(game);
    if (script.steps.empty())
    {
        return {};
    }
    const auto archive = decode_dax_archive(
        por::detail::read_archive(por::detail::resolve_archive(folder, "TITLE.DAX")));
    if (!archive)
    {
        throw std::runtime_error("Invalid TITLE.DAX");
    }
    std::map<std::uint8_t, Image> pictures;
    for (const auto &record : archive.records)
    {
        if (auto picture = decode_ega_picture(record.bytes))
        {
            pictures.emplace(record.id, std::move(picture.image));
        }
    }
    TitleScreens title{{}, script.key_ends_all};
    auto screen = black_screen();
    for (const auto &step : script.steps)
    {
        for (const auto &[record, column, row] : step.draws)
        {
            const auto picture = pictures.find(record);
            if (picture == pictures.end())
            {
                throw std::runtime_error("TITLE.DAX has no picture " + std::to_string(record));
            }
            draw(screen, picture->second, column * cell, row * cell);
        }
        title.screens.push_back({screen, step.seconds, step.sound});
    }
    return title;
}

} // namespace opengold
