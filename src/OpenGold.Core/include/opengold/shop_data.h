#ifndef OPENGOLD_SHOP_DATA_H
#define OPENGOLD_SHOP_DATA_H
#include "opengold/temple_catalog.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace opengold
{
// An item a shop's record sells: an original item from an ITEMn.DAX list, or
// gear OpenGoldBox adds (an authored item type, with no source list).
struct ShopOffering
{
    std::string archive; // "ITEM3.DAX", or empty for authored gear
    unsigned record{}, index{};
    unsigned item_type{};
    std::string name;
    std::uint32_t price_gp{};
    unsigned bundle_quantity{1};
    bool operator==(const ShopOffering &) const = default;
};

// A shop the game knows from data/shops: the map squares where it opens, its
// shopkeeper's portrait (relative to the game's portraits folder) and what it
// sells. Items out of stock (quantity 0) are left out; every other item is
// unlimited.
struct ShopSite
{
    std::string archive; // "GEO3.DAX"
    unsigned record{};
    std::vector<std::pair<unsigned, unsigned>> markers;
    std::string portrait;
    std::vector<ShopOffering> offerings;
};

// Everything data/shops describes, which the Shop & Temple Manager edits.
struct ShopData
{
    std::vector<TempleSite> temples;
    std::vector<ShopSite> shops;
};

// Reads data/shops: locations.json and each location's record. Throws on a
// file the game cannot use.
[[nodiscard]] ShopData load_shop_data(const std::filesystem::path &directory);
} // namespace opengold
#endif
