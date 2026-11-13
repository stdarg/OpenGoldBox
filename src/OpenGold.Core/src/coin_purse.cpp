#include "opengold/coin_purse.h"
#include <algorithm>
#include <stdexcept>

namespace opengold
{
namespace
{
// The purse's coins' worth in 200ths of a gold piece.
std::uint64_t coin_value(const Purse &purse, const CoinRates &rates)
{
    std::uint64_t total = 0;
    for (std::size_t coin = 0; coin < rates.worth.size(); ++coin)
    {
        total += purse[coin] * rates.worth[coin];
    }
    return total;
}

void add_coins(Purse &purse, std::size_t coin, std::uint64_t count)
{
    if (purse[coin] + count > purse_limit)
    {
        throw std::runtime_error("The purse cannot hold its change");
    }
    purse[coin] = static_cast<std::uint32_t>(purse[coin] + count);
}

// Change comes back as gold, silver and copper, the coins players expect.
void give_change(Purse &purse, Coins &change, std::uint64_t copper, const CoinRates &rates)
{
    for (const std::size_t coin : {3, 1, 0})
    {
        const auto count = copper / rates.worth[coin];
        copper -= count * rates.worth[coin];
        add_coins(purse, coin, count);
        change[coin] = static_cast<std::uint32_t>(change[coin] + count);
    }
}

// Pays `copper` (200ths of a gold piece) from the largest coins that do not
// overpay, then breaks the smallest remaining coin. Every coin left after the
// first pass is worth more than what is still owed, so that one coin always
// covers it.
void pay(Purse &purse, CoinExchange &exchange, std::uint64_t copper, const CoinRates &rates)
{
    for (std::size_t coin = rates.worth.size(); coin-- > 0;)
    {
        const auto count = std::min<std::uint64_t>(purse[coin], copper / rates.worth[coin]);
        purse[coin] = static_cast<std::uint32_t>(purse[coin] - count);
        exchange.paid[coin] = static_cast<std::uint32_t>(exchange.paid[coin] + count);
        copper -= count * rates.worth[coin];
    }
    if (!copper)
    {
        return;
    }
    std::size_t coin = 0;
    while (!purse[coin])
    {
        ++coin;
    }
    --purse[coin];
    ++exchange.paid[coin];
    give_change(purse, exchange.change, rates.worth[coin] - copper, rates);
}
} // namespace

std::uint64_t gold_value(const Purse &purse, const CoinRates &rates)
{
    return coin_value(purse, rates) / rates.worth[static_cast<std::size_t>(Coin::gold)];
}

std::uint32_t spendable_gold(const Purse &purse, const CoinRates &rates)
{
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(gold_value(purse, rates), purse_limit));
}

SettledPurse pay_gold(const Purse &purse, std::uint64_t gold, const CoinRates &rates)
{
    constexpr auto gold_coin = static_cast<std::size_t>(Coin::gold);
    if (gold > gold_value(purse, rates))
    {
        throw std::runtime_error("The party's coins cannot pay the price");
    }
    SettledPurse result{purse, {}};
    auto &settled = result.purse;
    if (settled[gold_coin] >= gold)
    {
        settled[gold_coin] = static_cast<std::uint32_t>(settled[gold_coin] - gold);
        return result;
    }
    CoinExchange exchange;
    exchange.owed[gold_coin] = static_cast<std::uint32_t>(gold);
    pay(settled, exchange, gold * rates.worth[gold_coin], rates);
    result.exchange = exchange;
    return result;
}

Purse script_coins(const Purse &purse, const CoinRates &rates)
{
    Purse view;
    const auto total = coin_value(purse, rates);
    for (std::size_t coin = 0; coin < rates.worth.size(); ++coin)
    {
        view[coin] = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(script_coin_limit, total / rates.worth[coin]));
    }
    for (const auto valuable : {Coin::gems, Coin::jewelry})
    {
        coins(view, valuable) = std::min(coins(purse, valuable), script_coin_limit);
    }
    return view;
}

SettledPurse settle_script_coins(const Purse &purse, const Purse &after_script,
                                 const CoinRates &rates)
{
    const auto before_script = script_coins(purse, rates);
    SettledPurse result{purse, {}};
    auto &settled = result.purse;
    // Gems and jewelry change as the script changed them. The script was shown
    // no more than the purse holds, so a loss always comes out of the purse.
    for (const auto valuable : {Coin::gems, Coin::jewelry})
    {
        const auto shown = coins(before_script, valuable);
        const auto left = coins(after_script, valuable);
        if (left >= shown)
        {
            add_coins(settled, static_cast<std::size_t>(valuable), left - shown);
        }
        else
        {
            coins(settled, valuable) -= shown - left;
        }
    }
    Coins owed{};
    for (std::size_t coin = 0; coin < rates.worth.size(); ++coin)
    {
        if (after_script[coin] >= before_script[coin])
        {
            add_coins(settled, coin, after_script[coin] - before_script[coin]);
        }
        else
        {
            owed[coin] = before_script[coin] - after_script[coin];
        }
    }
    CoinExchange exchange;
    std::uint64_t unpaid = 0;
    for (std::size_t coin = 0; coin < rates.worth.size(); ++coin)
    {
        const auto held = std::min(owed[coin], settled[coin]);
        settled[coin] -= held;
        exchange.owed[coin] = owed[coin] - held;
        unpaid += exchange.owed[coin] * rates.worth[coin];
    }
    if (!unpaid)
    {
        return result;
    }
    if (unpaid > coin_value(settled, rates))
    {
        throw std::runtime_error("A script took more coins than the purse holds");
    }
    pay(settled, exchange, unpaid, rates);
    result.exchange = exchange;
    return result;
}
} // namespace opengold
