#include "opengold/exploration_view.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace opengold::por
{
ExplorationView render_exploration_view(const GeoMap &map, const WallArtSet &art, unsigned x,
                                        unsigned y, MapDirection facing)
{
    if (x >= 16 || y >= 16)
    {
        throw std::out_of_range("Invalid exploration view pose");
    }
    ExplorationView view;
    auto &result = view.image;
    const int scale = static_cast<int>(art.scale);
    const int side = 88 * scale;
    view.scale = art.scale;
    view.wall_hits.resize(static_cast<std::size_t>(side) * side);
    // Each screen pixel remembers the map cell that owns its visible surface.
    // Painting a nearer opaque wall replaces both its color and its owner;
    // transparent cutouts leave both untouched. Hidden walls cannot reveal cells.
    std::vector<int> owners(static_cast<std::size_t>(side) * side, -1);
    result.width = result.height = static_cast<std::uint16_t>(side);
    result.rgba.resize(static_cast<std::size_t>(side) * side * 4);
    const bool interior = map.at(x, y).event_high_bit();
    // Sky (or ceiling) above the horizon and ground below: paint one row of
    // each, then copy it down, since new art makes the view large.
    const auto row_bytes = static_cast<std::size_t>(side) * 4;
    for (const int band : {0, 44 * scale})
    {
        const std::array<std::uint8_t, 4> color =
            band == 0 ? (interior ? std::array<std::uint8_t, 4>{85, 85, 85, 255}
                                  : std::array<std::uint8_t, 4>{85, 255, 255, 255})
                      : std::array<std::uint8_t, 4>{170, 85, 0, 255};
        const auto first = result.rgba.begin() + static_cast<std::ptrdiff_t>(band * row_bytes);
        for (int column = 0; column < side; ++column)
        {
            std::copy(color.begin(), color.end(), first + column * 4);
        }
        for (int row = 1; row < 44 * scale; ++row)
        {
            std::copy_n(first, row_bytes, first + static_cast<std::ptrdiff_t>(row * row_bytes));
        }
    }
    const auto cell_at = [&](int depth, int lateral) -> int
    {
        const int cx = static_cast<int>(x) + step_x(facing) * depth - step_y(facing) * lateral;
        const int cy = static_cast<int>(y) + step_y(facing) * depth + step_x(facing) * lateral;
        return cx < 0 || cy < 0 || cx >= 16 || cy >= 16 ? -1 : cy * 16 + cx;
    };
    // The authored perspective has three floor bands, below the far, middle,
    // and near wall bottoms (48, 56, 72). Interpolate their projected cell
    // widths to label the otherwise featureless floor. Do not attribute the
    // horizon or sky to distant cells beyond the renderer's two-cell reach.
    for (int row = 48; row < 88; ++row)
    {
        const int depth = row < 56 ? 2 : row < 72 ? 1 : 0;
        const double width = row < 56   ? 16 + (row - 48 + .5)
                             : row < 72 ? 24 + 2 * (row - 56 + .5)
                                        : 56 + 2 * (row - 72 + .5);
        for (int column = 0; column < 88; ++column)
        {
            const int lateral = static_cast<int>(std::floor((column + .5 - 44) / width + .5));
            const int owner = cell_at(depth, lateral);
            for (int py = row * scale; py < (row + 1) * scale; ++py)
            {
                std::fill_n(owners.begin() + py * side + column * scale, scale, owner);
            }
        }
    }

    struct WallSample
    {
        unsigned id{};
        int cell{-1};

        explicit operator bool() const
        {
            return id != 0;
        }
    };

    // Read the face of the sampled cell seen by the party. Opposite faces may
    // deliberately have different art. Do not merge neighboring edge records.
    const auto wall = [&](int depth, int lateral, MapDirection side) -> WallSample
    {
        const int cell = cell_at(depth, lateral);
        return cell < 0 ? WallSample{} : WallSample{map.cells[cell].walls[index(side)], cell};
    };
    const auto draw =
        [&](WallSample sample, MapDirection face, WallView perspective, int left, int top)
    {
        if (!sample)
        {
            return;
        }
        if (sample.id > art.appearances.size())
        {
            throw std::runtime_error("Missing map wall appearance");
        }
        const auto &piece = art.appearances[sample.id - 1][static_cast<unsigned>(perspective)];
        if (!consistent(piece))
        {
            throw std::runtime_error("Invalid wall image");
        }
        // Offsets are in original pixels; the piece is already at the art's scale.
        left *= scale;
        top *= scale;
        const int x0 = std::max(0, left), y0 = std::max(0, top);
        const int x1 = std::min(side, left + piece.width), y1 = std::min(side, top + piece.height);
        for (int py = y0; py < y1; ++py)
        {
            for (int px = x0; px < x1; ++px)
            {
                const auto source = ((py - top) * piece.width + px - left) * 4;
                if (piece.rgba[source + 3])
                {
                    std::copy_n(piece.rgba.begin() + source, 4,
                                result.rgba.begin() + (py * side + px) * 4);
                    owners[py * side + px] = sample.cell;
                    view.wall_hits[py * side + px] = {sample.cell, face, perspective};
                }
            }
        }
    };
    const auto left_side = turned_left(facing), right_side = turned_right(facing);

    // Paint far to near. Stored side silhouettes and transparent cutouts retain
    // their authored perspective; no flat textures or procedural doors are added.
    for (int lateral = -3; lateral <= 3; ++lateral)
    {
        const auto front = wall(2, lateral, facing);
        draw(front, facing, WallView::far_front, 40 + lateral * 16, 32);
        if (lateral <= 0 && lateral > -3 &&
            (wall(2, lateral, left_side) || wall(2, lateral - 1, facing)))
        {
            draw(front, facing, WallView::far_extension, 32 + lateral * 16, 32);
        }
        if (lateral > 0 && (front || wall(2, lateral - 1, right_side)))
        {
            draw(wall(2, lateral - 1, facing), facing, WallView::far_extension, 32 + lateral * 16,
                 32);
        }
    }
    for (int lateral = -2; lateral <= 2; ++lateral)
    {
        if (lateral <= 0)
        {
            draw(wall(2, lateral, left_side), left_side, WallView::far_left,
                 32 + lateral * 16 - (lateral < 0 ? 8 : 0), 24);
        }
        if (lateral >= 0)
        {
            draw(wall(2, lateral, right_side), right_side, WallView::far_right,
                 48 + lateral * 16 + (lateral > 0 ? 8 : 0), 24);
        }
    }
    for (int lateral = -2; lateral <= 2; ++lateral)
    {
        draw(wall(1, lateral, facing), facing, WallView::middle_front, 32 + lateral * 24, 24);
    }
    for (int lateral = -1; lateral <= 1; ++lateral)
    {
        if (lateral <= 0)
        {
            draw(wall(1, lateral, left_side), left_side, WallView::middle_left, 16 + lateral * 24,
                 8);
        }
        if (lateral >= 0)
        {
            draw(wall(1, lateral, right_side), right_side, WallView::middle_right,
                 56 + lateral * 24, 8);
        }
    }
    for (int lateral = -1; lateral <= 1; ++lateral)
    {
        draw(wall(0, lateral, facing), facing, WallView::near_front, 16 + lateral * 56, 8);
    }
    draw(wall(0, 0, left_side), left_side, WallView::near_left, 0, 0);
    draw(wall(0, 0, right_side), right_side, WallView::near_right, 72, 0);
    view.visible.set(y * 16 + x);
    for (const int cell : owners)
    {
        if (cell >= 0)
        {
            view.visible.set(cell);
        }
    }
    return view;
}

Image compose_exploration_view(const GeoMap &map, const WallArtSet &art, unsigned x, unsigned y,
                               MapDirection facing)
{
    return render_exploration_view(map, art, x, y, facing).image;
}
} // namespace opengold::por
