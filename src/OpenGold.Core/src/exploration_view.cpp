#include "opengold/exploration_view.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace opengold::por
{
ExplorationView render_exploration_view(const GeoMap &map, const WallArtSet &art, unsigned x,
                                        unsigned y, unsigned facing)
{
    if (x >= 16 || y >= 16 || facing >= 4)
        throw std::out_of_range("Invalid exploration view pose");
    ExplorationView view;
    auto &result = view.image;
    // Each screen pixel remembers the map cell that owns its visible surface.
    // Painting a nearer opaque wall replaces both its color and its owner;
    // transparent cutouts leave both untouched. Hidden walls cannot reveal cells.
    std::array<int, 88 * 88> owners;
    owners.fill(-1);
    result.width = result.height = 88;
    result.rgba.resize(88 * 88 * 4);
    const bool interior = map.at(x, y).event_high_bit();
    for (int row = 0; row < 88; ++row)
    {
        const std::array<std::uint8_t, 4> color =
            row < 44 ? (interior ? std::array<std::uint8_t, 4> {85, 85, 85, 255}
                        : std::array<std::uint8_t, 4> {85, 255, 255, 255})
            : std::array<std::uint8_t, 4> {170, 85, 0, 255};
        for (int column = 0; column < 88; ++column)
            std::copy(color.begin(), color.end(), result.rgba.begin() + (row * 88 + column) * 4);
    }
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    const auto cell_at = [&](int depth, int lateral) -> int
    {
        const int cx = static_cast<int>(x) + dx[facing] * depth - dy[facing] * lateral;
        const int cy = static_cast<int>(y) + dy[facing] * depth + dx[facing] * lateral;
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
            owners[row * 88 + column] = cell_at(depth, lateral);
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
    const auto wall = [&](int depth, int lateral, unsigned side) -> WallSample
    {
        const int cell = cell_at(depth, lateral);
return cell < 0 ? WallSample{} :
        WallSample{map.cells[cell].walls[side], cell};
    };
    const auto draw = [&](WallSample sample, WallView perspective, int left, int top)
    {
        if (!sample)
            return;
        if (sample.id > art.appearances.size())
            throw std::runtime_error("Missing map wall appearance");
        const auto &piece = art.appearances[sample.id - 1][static_cast<unsigned>(perspective)];
        if (piece.rgba.size() != static_cast<std::size_t>(piece.width) * piece.height * 4)
            throw std::runtime_error("Invalid wall image");
        const int x0 = std::max(0, left), y0 = std::max(0, top);
        const int x1 = std::min(88, left + piece.width), y1 = std::min(88, top + piece.height);
        for (int py = y0; py < y1; ++py)
        {
            for (int px = x0; px < x1; ++px)
            {
                const auto source = ((py - top) * piece.width + px - left) * 4;
                if (piece.rgba[source + 3])
                {
                    std::copy_n(piece.rgba.begin() + source, 4,
                                result.rgba.begin() + (py * 88 + px) * 4);
                    owners[py * 88 + px] = sample.cell;
                }
            }
        }
    };
    const unsigned left_side = (facing + 3) % 4, right_side = (facing + 1) % 4;

    // Paint far to near. Stored side silhouettes and transparent cutouts retain
    // their authored perspective; no flat textures or procedural doors are added.
    for (int lateral = -3; lateral <= 3; ++lateral)
    {
        const auto front = wall(2, lateral, facing);
        draw(front, WallView::far_front, 40 + lateral * 16, 32);
        if (lateral <= 0 && lateral > -3 &&
                (wall(2, lateral, left_side) || wall(2, lateral - 1, facing)))
            draw(front, WallView::far_extension, 32 + lateral * 16, 32);
        if (lateral > 0 && (front || wall(2, lateral - 1, right_side)))
            draw(wall(2, lateral - 1, facing), WallView::far_extension, 32 + lateral * 16, 32);
    }
    for (int lateral = -2; lateral <= 2; ++lateral)
    {
        if (lateral <= 0)
            draw(wall(2, lateral, left_side), WallView::far_left,
                 32 + lateral * 16 - (lateral < 0 ? 8 : 0), 24);
        if (lateral >= 0)
            draw(wall(2, lateral, right_side), WallView::far_right,
                 48 + lateral * 16 + (lateral > 0 ? 8 : 0), 24);
    }
    for (int lateral = -2; lateral <= 2; ++lateral)
        draw(wall(1, lateral, facing), WallView::middle_front, 32 + lateral * 24, 24);
    for (int lateral = -1; lateral <= 1; ++lateral)
    {
        if (lateral <= 0)
            draw(wall(1, lateral, left_side), WallView::middle_left, 16 + lateral * 24, 8);
        if (lateral >= 0)
            draw(wall(1, lateral, right_side), WallView::middle_right, 56 + lateral * 24, 8);
    }
    for (int lateral = -1; lateral <= 1; ++lateral)
        draw(wall(0, lateral, facing), WallView::near_front, 16 + lateral * 56, 8);
    draw(wall(0, 0, left_side), WallView::near_left, 0, 0);
    draw(wall(0, 0, right_side), WallView::near_right, 72, 0);
    view.visible.set(y * 16 + x);
    for (const int cell : owners)
        if (cell >= 0)
            view.visible.set(cell);
    return view;
}

Image compose_exploration_view(const GeoMap &map, const WallArtSet &art, unsigned x, unsigned y,
                               unsigned facing)
{
    return render_exploration_view(map, art, x, y, facing).image;
}
} // namespace opengold::por
