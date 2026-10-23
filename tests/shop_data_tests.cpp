// data/shops as the game reads it (TEMPLE-1, #275): the shipped records'
// temples, shops, prices and portraits, and the Shop & Temple Manager's edits
// in small records written here.
#include "opengold/shop_data.h"
#include "test_support.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace opengold;
using opengold::test::check;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

const ShopSite &shop_at(const ShopData &data, unsigned x, unsigned y)
{
    const auto found = std::ranges::find_if(
        data.shops,
        [&](const auto &shop)
        {
            return std::ranges::find(shop.markers, std::pair{x, y}) != shop.markers.end();
        });
    check(found != data.shops.end(), "A shop stands on the square");
    return *found;
}

const ShopOffering &offering(const ShopSite &shop, std::string_view name)
{
    const auto found = std::ranges::find(shop.offerings, name, &ShopOffering::name);
    check(found != shop.offerings.end(), "The shop sells " + std::string(name));
    return *found;
}

std::string bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// Every portrait a record names ships with the game: the game's own from
// art/portraits, and the manager's as up-to-date copies it exported to
// data/art/portraits/shops.
void portraits_ship(const ShopData &data)
{
    std::vector<std::string> portraits;
    for (const auto &temple : data.temples)
    {
        portraits.push_back(temple.portrait);
    }
    for (const auto &shop : data.shops)
    {
        portraits.push_back(shop.portrait);
    }
    for (const auto &portrait : portraits)
    {
        check(!portrait.empty(), "Every shop and temple has a portrait");
        if (portrait.starts_with("shops/"))
        {
            const auto name = portrait.substr(6);
            const auto copy = bytes(root / "data/art/portraits/shops" / name);
            check(!copy.empty() && copy == bytes(root / "dev_tools/shop_manager/portraits" / name),
                  "The manager exported " + name + " as drawn");
        }
        else
        {
            check(std::filesystem::exists(root / "art/portraits" / portrait),
                  "The game ships " + portrait);
        }
    }
}

void shipped_records()
{
    const auto data = load_shop_data(root / "data/shops");
    check(data.temples.size() == 4 && data.shops.size() == 12,
          "Four temples and twelve shops are recorded");
    const auto &tyr = data.temples.front();
    check(tyr.archive == "GEO3.DAX" && tyr.record == 0 &&
              tyr.markers == std::vector<std::pair<unsigned, unsigned>>{{11, 3}, {8, 4}, {8, 6}},
          "The Temple of Tyr's squares in New Phlan");
    check(tyr.offerings == original_temple_offerings() && tyr.portrait == "NPCs/bishop-braccio.png",
          "Tyr offers every service at the original's prices, with Bishop Braccio");
    const auto &ilmater = data.temples.back();
    check(ilmater.archive == "GEO1.DAX" && ilmater.record == 18 &&
              ilmater.markers == std::vector<std::pair<unsigned, unsigned>>{{1, 15}} &&
              ilmater.portrait == "shops/temple-ilmater.png",
          "The Temple of Ilmater is in Podol Plaza, with a portrait drawn for the manager");

    for (const auto &[x, y, count] : std::array<std::array<unsigned, 3>, 4>{
             {{13, 8, 55}, {15, 8, 18}, {11, 10, 11}, {8, 10, 11}}})
    {
        check(shop_at(data, x, y).offerings.size() == count, "Each kind of shop's stock");
    }
    const auto &general = shop_at(data, 15, 8);
    check(general.archive == "GEO3.DAX" && general.record == 0 &&
              general.portrait == "NPCs/phlan-general-15-08.png",
          "A shop's square and its shopkeeper's portrait");
    const auto &oil = offering(general, "1 Flask OF Oil Flask(s)");
    check(oil.archive == "ITEM3.DAX" && oil.record == 54 && oil.index == 4 && oil.item_type == 86 &&
              oil.price_gp == 1,
          "An original item names its list and place in it");
    const auto &torches = offering(general, "10 Torches");
    check(torches.archive.empty() && torches.item_type == 200 && torches.bundle_quantity == 10 &&
              torches.price_gp == 1,
          "Gear OpenGoldBox adds has no source list");
    check(offering(general, "Bottle of Magic Ink (Level 8)").price_gp == 8000,
          "Magic Ink costs 1000 gp a level");
    check(offering(shop_at(data, 8, 10), "1 Diamond Necklace").price_gp == 50000,
          "The jeweler's dearest piece");
    portraits_ship(data);
}

struct Records
{
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("opengold-shop-data-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Records()
    {
        std::filesystem::create_directories(directory);
    }

    Records(const Records &) = delete;
    Records &operator=(const Records &) = delete;

    ~Records()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    void write(const std::string &name, const std::string &text) const
    {
        std::ofstream(directory / name) << text;
    }

    // One temple and one shop, as the manager writes them, with `temple` and
    // `shop` as their records' text.
    void write_all(const std::string &temple, const std::string &shop) const
    {
        write("locations.json", R"({"version": 1,
            "maps": [{"name": "New Phlan", "archive": "GEO3.DAX", "record": 0}],
            "locations": [
                {"id": "a-temple", "map": "New Phlan", "kind": "temple", "markers": [[2, 3]]},
                {"id": "a-shop", "map": "New Phlan", "kind": "shop", "markers": [[4.0, 5.0]]}]})");
        write("a-temple.json", temple);
        write("a-shop.json", shop);
    }
};

const std::string temple_record = R"({"kind": "temple", "portrait": "", "offerings": [
    {"service_id": "cure_light_wounds", "price_gp": 120, "offered": true},
    {"service_id": "cure_disease", "price_gp": 1000, "offered": false}]})";

const std::string shop_record = R"({"kind": "shop",
    "portrait": "dev_tools/shop_manager/portraits/keeper.png",
    "offerings": [
    {"name": "Long Sword", "item_type": 36, "price_gp": 17, "bundle_quantity": 1,
     "quantity": -1, "source_archive": "ITEM3.DAX", "source_record": 53, "source_index": 21},
    {"name": "Sold out", "item_type": 37, "price_gp": 8, "bundle_quantity": 1,
     "quantity": 0, "source_archive": "ITEM3.DAX", "source_record": 53, "source_index": 20},
    {"name": "Café \"Kit\"", "item_type": 212, "price_gp": 50, "bundle_quantity": 1,
     "quantity": 3, "source_archive": "OpenGold authored gear", "source_record": -1,
     "source_index": -1}]})";

void manager_edits()
{
    Records records;
    records.write_all(temple_record, shop_record);
    const auto data = load_shop_data(records.directory);
    check(data.temples.size() == 1 && data.temples[0].markers.size() == 1 &&
              data.temples[0].offerings ==
                  std::vector<TempleOffering>{{rules::TempleService::cure_light_wounds, 120}},
          "A temple offers only its Offered services, at its own prices");
    const auto &shop = data.shops.at(0);
    check(shop.markers == std::vector<std::pair<unsigned, unsigned>>{{4, 5}} &&
              shop.portrait == "shops/keeper.png" && data.temples[0].portrait.empty(),
          "A manager portrait is exported to the portraits' shops; a location may have none");
    check(shop.offerings.size() == 2 && shop.offerings[0].price_gp == 17 &&
              shop.offerings[0].index == 21,
          "An edited price is read; an item out of stock is not sold");
    check(shop.offerings[1].name == "Caf\xC3\xA9 \"Kit\"" && shop.offerings[1].archive.empty() &&
              shop.offerings[1].item_type == 212,
          "A limited item is sold, unlimited for now; escaped text is read");
}

void broken_records()
{
    const auto rejected =
        [](const std::string &temple, const std::string &shop, const std::string &message)
    {
        Records records;
        records.write_all(temple, shop);
        test::rejects(
            [&]
            {
                (void)load_shop_data(records.directory);
            },
            message);
    };
    rejected(temple_record, "{\"kind\": \"shop\",", "A cut-off file is refused");
    rejected(
        R"({"portrait": "", "offerings": [{"service_id": "heal", "price_gp": 1, "offered": true}]})",
        shop_record, "An unknown service is refused");
    auto outside = shop_record;
    outside.replace(outside.find("dev_tools/shop_manager/portraits"), 32, "elsewhere");
    rejected(temple_record, outside, "A portrait the game does not ship is refused");
    auto fraction = shop_record;
    fraction.replace(fraction.find("17"), 2, "1.5");
    rejected(temple_record, fraction, "A price in part gold is refused");
    auto sourceless = shop_record;
    sourceless.replace(sourceless.find("\"item_type\": 212"), 16, "\"item_type\": 36");
    rejected(temple_record, sourceless, "An original item without its list is refused");
}
} // namespace

int main()
{
    try
    {
        shipped_records();
        manager_edits();
        broken_records();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Shop data tests passed\n";
}
