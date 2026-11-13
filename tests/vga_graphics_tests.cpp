// The 256-color pictures of Pools of Darkness and Treasures of the Savage
// Frontier, read from the games' own files (docs/games/darkness.md). Skipped
// when OPENGOLD_GOLDBOX_DIR does not name the folder holding DARKNESS and
// TREASURE.
#include "opengold/formats.h"
#include "opengold/treasures_texts.h"
#include "opengold/vga_picture.h"
#include "opengold/wall_art.h"
#include "test_support.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;

Bytes record_of(const std::filesystem::path &file, unsigned id)
{
    std::ifstream stream(file, std::ios::binary);
    const Bytes bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    auto archive = opengold::decode_dax_archive(bytes);
    check(static_cast<bool>(archive), file.string() + " is a DAX archive");
    for (auto &record : archive.records)
    {
        if (record.id == id)
        {
            return std::move(record.bytes);
        }
    }
    throw opengold::test::CheckFailure(file.string() + " has record " + std::to_string(id));
}

std::uint8_t pixel(const opengold::VgaImage &image, unsigned x, unsigned y)
{
    return image.pixels.at(std::size_t{y} * image.width + x);
}

// The title picture: the whole screen, every one of the 256 colors its own.
void a_title_picture_keeps_its_indices_and_colors(const std::filesystem::path &darkness)
{
    const auto picture = opengold::decode_vga_picture(record_of(darkness / "TITLE.DAX", 1));
    check(picture.has_value(), "TITLE.DAX record 1 is a picture");
    check(picture->frames.size() == 1 && picture->frames[0].width == 320 &&
              picture->frames[0].height == 200,
          "The title fills the 320 x 200 screen");
    check(picture->palette.first == 0 && picture->palette.colors.size() == 256,
          "The title sets all 256 colors");
    check(picture->palette.colors[1] == opengold::VgaColor{0, 0, 42} &&
              picture->palette.colors[15] == opengold::VgaColor{63, 63, 63},
          "Colors 1 and 15 are the title's dark blue and white, 0 to 63 a channel");
    const auto &image = picture->frames[0];
    check(pixel(image, 0, 0) == 160 && pixel(image, 160, 100) == 225 &&
              pixel(image, 100, 50) == 168,
          "The title's pixels keep their 8-bit color indices");
}

// PIC1.DAX record 7 moves in three frames; the key frame (1) is stored whole,
// the others as their difference from it.
void an_animation_unpacks_its_frames_against_the_key_frame(const std::filesystem::path &darkness)
{
    const auto animation = opengold::decode_vga_animation(record_of(darkness / "PIC1.DAX", 7));
    check(animation.has_value(), "PIC1.DAX record 7 is an animation");
    check(animation->frames.size() == 3, "It has three frames");
    for (const auto &frame : animation->frames)
    {
        check(frame.image.width == 88 && frame.image.height == 88, "Each frame is 88 x 88");
    }
    check(animation->palette.first == 32 && animation->palette.colors.size() == 224 &&
              animation->palette.colors[0] == opengold::VgaColor{63, 63, 63},
          "It sets colors 32 to 255, the first of them white");
    check(animation->frames[0].delay == 10 && animation->frames[1].delay == 3 &&
              animation->frames[2].delay == 3 && animation->first_frame == 0,
          "The frames keep their delays and the loop starts again at frame 0");
    const auto &key = animation->frames[1].image;
    check(pixel(key, 417 % 88, 417 / 88) == 0 &&
              pixel(animation->frames[0].image, 417 % 88, 417 / 88) == 81,
          "Frame 0 differs from the key frame where it moves");
    check(pixel(key, 1368 % 88, 1368 / 88) == 54 &&
              pixel(animation->frames[2].image, 1368 % 88, 1368 / 88) == 53,
          "Frame 2 is the key frame with its difference applied");
}

void a_sprite_has_its_three_distances(const std::filesystem::path &darkness)
{
    const auto sprite = opengold::decode_vga_animation(record_of(darkness / "SPRIT1.DAX", 5));
    check(sprite.has_value() && sprite->frames.size() == 3 && sprite->palette.first == 32,
          "SPRIT1.DAX record 5 holds a near, a middle and a far frame");
}

// WALLDEF1.DAX record 1 holds three wall definitions of five appearances each,
// drawn from the tiles of 8X8D1.DAX record 1.
void walls_are_drawn_from_their_own_tiles(const std::filesystem::path &darkness)
{
    const auto tiles = opengold::decode_vga_picture(record_of(darkness / "8X8D1.DAX", 1));
    check(tiles.has_value() && tiles->frames.size() == 256, "8X8D1.DAX record 1 is 256 tiles");
    const auto walls =
        opengold::por::decode_vga_wall_art(record_of(darkness / "WALLDEF1.DAX", 1), *tiles);
    check(walls.has_value() && walls->size() == 15, "The record holds 15 wall appearances");
    const auto &near_front =
        (*walls)[0][static_cast<std::size_t>(opengold::por::WallView::near_front)];
    check(near_front.width == 56 && near_front.height == 64, "The near front wall is 7 x 8 tiles");
    check(pixel(near_front, 20, 30) == 140, "A wall pixel is its tile's color index");
    const auto &far_left =
        (*walls)[12][static_cast<std::size_t>(opengold::por::WallView::far_left)];
    check(pixel(far_left, 0, 8) == 13, "Tile 0 comes out in the see-through color 13");

    check(!opengold::por::decode_vga_wall_art(record_of(darkness / "WALLDEF1.DAX", 1),
                                              opengold::VgaPicture{})
               .has_value(),
          "Walls whose tiles are missing are refused");
}

void broken_records_are_refused(const std::filesystem::path &darkness)
{
    auto title = record_of(darkness / "TITLE.DAX", 1);
    title.pop_back();
    check(!opengold::decode_vga_picture(title).has_value(), "A picture one byte short is refused");
    auto animation = record_of(darkness / "PIC1.DAX", 7);
    animation.pop_back();
    check(!opengold::decode_vga_animation(animation).has_value(),
          "An animation one byte short is refused");
    check(!opengold::decode_vga_animation(record_of(darkness / "TITLE.DAX", 1)).has_value(),
          "A still picture is not an animation");
}

// Treasures keeps records that only set colors among its PIC pictures, and
// tables and texts among its border tiles.
void treasures_reads_its_own_files(const std::filesystem::path &treasure)
{
    const auto colors = opengold::decode_vga_picture(record_of(treasure / "PIC1.DAX", 38));
    check(colors.has_value() && colors->frames.size() == 1 && colors->frames[0].width == 0 &&
              colors->palette.first == 32 && colors->palette.colors.size() == 224 &&
              colors->palette.colors[0] == opengold::VgaColor{21, 21, 21},
          "PIC1.DAX record 38 only sets colors 32 to 255");

    const auto runs = opengold::treasures::decode_border_runs(record_of(treasure / "BORD.DAX", 7));
    check(runs.has_value() && runs->size() == 19 && runs->front().size() == 23,
          "BORD.DAX record 7 holds 19 runs of border tiles");
    const auto effects =
        opengold::treasures::decode_name_table(record_of(treasure / "BORD.DAX", 9), 37);
    check(effects.has_value() && effects->size() == 160 && effects->front() == "Blessed",
          "BORD.DAX record 9 names 160 effects");
    const auto words =
        opengold::treasures::decode_name_table(record_of(treasure / "BORD.DAX", 10), 20);
    check(words.has_value() && words->size() == 255 && words->front() == "Battle Axe",
          "BORD.DAX record 10 holds 255 item words");
    const auto luck = opengold::treasures::decode_luck_text(record_of(treasure / "LUCK.DAX", 0));
    check(luck.has_value() && luck->starts_with("As you place the crown"),
          "LUCK.DAX record 0 is a text");
    const auto font = opengold::decode_vga_picture(record_of(treasure / "BORD.DAX", 8));
    check(font.has_value() && font->frames.size() == 64 && font->frames[0].width == 8,
          "Treasures' font is a picture of 64 8x8 glyphs");
}

} // namespace

int main()
{
    try
    {
        const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
        const auto darkness =
            games ? std::filesystem::path(games) / "DARKNESS" : std::filesystem::path();
        const auto treasure =
            games ? std::filesystem::path(games) / "TREASURE" : std::filesystem::path();
        if (!games || !std::filesystem::exists(darkness / "TITLE.DAX") ||
            !std::filesystem::exists(treasure / "LUCK.DAX"))
        {
            std::cout << "Skipped: Pools of Darkness and Treasures are not here.\n";
            return 0;
        }
        a_title_picture_keeps_its_indices_and_colors(darkness);
        an_animation_unpacks_its_frames_against_the_key_frame(darkness);
        a_sprite_has_its_three_distances(darkness);
        walls_are_drawn_from_their_own_tiles(darkness);
        broken_records_are_refused(darkness);
        treasures_reads_its_own_files(treasure);
        std::cout << "VGA graphics passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
