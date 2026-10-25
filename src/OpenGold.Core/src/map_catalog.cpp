#include "archive_io.h"
#include "opengold/path_text.h"
#include "opengold/map_catalog.h"
#include "opengold/formats.h"
#include <algorithm>
#include <fstream>
#include <iterator>

namespace opengold::por
{
MapCatalog MapCatalog::load(const std::filesystem::path &directory)
{
    try
    {
        std::map<std::string, std::filesystem::path> archives;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            auto name = detail::upper_ascii(path_text(entry.path().filename()));
            if (!name.starts_with("GEO") || !name.ends_with(".DAX"))
            {
                continue;
            }
            const auto bank = name.substr(3, name.size() - 7);
            if (!std::all_of(bank.begin(), bank.end(),
                             [](char c)
                             {
                                 return c >= '0' && c <= '9';
                             }))
            {
                continue;
            }
            if (!archives.emplace(name, entry.path()).second)
            {
                throw MapError("Ambiguous map archive " + name);
            }
        }
        MapCatalog catalog;
        for (const auto &[name, path] : archives)
        {
            auto bytes =
                detail::read_archive_bytes<MapError>(path, "Map archive too large: " + name,
                                                     "Cannot open " + name, "Cannot read " + name);
            auto dax = decode_dax_archive(bytes);
            if (!dax)
            {
                throw MapError("Invalid DAX archive " + name);
            }
            for (const auto &record : dax.records)
            {
                auto map = decode_geo_map(record.bytes);
                if (!map)
                {
                    throw MapError("Truncated GEO map " + name + ":" + std::to_string(record.id));
                }
                catalog.maps_.emplace(MapId{name, record.id}, std::move(*map));
            }
        }
        if (catalog.maps_.empty())
        {
            throw MapError("No GEO maps found in " + path_text(directory));
        }
        return catalog;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        throw MapError(e.what());
    }
}

std::optional<std::reference_wrapper<const GeoMap>> MapCatalog::find(const MapId &id) const
{
    const auto it = maps_.find(id);
    if (it == maps_.end())
    {
        return std::nullopt;
    }
    return std::cref(it->second);
}
} // namespace opengold::por
