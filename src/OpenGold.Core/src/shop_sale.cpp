#include "opengold/shop_sale.h"
#include "opengold/campaign_party.h"
#include <algorithm>
#include <cstdint>
#include <optional>

namespace opengold
{
namespace
{
// The name words "+1" to "+5" (docs/MAGIC_ITEMS.md). A shop's Leather Armor +1
// is the armory's leather with only its bonus raised, so these words are left
// out when items are compared.
bool enchantment_word(std::uint8_t word)
{
    return word >= 0xA2 && word <= 0xA6;
}

// An item as a shop prices it: its base type, its enchantment and its other
// name words, which tell a Silver Short Sword from a plain one.
struct PriceKey
{
    unsigned type{};
    int magic_bonus{};
    std::vector<std::uint8_t> words;

    bool operator==(const PriceKey &) const = default;
};

PriceKey price_key(const por::ItemRecord &record)
{
    PriceKey key{.type = record.type, .magic_bonus = record.magic_bonus, .words = {}};
    for (const auto word : record.name_components)
    {
        if (word && !enchantment_word(word))
        {
            key.words.push_back(word);
        }
    }

    return key;
}

// The unenchanted version of an item not yet identified: the words the party
// can see, with no bonus.
PriceKey unenchanted_key(const por::ItemRecord &record)
{
    PriceKey key{.type = record.type, .magic_bonus = 0, .words = {}};
    for (std::size_t n = 0; n < record.name_components.size(); ++n)
    {
        const auto word = record.name_components[n];
        const bool hidden = (record.hidden_name_words & (4u >> n)) != 0;
        if (word && !hidden && !enchantment_word(word))
        {
            key.words.push_back(word);
        }
    }

    return key;
}

// A price in gold pieces for `quantity` items of a stack, or charges.
struct Price
{
    std::uint64_t gold{};
    unsigned quantity{1};
};

Price price_of(const por::Equipment &item)
{
    return {.gold = item.stored.value, .quantity = std::max(carried_quantity(item), 1u)};
}

// The price a shop in town asks for the item `key` names, the party's shop
// first; the shops ask the same for the same item.
std::optional<Price> town_price(const PriceKey &key, const SaleMarket &market)
{
    const auto in_stock = [&](std::span<const por::Equipment> stock) -> std::optional<Price>
    {
        const auto found = std::find_if(stock.begin(), stock.end(),
                                        [&](const auto &offered)
                                        {
                                            return price_key(offered.stored) == key;
                                        });
        if (found == stock.end())
        {
            return std::nullopt;
        }

        return price_of(*found);
    };
    if (const auto here = in_stock(market.shop))
    {
        return here;
    }

    for (const auto &stock : market.town)
    {
        if (const auto there = in_stock(stock))
        {
            return there;
        }
    }

    return std::nullopt;
}

bool sells_base_type(const por::Equipment &item, const SaleMarket &market)
{
    return std::any_of(market.shop.begin(), market.shop.end(),
                       [&](const auto &offered)
                       {
                           return offered.stored.type == item.stored.type;
                       });
}
} // namespace

unsigned shop_offer(const por::Equipment &item, unsigned quantity_left, int charisma_modifier,
                    const SaleMarket &market)
{
    // An item not yet identified could be cursed: the shop pays half what it
    // would for the unenchanted version, and nothing when it sells none. A
    // known item no shop sells is priced by its own stored value.
    const bool identified = item.identified();
    const std::optional<Price> price =
        identified ? town_price(price_key(item.stored), market).value_or(price_of(item))
                   : town_price(unenchanted_key(item.stored), market);
    if (!price)
    {
        return 0;
    }

    // Half the price; 10% of that per point of Charisma modifier; 25% less at
    // a shop that does not sell the base type. Rounded down once, at the end.
    const std::uint64_t charisma_tenths =
        static_cast<std::uint64_t>(std::max(0, 10 + charisma_modifier));
    const std::uint64_t shop_quarters = sells_base_type(item, market) ? 4 : 3;
    const std::uint64_t unidentified_halving = identified ? 1 : 2;
    const std::uint64_t paid = price->gold * quantity_left * charisma_tenths * shop_quarters;
    const std::uint64_t whole = std::uint64_t{price->quantity} * 2 * 10 * 4 * unidentified_halving;

    return static_cast<unsigned>(paid / whole);
}
} // namespace opengold
