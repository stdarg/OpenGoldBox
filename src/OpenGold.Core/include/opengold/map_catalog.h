#ifndef OPENGOLD_MAP_CATALOG_H
#define OPENGOLD_MAP_CATALOG_H
#include "opengold/geo_map.h"
#include <compare>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

namespace opengold::por
{
struct MapId
{
    std::string archive; // Uppercase DOS filename, preserves archive namespace.
    std::uint8_t record{};
    auto operator<=>(const MapId &) const = default;
};

class MapError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

class MapCatalog
{
  public:
    // Loads GEO.DAX and GEO<number>.DAX, case-insensitively. Atomic failure on
    // malformed input, ambiguous filenames, or no map records. No borrowed files.
    [[nodiscard]] static MapCatalog load(const std::filesystem::path &directory);

    [[nodiscard]] const std::map<MapId, GeoMap> &all() const noexcept
    {
        return maps_;
    }

    // References borrow from this catalog. IDs use canonical uppercase filenames.
    [[nodiscard]] std::optional<std::reference_wrapper<const GeoMap>> find(const MapId &id) const;

  private:
    MapCatalog() = default;
    std::map<MapId, GeoMap> maps_;
};
} // namespace opengold::por
#endif
