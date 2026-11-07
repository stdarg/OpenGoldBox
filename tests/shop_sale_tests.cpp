// What New Phlan's shops pay for the original's items (SELL-1): half the
// town's price, bargained by Charisma, 25% less at a shop that sells nothing
// of the item's base type, and half the unenchanted price for an item not yet
// identified. Needs the installed game (OPENGOLD_GAME_DIR); without it the
// test passes.
#include "opengold/random_treasure.h"
#include "opengold/rolf_tour.h"
#include "opengold/shop_data.h"
#include "opengold/shop_sale.h"
#include "repository_data.h"
#include "test_support.h"
#include "tour_access.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>

using namespace opengold;
using namespace opengold::por;
using opengold::test::check;

namespace
{
constexpr std::uint8_t long_sword = 36, short_sword = 37, leather = 50;
constexpr std::uint8_t plus_one = 0xA2;

// The market of the shop `id` names, in a town whose shops are data/shops'.
SaleMarket market_at(const RolfTourSession &town, std::string_view id)
{
    const auto &sites = town.shops();
    const auto site = std::find_if(sites.begin(), sites.end(),
                                   [&](const auto &shop)
                                   {
                                       return shop.id == id;
                                   });
    check(site != sites.end(), "data/shops has " + std::string(id));
    return {.shop = town.town_stock()[static_cast<std::size_t>(site - sites.begin())],
            .town = town.town_stock()};
}

// The first item any creature carries that `wanted` picks.
template <typename Predicate>
Equipment carried(const std::vector<Equipment> &gear, Predicate wanted, const std::string &what)
{
    const auto found = std::find_if(gear.begin(), gear.end(), wanted);
    check(found != gear.end(), "A creature carries " + what);
    return *found;
}

void monster_gear(const RolfTourSession &town)
{
    const auto gear = test::TourAccess::creature_equipment(town);
    const auto armory = market_at(town, "phlan-arms-13-08");
    const auto general_store = market_at(town, "phlan-general-09-10");
    const auto rogue_shop = market_at(town, "phlan-rogue-04-13");

    const auto cheap_sword = carried(
        gear,
        [](const auto &item)
        {
            return item.stored.type == short_sword && !item.stored.magic_bonus &&
                   item.stored.value < 8 &&
                   item.stored.name_components == std::array<std::uint8_t, 3>{0, 0, short_sword};
        },
        "a plain Short Sword its record values under 8 gp");
    check(shop_offer(cheap_sword, 1, 0, armory) == 4,
          "The armory pays half its 8 gp for a monster's Short Sword");
    check(shop_offer(cheap_sword, 1, 0, general_store) == 3,
          "The general store pays 25% less for it");
    check(shop_offer(cheap_sword, 1, 3, armory) == 5, "Charisma +3 gets 5 gp for it");

    // A creature's magic gear comes with its "+N" hidden.
    const auto found_leather = carried(
        gear,
        [](const auto &item)
        {
            return item.stored.type == leather && item.stored.magic_bonus == 1 &&
                   item.stored.name_components[0] == plus_one;
        },
        "Leather Armor +1");
    check(!found_leather.identified() && shop_offer(found_leather, 1, 0, rogue_shop) == 1,
          "Unidentified, it fetches half of plain leather's 2.5 gp");
    auto known_leather = found_leather;
    known_leather.identify();
    check(shop_offer(known_leather, 1, 0, rogue_shop) == 2000,
          "Identified, the Rogue's Shop pays half its 4,000 gp for it");

    const auto magic_sword = carried(
        gear,
        [](const auto &item)
        {
            return item.stored.type == long_sword && item.stored.magic_bonus == 1;
        },
        "a Long Sword +1");
    auto known_sword = magic_sword;
    known_sword.identify();
    check(shop_offer(known_sword, 1, 0, armory) == known_sword.stored.value / 2,
          "No shop sells a Long Sword +1: the armory pays half its own value");
}

// Random treasure's enchanted Long Sword, its "+N" hidden, fetches half what
// the armory pays for a plain Long Sword (7 gp): 3 gp.
void unidentified(const RolfTourSession &town)
{
    std::mt19937 dice(7);
    const DieRoller roll = [&](unsigned sides)
    {
        return std::uniform_int_distribution<unsigned>(1, sides)(dice);
    };
    for (int tries = 0; tries < 10000; ++tries)
    {
        for (auto &record : random_treasure_items(1, roll))
        {
            if (record.type != long_sword)
            {
                continue;
            }
            Equipment sword;
            sword.stored = std::move(record);
            check(!sword.identified(), "Random treasure's Long Sword comes unidentified");
            check(shop_offer(sword, 1, 0, market_at(town, "phlan-arms-13-08")) == 3,
                  "An unidentified Long Sword fetches half of a Long Sword's 7 gp");
            return;
        }
    }
    check(false, "Random treasure rolled a Long Sword");
}
} // namespace

int main()
{
    const char *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !std::filesystem::exists(directory))
    {
        std::cout << "Shop sale checks skipped: OPENGOLD_GAME_DIR is not set\n";
        return 0;
    }

    try
    {
        auto town = RolfTourSession::load(directory, repository_encounters());
        town.shops(load_shop_data(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/shops").shops);
        monster_gear(town);
        unidentified(town);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    std::cout << "Shop sale checks passed\n";
    return 0;
}
