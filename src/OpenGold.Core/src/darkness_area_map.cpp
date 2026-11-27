// Drawing Pools of Darkness's area map: outside a dungeon the original shows
// the whole big picture of the area's tile set and draws the party's marker
// over its square (docs/games/darkness.md, The area map). Kept apart from the
// 256-color decoders it builds on (vga_picture.h).
#include "opengold/rolf_tour.h"
#include "area_loading.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <array>

namespace opengold::por
{
namespace
{

// The cells the area map reads: its tile set (3Ah) and the party's square,
// 38 columns by 15 rows (25h and 26h).
constexpr EclAddress tile_set_cell{0x3A}, column_cell{0x25}, row_cell{0x26};
// The big picture of each tile set, as the original's areaPictureIds lists
// them; the last set has none.
constexpr std::array<unsigned, 6> area_pictures{240, 241, 242, 242, 245, 0};
// A square of the area map is one 8-pixel cell of its picture.
constexpr unsigned square_size = 8;
// The marker's see-through color, as the original loads it.
constexpr std::uint8_t marker_see_through = 13;

} // namespace

namespace detail
{

void draw_over(Image &picture, const Image &marker, unsigned x, unsigned y)
{
    for (unsigned row = 0; row < marker.height && y + row < picture.height; ++row)
    {
        for (unsigned column = 0; column < marker.width && x + column < picture.width; ++column)
        {
            const auto from = (static_cast<std::size_t>(row) * marker.width + column) * 4;
            if (marker.rgba[from + 3] == 0)
            {
                continue;
            }
            const auto to = (static_cast<std::size_t>(y + row) * picture.width + x + column) * 4;
            std::copy_n(marker.rgba.begin() + static_cast<std::ptrdiff_t>(from), 4,
                        picture.rgba.begin() + static_cast<std::ptrdiff_t>(to));
        }
    }
}

std::optional<Image> load_darkness_area_map_marker(const AreaFiles &files)
{
    constexpr auto name = "CURSOR.DAX";
    constexpr unsigned marker_record = 1;
    if (!files.has_archive(name))
    {
        return std::nullopt;
    }
    const auto &records = files.archive(name).records;
    const auto found = std::ranges::find(records, marker_record, &DaxRecord::id);
    const auto picture = found == records.end() ? std::nullopt : decode_vga_picture(found->bytes);
    if (!picture || picture->frames.empty())
    {
        return std::nullopt;
    }
    return vga_rgba(picture->frames.front(), vga_color_table(picture->palette), marker_see_through);
}

} // namespace detail

bool VgaHost::darkness_area_map(const TourHost &tour) const
{
    return (tour.game() == GoldBoxGame::pools_of_darkness || krynn_game(tour.game())) &&
           vga_area_map(tour);
}

void VgaHost::publish_darkness_area_square(TourHost &tour)
{
    tour.screen().overland = true;
    tour.screen().overland_x = tour.script_machine().variable(
        krynn_game(tour.game()) ? tour.game_cell(ecl_overland_x) : column_cell);
    tour.screen().overland_y = tour.script_machine().variable(
        krynn_game(tour.game()) ? tour.game_cell(ecl_overland_y) : row_cell);
    tour.screen().overland_direction =
        tour.script_machine().variable(tour.game_cell(ecl_overland_direction)) % 8;
}

Image VgaHost::render_darkness_area_map(const TourHost &tour) const
{
    const auto tile_set =
        krynn_game(tour.game()) ? 0U : tour.script_machine().variable(tile_set_cell);
    const auto picture_id = tour.game() == GoldBoxGame::dark_queen_of_krynn
                                ? tour.script_machine().variable(EclAddress{0x3E})
                            : tour.game() == GoldBoxGame::death_knights_of_krynn ? 112
                            : tour.game() == GoldBoxGame::champions_of_krynn     ? 121
                            : tile_set < area_pictures.size() ? area_pictures[tile_set]
                                                              : 0;
    const auto &pictures = tour.area_resources().pictures;
    const auto found = pictures.find(picture_id);
    if (found == pictures.end())
    {
        return {};
    }
    auto map = found->second;
    if (tour.town_resources().area_map_marker)
    {
        const auto x = tour.script_machine().variable(
            krynn_game(tour.game()) ? tour.game_cell(ecl_overland_x) : column_cell);
        const auto y = tour.script_machine().variable(
            krynn_game(tour.game()) ? tour.game_cell(ecl_overland_y) : row_cell);
        detail::draw_over(map, *tour.town_resources().area_map_marker, x * square_size,
                          y * square_size);
    }
    return map;
}

} // namespace opengold::por
