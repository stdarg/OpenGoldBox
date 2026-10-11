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

// Facing, turning and stepping on a map, named so callers do not pass or
// repeat arithmetic on bare 0..3 numbers (Effective C++ Item 18).
[[nodiscard]] constexpr std::size_t index(MapDirection direction) noexcept
{
    return static_cast<std::size_t>(direction);
}

[[nodiscard]] constexpr MapDirection turned_left(MapDirection direction) noexcept
{
    return MapDirection{(index(direction) + 3) % 4};
}

[[nodiscard]] constexpr MapDirection turned_right(MapDirection direction) noexcept
{
    return MapDirection{(index(direction) + 1) % 4};
}

[[nodiscard]] constexpr MapDirection reversed(MapDirection direction) noexcept
{
    return MapDirection{(index(direction) + 2) % 4};
}

// One square's step in that direction; north is towards smaller y.
[[nodiscard]] constexpr int step_x(MapDirection direction) noexcept
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1};
    return dx[index(direction)];
}

[[nodiscard]] constexpr int step_y(MapDirection direction) noexcept
{
    constexpr std::array<int, 4> dy{-1, 0, 1, 0};
    return dy[index(direction)];
}

// A facing read from a script variable or other outside value: 0..3 or nothing.
[[nodiscard]] constexpr std::optional<MapDirection> map_direction(unsigned value) noexcept
{
    if (value >= 4)
        return std::nullopt;
    return MapDirection{value};
}

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
