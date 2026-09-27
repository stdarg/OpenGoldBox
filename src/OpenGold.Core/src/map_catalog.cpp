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
                continue;
            auto name = entry.path().filename().string();
            for (auto &c : name)
                if (c >= 'a' && c <= 'z')
                    c -= 'a' - 'A';
            if (!name.starts_with("GEO") || !name.ends_with(".DAX"))
                continue;
            const auto bank = name.substr(3, name.size() - 7);
            if (!std::all_of(bank.begin(), bank.end(),
                             [](char c)
                             {
                                 return c >= '0' && c <= '9';
                             }))
                continue;
            if (!archives.emplace(name, entry.path()).second)
                throw MapError("Ambiguous map archive " + name);
        }
        MapCatalog catalog;
        for (const auto &[name, path] : archives)
        {
            const auto size = std::filesystem::file_size(path);
            if (size > 32 * 1024 * 1024)
                throw MapError("Map archive too large: " + name);
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw MapError("Cannot open " + name);
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
            if (input.bad() || bytes.size() != size)
                throw MapError("Cannot read " + name);
            auto dax = decode_dax_archive(bytes);
            if (!dax)
                throw MapError("Invalid DAX archive " + name);
            for (const auto &record : dax.records)
            {
                auto map = decode_geo_map(record.bytes);
                if (!map)
                    throw MapError("Truncated GEO map " + name + ":" + std::to_string(record.id));
                catalog.maps_.emplace(MapId{name, record.id}, std::move(*map));
            }
        }
        if (catalog.maps_.empty())
            throw MapError("No GEO maps found in " + directory.string());
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
        return std::nullopt;
    return std::cref(it->second);
}
} // namespace opengold::por
