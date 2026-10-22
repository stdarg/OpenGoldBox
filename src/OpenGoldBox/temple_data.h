#ifndef OPENGOLDBOX_TEMPLE_DATA_H
#define OPENGOLDBOX_TEMPLE_DATA_H

#include "game_resources.h"
#include "opengold/temple_catalog.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

// The temples data/shops describes (TEMPLE-1), which the Shop & Temple
// Manager edits: where each one's service opens, and the services it offers
// at its prices. Throws on a file the game cannot use.
inline std::vector<opengold::TempleSite> load_temple_sites()
{
    using namespace godot;
    const auto directory = game_data_path("data/shops");
    const auto read = [&](const String &name) -> Dictionary
    {
        Ref<JSON> json;
        json.instantiate();
        const auto file = directory.path_join(name);
        if (json->parse(FileAccess::get_file_as_string(file)) != OK ||
            json->get_data().get_type() != Variant::DICTIONARY)
        {
            throw std::runtime_error("Cannot read temple data " +
                                     std::string(file.utf8().get_data()));
        }
        return json->get_data();
    };
    const auto manifest = read("locations.json");
    const Array maps = manifest.get("maps", Array());
    const Array locations = manifest.get("locations", Array());
    std::vector<opengold::TempleSite> sites;
    for (int n = 0; n < locations.size(); ++n)
    {
        const Dictionary location = locations[n];
        if (String(location.get("kind", "")) != "temple")
        {
            continue;
        }
        opengold::TempleSite site;
        for (int m = 0; m < maps.size(); ++m)
        {
            const Dictionary map = maps[m];
            if (map.get("name", "") == location.get("map", ""))
            {
                site.archive = String(map.get("archive", "")).utf8().get_data();
                site.record = static_cast<unsigned>(int(map.get("record", -1)));
            }
        }
        const Array markers = location.get("markers", Array());
        for (int m = 0; m < markers.size(); ++m)
        {
            const Array marker = markers[m];
            site.markers.emplace_back(static_cast<unsigned>(int(marker[0])),
                                      static_cast<unsigned>(int(marker[1])));
        }
        const auto record = read(String(location.get("id", "")) + ".json");
        const Array offerings = record.get("offerings", Array());
        for (int m = 0; m < offerings.size(); ++m)
        {
            const Dictionary offering = offerings[m];
            const auto service = opengold::temple_service_named(
                String(offering.get("service_id", "")).utf8().get_data());
            const int price = offering.get("price_gp", -1);
            if (!service || price < 0)
            {
                throw std::runtime_error("Invalid temple service in data/shops");
            }
            if (bool(offering.get("offered", false)))
            {
                site.offerings.push_back({*service, static_cast<std::uint32_t>(price)});
            }
        }
        if (site.archive.empty())
        {
            throw std::runtime_error("A temple in data/shops names no known map");
        }
        sites.push_back(std::move(site));
    }
    return sites;
}

#endif
