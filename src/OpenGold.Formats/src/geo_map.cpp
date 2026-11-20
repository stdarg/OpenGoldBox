#include "opengold/geo_map.h"
#include <stdexcept>

namespace opengold::por
{
bool GeoMap::valid_geometry() const noexcept
{
    return width && height && width <= max_cells && height <= max_cells &&
           std::size_t{width} * height == cells.size() && cells.size() <= max_cells;
}

const MapCell &GeoMap::at(unsigned x, unsigned y) const
{
    if (!valid_geometry() || x >= width || y >= height)
    {
        throw std::out_of_range("Map coordinate outside grid");
    }
    return cells.at(std::size_t{y} * width + x);
}

unsigned GeoMap::wrapped_x(int x) const
{
    if (!valid_geometry())
    {
        throw std::out_of_range("Invalid map width");
    }
    const int extent = static_cast<int>(width);
    return static_cast<unsigned>((x % extent + extent) % extent);
}

unsigned GeoMap::wrapped_y(int y) const
{
    if (!valid_geometry())
    {
        throw std::out_of_range("Invalid map height");
    }
    const int extent = static_cast<int>(height);
    return static_cast<unsigned>((y % extent + extent) % extent);
}

std::optional<GeoMap> decode_geo_map(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 1026)
    {
        return std::nullopt;
    }
    GeoMap map;
    map.raw.assign(bytes.begin(), bytes.end());
    for (std::size_t i = 0; i < map.cells.size(); ++i)
    {
        auto &c = map.cells[i];
        c.walls = {static_cast<std::uint8_t>(bytes[2 + i] >> 4),
                   static_cast<std::uint8_t>(bytes[2 + i] & 15),
                   static_cast<std::uint8_t>(bytes[258 + i] >> 4),
                   static_cast<std::uint8_t>(bytes[258 + i] & 15)};
        c.event_raw = bytes[514 + i];
        for (unsigned d = 0; d < 4; ++d)
        {
            c.doors[d] = (bytes[770 + i] >> (2 * d)) & 3;
        }
    }
    return map;
}
} // namespace opengold::por
