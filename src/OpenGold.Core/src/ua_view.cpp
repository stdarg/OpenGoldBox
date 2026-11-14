#include "opengold/ua_view.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

constexpr int view_size = 88;
constexpr std::size_t header_sets = 4, header_backdrops = 8;

// Where the ten pieces of a view are drawn, in 8-pixel steps from the
// window's corner: far front, far left and right sides, middle front, middle
// sides, near front, the party's own side walls, and the filler between two
// far front walls (the kit's position tables, shared with the earlier games).
constexpr std::array<int, 10> piece_x{5, 4, 6, 4, 2, 7, 2, 0, 9, 5};
constexpr std::array<int, 10> piece_y{4, 3, 3, 3, 1, 1, 1, 0, 0, 4};

[[nodiscard]] std::uint8_t full_level(std::uint8_t level) noexcept
{
    return static_cast<std::uint8_t>(level << 2 | level >> 4);
}

void apply(std::array<VgaColor, 256> &palette, const VgaPaletteRange &range)
{
    for (std::size_t n = 0; n < range.colors.size() && range.first + n < palette.size(); ++n)
    {
        palette[range.first + n] = range.colors[n];
    }
}

} // namespace

void apply_palette_glyph(std::array<VgaColor, 256> &palette, const TileLibrary &tiles)
{
    for (const auto &glyph : tiles.glyphs)
    {
        if (glyph && glyph->kind == GlyphKind::palette)
        {
            apply(palette, glyph->palette);
            return;
        }
    }
}

TileLibrary nested_tiles(const Library &outer, unsigned number, const std::string &what)
{
    const auto index = decode_index(outer);
    if (!index)
    {
        throw std::runtime_error(what + ": its library has no index");
    }
    const auto entry = std::ranges::find(*index, number, &IndexEntry::number);
    if (entry == index->end())
    {
        throw std::runtime_error(what + ": the library has none");
    }
    const auto inner = decode_library(outer.items[entry->item]);
    if (!inner)
    {
        throw std::runtime_error(what + ": not a resource library");
    }
    auto tiles = decode_tile_library(*inner);
    if (!tiles)
    {
        throw std::runtime_error(what + ": its glyphs do not decode");
    }
    return std::move(*tiles);
}

Image glyph_image(const Glyph &glyph, const std::array<VgaColor, 256> &palette)
{
    const auto &picture = glyph.image;
    Image image;
    image.width = picture.width;
    image.height = picture.height;
    image.rgba.assign(std::size_t{picture.width} * picture.height * 4, 0);
    for (std::size_t at = 0; at < picture.pixels.size(); ++at)
    {
        if (!glyph.see_through.empty() && glyph.see_through[at])
        {
            continue;
        }
        const auto &color = palette[picture.pixels[at]];
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            image.rgba[at * 4 + channel] = full_level(color[channel]);
        }
        image.rgba[at * 4 + 3] = 255;
    }
    return image;
}

namespace
{

class ViewPainter
{
  public:
    ViewPainter(const Area &area, const ViewArt &art) : area_(area), art_(art)
    {
        image_.width = image_.height = view_size;
        image_.rgba.assign(std::size_t{view_size} * view_size * 4, 255);
    }

    void fill(int top, int bottom, VgaColor color)
    {
        for (int y = top; y < bottom; ++y)
        {
            for (int x = 0; x < view_size; ++x)
            {
                put(x, y, color);
            }
        }
    }

    void draw(const Glyph &glyph, const std::array<VgaColor, 256> &palette, int left, int top)
    {
        left -= glyph.origin_column;
        top -= glyph.origin_row;
        const auto &picture = glyph.image;
        for (int row = 0; row < picture.height; ++row)
        {
            for (int column = 0; column < picture.width; ++column)
            {
                const auto at = static_cast<std::size_t>(row) * picture.width +
                                static_cast<std::size_t>(column);
                if (!glyph.see_through.empty() && glyph.see_through[at])
                {
                    continue;
                }
                put(left + column, top + row, palette[picture.pixels[at]]);
            }
        }
    }

    // A wall's graphic (1 to 15) as view piece `slot`, at the place the
    // tables give moved by `offset` 8-pixel steps.
    void piece(unsigned slot, int offset, std::uint8_t graphic)
    {
        const unsigned set_slot = (graphic - 1U) / 5;
        unsigned variant = (graphic - 1U) % 5 + 1;
        const auto &set = art_.sets.at(set_slot);
        if (!set)
        {
            return;
        }
        // The filler shows only the plain walls.
        if (slot == 9 && variant > 2)
        {
            variant = 1;
        }
        const int left = 8 * (piece_x[slot] + offset);
        const int top = 8 * piece_y[slot] + (slot == 7 || slot == 8 ? 0 : 8);
        // Glyph 1 and 11 are the filler; 2 to 10 and 12 to 20 the two walls;
        // the overlays of variants 3 to 5 (21, 30 and 39 on) lie over the
        // first wall.
        const unsigned first = slot == 9 ? 1 : slot + 2;
        const auto draw_glyph = [&](unsigned number)
        {
            if (number < set->glyphs.size() && set->glyphs[number])
            {
                draw(*set->glyphs[number], set->palette, left, top);
            }
        };
        if (variant <= 2)
        {
            draw_glyph(first + (variant - 1) * 10);
            return;
        }
        draw_glyph(first);
        draw_glyph((variant - 1) * 9 + (slot == 9 ? 0 : slot + 1) + 2);
    }

    // The graphic of the wall on side `side` of square (x, y); the area
    // wraps round at its edges, as the kit's view does.
    [[nodiscard]] std::uint8_t graphic(int x, int y, por::MapDirection side) const
    {
        const int columns = area_.columns, rows = area_.rows;
        x = (x % columns + columns) % columns;
        y = (y % rows + rows) % rows;
        return area_.squares[static_cast<std::size_t>(y * columns + x)].walls[por::index(side)] &
               0xF;
    }

    [[nodiscard]] Image take() &&
    {
        return std::move(image_);
    }

  private:
    void put(int x, int y, VgaColor color)
    {
        if (x < 0 || y < 0 || x >= view_size || y >= view_size)
        {
            return;
        }
        auto *pixel = &image_.rgba[(static_cast<std::size_t>(y) * view_size + x) * 4];
        pixel[0] = full_level(color[0]);
        pixel[1] = full_level(color[1]);
        pixel[2] = full_level(color[2]);
        pixel[3] = 255;
    }

    const Area &area_;
    const ViewArt &art_;
    Image image_;
};

struct Square
{
    int x{}, y{};

    [[nodiscard]] Square step(por::MapDirection direction, int count = 1) const
    {
        return {x + por::step_x(direction) * count, y + por::step_y(direction) * count};
    }
};

// The far row, two squares ahead: front walls to the left and to the right,
// the fillers between neighbouring front walls, then the side walls.
void paint_far_row(ViewPainter &painter, Square ahead, por::MapDirection facing)
{
    const auto left = por::turned_left(facing), right = por::turned_right(facing);
    for (const auto towards : {left, right})
    {
        const int sign = towards == left ? -1 : 1;
        auto square = ahead;
        std::uint8_t previous = 0;
        for (int n = 0, offset = 0; n < 4; ++n, offset += 2 * sign)
        {
            const auto front = painter.graphic(square.x, square.y, facing);
            if (front != 0)
            {
                if (previous != 0)
                {
                    painter.piece(9, offset - sign, previous);
                }
                previous = front;
                if (n < 3 && (sign < 0 || n != 0))
                {
                    painter.piece(0, offset, front);
                }
            }
            else
            {
                const auto behind = square.step(towards, -1);
                if (previous != 0 && painter.graphic(behind.x, behind.y, towards) != 0)
                {
                    painter.piece(9, offset - sign, previous);
                }
                previous = 0;
            }
            square = square.step(towards);
        }
    }
    for (const auto towards : {left, right})
    {
        const int sign = towards == left ? -1 : 1;
        auto square = ahead;
        for (int n = 0, offset = 0; n < 3; ++n, offset += 2 * sign)
        {
            const auto side = painter.graphic(square.x, square.y, towards);
            if (side != 0)
            {
                painter.piece(towards == left ? 1 : 2, offset + (n == 0 ? 0 : sign), side);
            }
            square = square.step(towards);
        }
    }
}

// The middle row, a square ahead: from each edge in, front walls and the
// side walls of the squares nearer the middle.
void paint_middle_row(ViewPainter &painter, Square ahead, por::MapDirection facing)
{
    const auto left = por::turned_left(facing), right = por::turned_right(facing);
    for (const auto edge : {left, right})
    {
        const int sign = edge == left ? -1 : 1;
        const auto inwards = por::reversed(edge);
        auto square = ahead.step(edge, 2);
        for (int n = 0, offset = 6 * sign; n < 3; ++n, offset -= 3 * sign)
        {
            const auto front = painter.graphic(square.x, square.y, facing);
            if (front != 0 && (sign < 0 || n < 2))
            {
                painter.piece(3, offset, front);
            }
            const auto side = painter.graphic(square.x, square.y, edge);
            if (n != 0 && side != 0)
            {
                painter.piece(edge == left ? 4 : 5, offset, side);
            }
            square = square.step(inwards);
        }
    }
}

// The party's own row: the front walls beside and ahead of it, and its
// square's side walls.
void paint_near_row(ViewPainter &painter, Square here, por::MapDirection facing)
{
    const auto left = por::turned_left(facing), right = por::turned_right(facing);
    for (const auto edge : {left, right})
    {
        const int sign = edge == left ? -1 : 1;
        const auto inwards = por::reversed(edge);
        auto square = here.step(edge);
        for (int n = 0, offset = 7 * sign; n < 2; ++n, offset -= 7 * sign)
        {
            const auto front = painter.graphic(square.x, square.y, facing);
            if (front != 0 && (sign < 0 || n < 1))
            {
                painter.piece(6, offset, front);
            }
            const auto side = painter.graphic(square.x, square.y, edge);
            if (n != 0 && side != 0)
            {
                painter.piece(edge == left ? 7 : 8, offset, side);
            }
            square = square.step(inwards);
        }
    }
}

} // namespace

ViewArt load_view_art(const DesignFiles &files, const Area &area)
{
    ViewArt art;
    for (std::size_t slot = 0; slot < art.sets.size(); ++slot)
    {
        const unsigned number = area.header[header_sets + slot];
        if (number == 0 || number == 0xFF)
        {
            continue;
        }
        const auto file = number >= 10 ? "8X8DC.TLB" : "8X8DB.TLB";
        const auto library = files.library(file);
        const auto what = std::string("Wall set ") + std::to_string(number);
        if (!library)
        {
            throw std::runtime_error(what + ": " + file + " is missing");
        }
        auto tiles = nested_tiles(*library, number, what);
        WallSet set;
        apply_palette_glyph(set.palette, tiles);
        set.glyphs = std::move(tiles.glyphs);
        art.sets[slot] = std::move(set);
    }
    std::optional<Library> backs;
    for (std::size_t slot = 0; slot < 4; ++slot)
    {
        unsigned number = area.header[header_backdrops + slot];
        if (number == 0)
        {
            continue;
        }
        if (!backs)
        {
            backs = files.library("BACK.TLB");
            if (!backs)
            {
                throw std::runtime_error("BACK.TLB is missing");
            }
        }
        // A backdrop from 20h on has a night picture after it.
        for (const unsigned picture : {number, number >= 0x20 ? number + 1 : number})
        {
            if (art.backdrops.contains(picture))
            {
                continue;
            }
            auto tiles = nested_tiles(*backs, picture, "Backdrop " + std::to_string(picture));
            Backdrop backdrop;
            apply_palette_glyph(backdrop.palette, tiles);
            const auto image =
                std::ranges::find_if(tiles.glyphs,
                                     [](const std::optional<Glyph> &glyph)
                                     {
                                         return glyph && glyph->kind == GlyphKind::image;
                                     });
            if (image == tiles.glyphs.end())
            {
                throw std::runtime_error("Backdrop " + std::to_string(picture) + " has no picture");
            }
            backdrop.picture = **image;
            art.backdrops.emplace(picture, std::move(backdrop));
        }
    }
    // The walls' colors outside their own run are the backdrops'.
    for (auto &set : art.sets)
    {
        if (!set || art.backdrops.empty())
        {
            continue;
        }
        auto base = art.backdrops.begin()->second.palette;
        for (std::size_t color = 0x20; color < 0x45; ++color)
        {
            base[color] = set->palette[color];
        }
        set->palette = base;
    }
    return art;
}

Image render_view(const Area &area, const ViewArt &art, unsigned x, unsigned y,
                  por::MapDirection facing, bool night)
{
    if (x >= area.columns || y >= area.rows)
    {
        throw std::out_of_range("The party is outside the area");
    }
    ViewPainter painter(area, art);
    // Sky, horizon and floor, which a backdrop covers.
    painter.fill(0, 44, {32, 48, 63});
    painter.fill(44, 46, {0, 0, 0});
    painter.fill(46, view_size, {24, 20, 16});
    const auto &square = area.squares[std::size_t{y} * area.columns + x];
    unsigned back = area.header[header_backdrops + (square.bits & 3)];
    if (back >= 0x20 && night)
    {
        ++back;
    }
    if (const auto backdrop = art.backdrops.find(back); backdrop != art.backdrops.end())
    {
        painter.draw(backdrop->second.picture, backdrop->second.palette, 0, 0);
    }
    const Square here{static_cast<int>(x), static_cast<int>(y)};
    paint_far_row(painter, here.step(facing, 2), facing);
    paint_middle_row(painter, here.step(facing), facing);
    paint_near_row(painter, here, facing);
    return std::move(painter).take();
}

} // namespace opengold::ua
