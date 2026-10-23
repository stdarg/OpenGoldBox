#include "opengold/shop_data.h"
#include "json_document.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace opengold
{
namespace
{
using detail::Json;

Json read_file(const std::filesystem::path &path)
{
    return detail::read_json_file(path, "shop data");
}

// A field the file must have, of the type it must be.
template <typename T> const T &field(const Json &object, std::string_view key)
{
    return detail::json_field<T>(object, key, "Shop data");
}

// A whole number in [low, high]; the manager writes some as 0.0.
long whole(const Json &object, std::string_view key, long low, long high)
{
    const double number = field<double>(object, key);
    if (number != std::floor(number) || number < low || number > high)
    {
        throw std::runtime_error("Invalid shop data " + std::string(key));
    }
    return static_cast<long>(number);
}

unsigned whole(const Json &value)
{
    const auto *number = std::get_if<double>(&value.value);
    if (!number || *number != std::floor(*number) || *number < 0 || *number > 255)
    {
        throw std::runtime_error("Invalid shop data marker");
    }
    return static_cast<unsigned>(*number);
}

std::vector<std::pair<unsigned, unsigned>> markers(const Json &location)
{
    std::vector<std::pair<unsigned, unsigned>> result;
    for (const auto &marker : field<Json::Array>(location, "markers"))
    {
        const auto *pair = std::get_if<Json::Array>(&marker.value);
        if (!pair || pair->size() != 2)
        {
            throw std::runtime_error("Invalid shop data marker");
        }
        result.emplace_back(whole((*pair)[0]), whole((*pair)[1]));
    }
    return result;
}

// A portrait as the game finds it, in its portraits folder; the Shop & Temple
// Manager exports its own portraits there, so records never name its folder.
std::string game_portrait(const std::string &path)
{
    const std::filesystem::path portrait(path);
    const bool outside = portrait.is_absolute() ||
                         std::ranges::find(portrait, std::filesystem::path("..")) != portrait.end();
    if (outside)
    {
        throw std::runtime_error("A shop portrait is outside the game's portraits: " + path);
    }
    return path;
}

std::vector<TempleOffering> temple_offerings(const Json &record)
{
    std::vector<TempleOffering> result;
    for (const auto &offering : field<Json::Array>(record, "offerings"))
    {
        const auto service = temple_service_named(field<std::string>(offering, "service_id"));
        if (!service)
        {
            throw std::runtime_error("Invalid temple service in data/shops");
        }
        const auto price = static_cast<std::uint32_t>(whole(offering, "price_gp", 0, 1'000'000));
        if (field<bool>(offering, "offered"))
        {
            result.push_back({*service, price});
        }
    }
    return result;
}

std::vector<ShopOffering> shop_offerings(const Json &record)
{
    std::vector<ShopOffering> result;
    for (const auto &item : field<Json::Array>(record, "offerings"))
    {
        // -1 is unlimited and a positive quantity a limit, which the game does
        // not keep yet: only 0, out of stock, changes what the shop sells.
        if (whole(item, "quantity", -1, 1'000'000) == 0)
        {
            continue;
        }
        ShopOffering offering;
        offering.item_type = static_cast<unsigned>(whole(item, "item_type", 0, 255));
        offering.name = field<std::string>(item, "name");
        // An item's value is 16 bits, as in the original's records.
        offering.price_gp = static_cast<std::uint32_t>(whole(item, "price_gp", 0, 65535));
        offering.bundle_quantity = static_cast<unsigned>(whole(item, "bundle_quantity", 1, 255));
        const auto &archive = field<std::string>(item, "source_archive");
        if (archive.starts_with("ITEM") && archive.ends_with(".DAX"))
        {
            offering.archive = archive;
            offering.record = static_cast<unsigned>(whole(item, "source_record", 0, 255));
            offering.index = static_cast<unsigned>(whole(item, "source_index", 0, 255));
        }
        else if (offering.item_type < 200)
        {
            throw std::runtime_error("A shop item has no original source: " + offering.name);
        }
        result.push_back(std::move(offering));
    }
    return result;
}
} // namespace

ShopData load_shop_data(const std::filesystem::path &directory)
{
    const auto manifest = read_file(directory / "locations.json");
    ShopData data;
    for (const auto &location : field<Json::Array>(manifest, "locations"))
    {
        std::string archive;
        unsigned record{};
        for (const auto &map : field<Json::Array>(manifest, "maps"))
        {
            if (field<std::string>(map, "name") == field<std::string>(location, "map"))
            {
                archive = field<std::string>(map, "archive");
                record = static_cast<unsigned>(whole(map, "record", 0, 255));
            }
        }
        if (archive.empty())
        {
            throw std::runtime_error("A location in data/shops names no known map");
        }
        const auto &id = field<std::string>(location, "id");
        const auto file = read_file(directory / (id + ".json"));
        const auto &kind = field<std::string>(location, "kind");
        const auto portrait = game_portrait(field<std::string>(file, "portrait"));
        if (kind == "temple")
        {
            data.temples.push_back(
                {archive, record, markers(location), temple_offerings(file), portrait});
        }
        else if (kind == "shop")
        {
            data.shops.push_back(
                {archive, record, markers(location), portrait, shop_offerings(file)});
        }
        else
        {
            throw std::runtime_error("Unknown location kind in data/shops: " + kind);
        }
    }
    return data;
}
} // namespace opengold
