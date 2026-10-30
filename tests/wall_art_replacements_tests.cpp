// New wall art from data/art/walls (#272, #273): the folder the Replacement
// Art Creator exports to, the per-appearance swap with original fallback, and
// the exploration view composed at the new art's scale.
#include "opengold/exploration_view.h"
#include "opengold/rolf_tour.h"
#include "opengold/wall_art_replacements.h"
#include "test_support.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace opengold;
using namespace opengold::por;
using opengold::test::check;
using opengold::test::throws;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

// The game decodes PNGs through Godot. Here a marker-colored image of the PNG
// header's size stands in, which is all the folder rules depend on.
std::optional<Image> png_size_only(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 24 || bytes[1] != 'P' || bytes[2] != 'N' || bytes[3] != 'G')
    {
        return std::nullopt;
    }
    const auto word = [&](std::size_t at)
    {
        return static_cast<unsigned>(bytes[at] << 24 | bytes[at + 1] << 16 | bytes[at + 2] << 8 |
                                     bytes[at + 3]);
    };
    Image image;
    image.width = static_cast<std::uint16_t>(word(16));
    image.height = static_cast<std::uint16_t>(word(20));
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(image.width) * image.height;
         ++pixel)
    {
        image.rgba.insert(image.rgba.end(), {1, 2, 3, 255});
    }
    return image;
}

Image solid(unsigned width, unsigned height, std::uint8_t red)
{
    Image image;
    image.width = static_cast<std::uint16_t>(width);
    image.height = static_cast<std::uint16_t>(height);
    for (unsigned pixel = 0; pixel < width * height; ++pixel)
    {
        image.rgba.insert(image.rgba.end(), {red, 0, 0, 255});
    }
    return image;
}

// One opaque appearance with every piece at its original size.
WallArtSet one_appearance(std::uint8_t red)
{
    constexpr std::array<std::array<unsigned, 2>, 10> sizes{{{8, 16},
                                                             {8, 32},
                                                             {8, 32},
                                                             {24, 32},
                                                             {16, 64},
                                                             {16, 64},
                                                             {56, 64},
                                                             {16, 88},
                                                             {16, 88},
                                                             {8, 16}}};
    WallArtSet art;
    art.appearances.emplace_back();
    for (unsigned view = 0; view < sizes.size(); ++view)
    {
        art.appearances.back()[view] = solid(sizes[view][0], sizes[view][1], red);
    }
    return art;
}

void shipped_folder()
{
    const auto shipped = load_wall_art_replacements(root / "data/art/walls", png_size_only);
    check(shipped.sets.size() == 168, "data/art/walls holds the 168 used appearances");
    check(shipped.sets.contains("WALLDEF3.DAX:0:1") && shipped.sets.contains("WALLDEF2.DAX:4:5"),
          "Sets are keyed by their WALLDEF archive, record and appearance");
    const auto &near_front = shipped.sets.at("WALLDEF3.DAX:0:1")[6];
    check(near_front.width == 56 * 4 && near_front.height == 64 * 4,
          "A near front piece is four times the original 56 x 64");
    check(load_wall_art_replacements(root / "no-such-folder", png_size_only).sets.empty(),
          "Without the folder the game shows only original art");
}

void broken_folders()
{
    const auto folder = std::filesystem::temp_directory_path() / "opengold-wall-art-test";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "WALLDEF1_DAX_1_4-0002");
    check(throws(
              [&]
              {
                  (void)load_wall_art_replacements(folder, png_size_only);
              }),
          "A numbered export copy is not a game set");
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "WALLDEF1_DAX_1_4");
    std::ofstream(folder / "WALLDEF1_DAX_1_4" / "far_front.png") << "not a png";
    check(throws(
              [&]
              {
                  (void)load_wall_art_replacements(folder, png_size_only);
              }),
          "A set with a missing or unreadable piece is refused");
    std::filesystem::remove_all(folder);
}

void replacement_and_fallback()
{
    WallArtSet original = one_appearance(10);
    original.appearances.push_back(one_appearance(20).appearances.front());
    const std::vector<std::string> keys{"WALLDEF1.DAX:1:1", "WALLDEF1.DAX:1:2"};
    check(with_replacements(original, keys, {}).scale == 1,
          "With no new art the original art is untouched");

    WallArtReplacements replacements;
    auto enlarged = one_appearance(30).appearances.front();
    for (auto &piece : enlarged)
    {
        piece = solid(piece.width * 4u, piece.height * 4u, 30);
    }
    replacements.sets.emplace("WALLDEF1.DAX:1:2", enlarged);
    const auto mixed = with_replacements(original, keys, replacements);
    check(mixed.scale == 4, "New art sets the scale");
    check(mixed.appearances[1][6].rgba[0] == 30, "The appearance with new art shows it");
    check(mixed.appearances[0][6].rgba[0] == 10 && mixed.appearances[0][6].width == 56 * 4,
          "An appearance without new art falls back to its enlarged original");
    check(mixed.originals.size() == 2 && mixed.originals[0][6].width == 0 &&
              mixed.originals[1][6].rgba[0] == 20 && mixed.originals[1][6].width == 56 * 4,
          "The replaced appearance keeps its original pieces at the new scale");

    replacements.sets.begin()->second[3] = solid(24, 32, 30);
    check(throws(
              [&]
              {
                  (void)with_replacements(original, keys, replacements);
              }),
          "A piece that is not four times its original size is refused");
}

void scaled_view()
{
    GeoMap corridor;
    for (auto &cell : corridor.cells)
    {
        cell.walls.fill(1);
    }
    corridor.cells[8 * 16 + 8].walls[0] = 0;
    const auto original = one_appearance(90);
    const auto small = render_exploration_view(corridor, original, 8, 8, MapDirection::north);
    WallArtReplacements none_matching;
    none_matching.sets.emplace("WALLDEF9.DAX:0:1", std::array<Image, 10>{});
    const auto large_art =
        with_replacements(original, std::vector<std::string>{"WALLDEF1.DAX:1:1"}, none_matching);
    const auto large = render_exploration_view(corridor, large_art, 8, 8, MapDirection::north);
    check(large.scale == 4 && large.image.width == 352 && large.image.height == 352,
          "Four-times art composes a 352 x 352 view");
    bool same = true;
    for (unsigned y = 0; y < 352 && same; ++y)
    {
        for (unsigned x = 0; x < 352 && same; ++x)
        {
            same = std::equal(large.image.rgba.begin() + (y * 352 + x) * 4,
                              large.image.rgba.begin() + (y * 352 + x) * 4 + 4,
                              small.image.rgba.begin() + ((y / 4) * 88 + x / 4) * 4);
        }
    }
    check(same, "Enlarged original art draws exactly the original view, four times larger");

    // The 3D view art setting: with new art loaded, the original view shows
    // the replaced appearance's original pieces, at the same scale (#274).
    WallArtReplacements matching;
    auto pieces = one_appearance(70).appearances.front();
    for (auto &piece : pieces)
    {
        piece = solid(piece.width * 4u, piece.height * 4u, 70);
    }
    matching.sets.emplace("WALLDEF1.DAX:1:1", pieces);
    const auto replaced =
        with_replacements(original, std::vector<std::string>{"WALLDEF1.DAX:1:1"}, matching);
    const auto fresh = render_exploration_view(corridor, replaced, 8, 8, MapDirection::north);
    const auto old = render_exploration_view(corridor, replaced, 8, 8, MapDirection::north, true);
    check(old.image.rgba == large.image.rgba && fresh.image.rgba != large.image.rgba,
          "The original view draws the original walls in place of the new art");
    check(large.visible == small.visible, "Scale does not change what the party sees");
    const auto &hit = large.wall_hits[(44 * 4) * 352 + 44 * 4];
    check(hit.cell == small.wall_hits[44 * 88 + 44].cell &&
              hit.perspective == small.wall_hits[44 * 88 + 44].perspective,
          "A wall is picked at the same spot in either view");
}

void game_uses_new_art(const std::filesystem::path &directory)
{
    const auto shipped = load_wall_art_replacements(root / "data/art/walls", png_size_only);
    // Loading checks every district's sets against its original pieces too.
    const auto tour = RolfTourSession::load(directory, shipped);
    check(tour.wall_art().scale == 4 && tour.wall_art().appearances.size() == 15,
          "New Phlan loads with new wall art");
    check(tour.wall_art().appearances[0][6].rgba[0] == 1 &&
              tour.wall_art().appearances[0][6].rgba[2] == 3,
          "New Phlan's first appearance is the shipped set, not its original");
    check(RolfTourSession::load(directory).wall_art().scale == 1,
          "Without new art the game loads the original pieces");
    auto shown = RolfTourSession::load(directory, shipped);
    const auto with_new = shown.observe_view();
    shown.show_original_walls(true);
    const auto with_original = shown.observe_view();
    shown.show_original_walls(false);
    check(with_new.rgba != with_original.rgba && shown.observe_view().rgba == with_new.rgba,
          "The session switches between the new and the original walls at once");
}
} // namespace

int main()
{
    try
    {
        shipped_folder();
        broken_folders();
        replacement_and_fallback();
        scaled_view();
        if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
        {
            game_uses_new_art(directory);
        }
        std::cout << "Wall art replacement tests passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
