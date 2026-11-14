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

// Draws `marker` over `picture` with its top-left corner at (x, y), leaving
// the picture's pixels under the marker's clear ones.
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

} // namespace

namespace detail
{

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

bool RolfTourSession::darkness_area_map() const
{
    return game() == GoldBoxGame::pools_of_darkness && vga_area_map();
}

void RolfTourSession::publish_darkness_area_square()
{
    snapshot_.overland = true;
    snapshot_.overland_x = machine_.variable(column_cell);
    snapshot_.overland_y = machine_.variable(row_cell);
    snapshot_.overland_direction = machine_.variable(game_cell(ecl_overland_direction)) % 8;
}

Image RolfTourSession::render_darkness_area_map() const
{
    const auto tile_set = machine_.variable(tile_set_cell);
    const auto picture_id = tile_set < area_pictures.size() ? area_pictures[tile_set] : 0;
    const auto &pictures = area_resources().pictures;
    const auto found = pictures.find(picture_id);
    if (found == pictures.end())
    {
        return {};
    }
    auto map = found->second;
    if (town_->area_map_marker)
    {
        draw_over(map, *town_->area_map_marker, machine_.variable(column_cell) * square_size,
                  machine_.variable(row_cell) * square_size);
    }
    return map;
}

} // namespace opengold::por
