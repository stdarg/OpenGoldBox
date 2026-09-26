#include "opengold/geo_map.h"
#include <stdexcept>
namespace opengold::por
{
const MapCell &GeoMap::at(unsigned x, unsigned y) const
{
    if (x >= width || y >= height)
        throw std::out_of_range("Map coordinate outside 16x16 grid");
    return cells[y * width + x];
}
std::optional<GeoMap> decode_geo_map(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 1026)
        return std::nullopt;
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
            c.doors[d] = (bytes[770 + i] >> (2 * d)) & 3;
    }
    return map;
}
} // namespace opengold::por
