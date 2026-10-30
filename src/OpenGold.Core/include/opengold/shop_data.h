#ifndef OPENGOLD_SHOP_DATA_H
#define OPENGOLD_SHOP_DATA_H
#include "opengold/geo_map.h"
#include "opengold/temple_catalog.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace opengold
{
// An item a shop's record sells: an original item from an ITEMn.DAX list, or
// gear OpenGoldBox adds (an authored item type, with no source list). An
// original item may be sold enchanted, as the Rogue's Shop sells the armory's
// Leather Armor with +1 (SECRET-1).
struct ShopOffering
{
    std::string archive; // "ITEM3.DAX", or empty for authored gear
    unsigned record{}, index{};
    unsigned item_type{};
    std::string name;
    std::uint32_t price_gp{};
    unsigned bundle_quantity{1};
    int magic_bonus{}; // added to an original item's own
    bool operator==(const ShopOffering &) const = default;
};

// The way into a shop OpenGoldBox adds (SECRET-1): a secret, locked door on
// one side of its square, drawn with the map's `door_wall` once found, and
// walls closing the square's other open sides.
struct HiddenEntrance
{
    por::MapDirection secret_door{};
    std::uint8_t door_wall{};
    std::vector<std::pair<por::MapDirection, std::uint8_t>> walls;
    bool operator==(const HiddenEntrance &) const = default;
};

// A shop the game knows from data/shops: the map squares where it opens, its
// shopkeeper's portrait (relative to the game's portraits folder; empty for
// none) and what it sells. Items out of stock (quantity 0) are left out; every
// other item is unlimited. A shop with a hidden entrance is OpenGoldBox's own:
// the game opens it on its square in place of the square's original event.
struct ShopSite
{
    std::string id; // its data/shops record, "phlan-arms-13-08"
    std::string archive; // "GEO3.DAX"
    unsigned record{};
    std::vector<std::pair<unsigned, unsigned>> markers;
    std::string portrait;
    std::vector<ShopOffering> offerings;
    std::optional<HiddenEntrance> entrance;
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
