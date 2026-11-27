#ifndef OPENGOLD_SHOP_SERVICE_H
#define OPENGOLD_SHOP_SERVICE_H

#include "opengold/equipment.h"
#include "opengold/shop_data.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace opengold::por
{
struct PhlanResources;
class TownServiceHost;

// The town's shops (host_shops.cpp): what each sells, and the shop the party
// is in.
class ShopService
{
  public:
    // The shops data/shops describes, each selling its record's items at its
    // prices from the original lists of `town`; see RolfTourSession::shops.
    void shops(const PhlanResources *town, std::vector<opengold::ShopSite> sites,
               opengold::ShopStock stock);

    [[nodiscard]] const std::vector<opengold::ShopSite> &shops() const noexcept
    {
        return shops_;
    }

    // Every shop's items, in the order of shops(), at their prices (SELL-1).
    [[nodiscard]] const std::vector<std::vector<Equipment>> &town_stock() const noexcept
    {
        return shop_stocks_;
    }

    // The items the next shop sells, which the script's TREASURE lists add
    // to, and the open shop's.
    [[nodiscard]] std::vector<Equipment> &shop_stock() noexcept
    {
        return treasure_;
    }

    [[nodiscard]] const std::vector<Equipment> &shop_stock() const noexcept
    {
        return treasure_;
    }

    // The shop data/shops places on the party's square, if any, stocks the
    // next shop with its record's items.
    bool stock_from_site(const TownServiceHost &host);
    // Opens the shop screen on shop_stock(), answering the script's `request`
    // when the party leaves, or 0 for a shop the party walked into.
    void enter_shop(TownServiceHost &host, std::uint64_t request);
    // OpenGoldBox's shop on the party's square opens (SECRET-1).
    void open_own_shop(TownServiceHost &host);
    bool buy(TownServiceHost &host, std::uint64_t ticket, std::size_t item);
    bool leave_shop(TownServiceHost &host, std::uint64_t ticket);

    // Whether the party left a shop since the last call: once the shop's
    // script ends, the party stands outside its door.
    [[nodiscard]] bool take_step_out() noexcept
    {
        return std::exchange(step_out_after_shop_, false);
    }

    // Forgets the open shop and its stock, as a new run of the tour, a
    // rolled-back event or a loaded save does.
    void close() noexcept
    {
        treasure_.clear();
        shop_request_ = 0;
    }

  private:
    std::vector<opengold::ShopSite> shops_;
    std::vector<std::vector<Equipment>> shop_stocks_; // each shop's items, as shops_
    std::vector<Equipment> treasure_;
    // The script's shop request the party is shopping for; 0 at a shop
    // OpenGoldBox opens itself (SECRET-1).
    std::uint64_t shop_request_{};
    // Set when the party leaves a shop: once the shop's script ends, the party
    // stands outside its door (step_out_of_shop).
    bool step_out_after_shop_{};
};
} // namespace opengold::por
#endif
