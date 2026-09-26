#ifndef OPENGOLD_GEO_MAP_H
#define OPENGOLD_GEO_MAP_H
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace opengold::por
{
enum class MapDirection : std::size_t
{
    north,
    east,
    south,
    west
};
struct MapCell
{
    std::array<std::uint8_t, 4> walls{},
        doors{}; // N, E, S, W; door codes retained, not collision rules.
    std::uint8_t event_raw{};
    [[nodiscard]] unsigned event_number() const noexcept
    {
        return event_raw & 127;
    }
    [[nodiscard]] bool event_high_bit() const noexcept
    {
        return (event_raw & 128) != 0;
    }
};
struct GeoMap
{
    static constexpr unsigned width = 16, height = 16;
    std::array<MapCell, width * height> cells{};
    std::vector<std::uint8_t> raw; // Includes the uninterpreted header and trailing data.
    [[nodiscard]] const MapCell &at(unsigned x, unsigned y) const;
};
// Reference PoR layout requires at least 1026 bytes; extra bytes are retained.
[[nodiscard]] std::optional<GeoMap> decode_geo_map(std::span<const std::uint8_t> bytes);
} // namespace opengold::por
#endif
