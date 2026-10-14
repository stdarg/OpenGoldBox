#ifndef OPENGOLD_TILE_MAP_ART_PROFILES_H
#define OPENGOLD_TILE_MAP_ART_PROFILES_H

#include <array>
#include <span>
#include <string_view>

struct MapArtProfileSpec
{
    unsigned bank;
    unsigned map_record;
    std::string_view name;
    std::string_view label;
    std::string_view source;
    // WALLDEF record IDs in appearance order; -1 means no further record.
    std::array<int, 3> wall_records;
};

[[nodiscard]] std::span<const MapArtProfileSpec> map_art_profiles() noexcept;

#endif
