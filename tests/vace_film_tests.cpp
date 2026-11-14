// Pools of Darkness' opening and ending films, INTRO and FINAL, read from the
// game's own files (docs/games/darkness.md, "The opening and ending films"). Skipped when
// OPENGOLD_GOLDBOX_DIR does not name the folder holding DARKNESS.
#include "opengold/vace_film.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using opengold::VaceFilmFrame;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;
using Color = std::array<std::uint8_t, 3>;

Bytes file_bytes(const std::filesystem::path &file)
{
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<VaceFilmFrame> film_of(const std::filesystem::path &darkness, const std::string &name)
{
    auto film = opengold::decode_vace_film(file_bytes(darkness / (name + ".ACF")),
                                           file_bytes(darkness / (name + ".BMA")));
    check(film.has_value(), name + " is a film");
    return std::move(*film);
}

std::uint8_t pixel(const VaceFilmFrame &frame, unsigned x, unsigned y)
{
    return frame.pixels.at(std::size_t{y} * opengold::vace_film_width + x);
}

// The opening: SSI's title card, then the story in 132 more frames.
void the_opening_starts_with_the_title_card(const std::filesystem::path &darkness)
{
    const auto film = film_of(darkness, "INTRO");
    check(film.size() == 133, "INTRO plays 133 frames");
    for (const auto &frame : film)
    {
        check(frame.pixels.size() == 320 * 200, "Every frame fills the 320 x 200 screen");
    }
    const auto &title = film.front();
    check(pixel(title, 0, 0) == 160 && pixel(title, 160, 100) == 174 &&
              pixel(title, 100, 50) == 168 && pixel(title, 160, 20) == 103,
          "The title card keeps its 8-bit color indices");
    check(title.palette[1] == Color{0, 0, 42} && title.palette[15] == Color{63, 63, 63} &&
              title.palette[174] == Color{0, 0, 48},
          "The title card's colors are 0 to 63 a channel");
}

// A frame redraws only what changes: INTRO's third frame changes one pixel of
// the second, the fourth two more.
void frames_draw_over_the_frame_before(const std::filesystem::path &darkness)
{
    const auto film = film_of(darkness, "INTRO");
    std::vector<std::size_t> changed;
    for (std::size_t at = 0; at < film[1].pixels.size(); ++at)
    {
        if (film[1].pixels[at] != film[2].pixels[at])
        {
            changed.push_back(at);
        }
    }
    check(changed == std::vector<std::size_t>{98 * 320 + 169},
          "INTRO's third frame changes only the pixel at 169, 98");
    check(pixel(film[2], 169, 98) == 32 && pixel(film[3], 169, 98) == 41 &&
              pixel(film[3], 168, 97) == 41,
          "The fourth frame lights 168, 97 and 169, 98");
    check(film[1].palette[pixel(film[1], 160, 100)] == Color{11, 2, 39},
          "The second scene sets its own colors");
}

// The pace: the slowest step of the films holds 261 ticks of the 72.8 Hz
// timer, the story's frames 16 or 14 of them.
void frames_keep_their_pace(const std::filesystem::path &darkness)
{
    const auto opening = film_of(darkness, "INTRO");
    check(opening[0].hold_milliseconds == 3583 && opening[1].hold_milliseconds == 3583 &&
              opening[2].hold_milliseconds == 219 && opening[45].hold_milliseconds == 192,
          "INTRO's frames hold 3583, 219 or 192 milliseconds");
    const auto ending = film_of(darkness, "FINAL");
    check(ending.size() == 58, "FINAL plays 58 frames");
    check(ending[0].hold_milliseconds == 3583 && ending[1].hold_milliseconds == 219 &&
              ending.back().hold_milliseconds == 3583,
          "FINAL's frames hold 3583 or 219 milliseconds");
    check(pixel(ending.back(), 160, 100) == 51 && ending.back().palette[51] == Color{16, 0, 31},
          "FINAL's last frame keeps its indices and colors");
}

// INTRO starts its song, P1.SMS's song 0, with the title card, which a key
// cannot pass; a key ends the film from the next frame on. FINAL never lets a
// key end it.
void films_start_their_song_and_keys_end_the_opening(const std::filesystem::path &darkness)
{
    const auto opening = film_of(darkness, "INTRO");
    check(opening[0].starts_song && opening[0].starts_song->file == "P1.SMS" &&
              opening[0].starts_song->number == 0,
          "INTRO starts P1.SMS's song 0 with its first frame");
    check(std::ranges::none_of(opening.begin() + 1, opening.end(),
                               [](const VaceFilmFrame &frame)
                               {
                                   return frame.starts_song.has_value();
                               }),
          "INTRO starts its song once");
    check(!opening[0].key_ends_film, "A key does not pass INTRO's title card");
    check(std::ranges::all_of(opening.begin() + 1, opening.end(), &VaceFilmFrame::key_ends_film),
          "A key ends INTRO after its title card");
    const auto ending = film_of(darkness, "FINAL");
    check(ending[0].starts_song && ending[0].starts_song->file == "P3.SMS",
          "FINAL starts P3.SMS's song");
    check(std::ranges::none_of(ending, &VaceFilmFrame::key_ends_film), "No key ends FINAL");
}

void broken_films_are_refused(const std::filesystem::path &darkness)
{
    const auto control = file_bytes(darkness / "INTRO.ACF");
    const auto frames = file_bytes(darkness / "INTRO.BMA");
    check(!opengold::decode_vace_film(control, Bytes(frames.begin(), frames.end() - 1)),
          "A cut .BMA is refused");
    check(!opengold::decode_vace_film(file_bytes(darkness / "FINAL.ACF"), frames),
          "Frames that do not match the control file's list are refused");
    auto bad_run = frames;
    bad_run[0x11] = 0xFF; // The title card's first piece claims 255 pixels.
    check(!opengold::decode_vace_film(control, bad_run), "A piece whose runs disagree is refused");
}

} // namespace

int main()
{
    try
    {
        const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
        const auto darkness =
            games ? std::filesystem::path(games) / "DARKNESS" : std::filesystem::path();
        if (!games || !std::filesystem::exists(darkness / "INTRO.ACF"))
        {
            std::cout << "Skipped: Pools of Darkness is not here.\n";
            return 0;
        }
        the_opening_starts_with_the_title_card(darkness);
        frames_draw_over_the_frame_before(darkness);
        frames_keep_their_pace(darkness);
        films_start_their_song_and_keys_end_the_opening(darkness);
        broken_films_are_refused(darkness);
        std::cout << "VACE films passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
