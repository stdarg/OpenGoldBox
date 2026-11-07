#ifndef OPENGOLD_SHOP_SALE_H
#define OPENGOLD_SHOP_SALE_H

#include "opengold/equipment.h"
#include <span>
#include <vector>

namespace opengold
{
// The prices a shopkeeper judges a sale by (SELL-1): the stock of the shop the
// party is in, and the stock of every shop in town. Each stocked item's stored
// value is its price for its stack or charges.
struct SaleMarket
{
    std::span<const por::Equipment> shop;
    std::span<const std::vector<por::Equipment>> town;
};

// What a shopkeeper pays for `item`, in gold pieces, rounded down (SELL-1):
// - half the town's price for the same item, or half its own stored value when
//   no shop sells it;
// - 10% of that more or less for each point of `charisma_modifier`;
// - 25% less when the shop sells nothing of the item's base type;
// - for an item not yet identified, half the offer for its unenchanted version.
// `quantity_left` is how much of the item's stack or charges is still carried.
[[nodiscard]] unsigned shop_offer(const por::Equipment &item, unsigned quantity_left,
                                  int charisma_modifier, const SaleMarket &market);
} // namespace opengold
#endif
