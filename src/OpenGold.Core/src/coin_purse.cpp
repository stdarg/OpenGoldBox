#include "opengold/coin_purse.h"
#include <algorithm>
#include <stdexcept>

namespace opengold
{
namespace
{
// SRD exchange rates in copper pieces: cp, sp, ep, gp, pp.
constexpr std::array<std::uint64_t, 5> copper_value{1, 10, 50, 100, 1000};

std::uint64_t coin_value(const Purse &purse)
{
    std::uint64_t total = 0;
    for (std::size_t coin = 0; coin < copper_value.size(); ++coin)
        total += purse[coin] * copper_value[coin];
    return total;
}

void add_coins(Purse &purse, std::size_t coin, std::uint64_t count)
{
    if (purse[coin] + count > 65535)
        throw std::runtime_error("The purse cannot hold its change");
    purse[coin] = static_cast<std::uint16_t>(purse[coin] + count);
}

// Change comes back as gold, silver and copper, the coins players expect.
void give_change(Purse &purse, Coins &change, std::uint64_t copper)
{
    for (const std::size_t coin : {3, 1, 0})
    {
        const auto count = copper / copper_value[coin];
        copper -= count * copper_value[coin];
        add_coins(purse, coin, count);
        change[coin] = static_cast<std::uint16_t>(change[coin] + count);
    }
}

// Pays `copper` from the largest coins that do not overpay, then breaks the
// smallest remaining coin. Every coin left after the first pass is worth more
// than what is still owed, so that one coin always covers it.
void pay(Purse &purse, CoinExchange &exchange, std::uint64_t copper)
{
    for (std::size_t coin = copper_value.size(); coin-- > 0;)
    {
        const auto count = std::min<std::uint64_t>(purse[coin], copper / copper_value[coin]);
        purse[coin] = static_cast<std::uint16_t>(purse[coin] - count);
        exchange.paid[coin] = static_cast<std::uint16_t>(exchange.paid[coin] + count);
        copper -= count * copper_value[coin];
    }
    if (!copper)
        return;
    std::size_t coin = 0;
    while (!purse[coin])
        ++coin;
    --purse[coin];
    ++exchange.paid[coin];
    give_change(purse, exchange.change, copper_value[coin] - copper);
}
} // namespace

Purse script_coins(const Purse &purse)
{
    auto view = purse;
    const auto total = coin_value(purse);
    for (std::size_t coin = 0; coin < copper_value.size(); ++coin)
        view[coin] = static_cast<std::uint16_t>(
                         std::min<std::uint64_t>(65535, total / copper_value[coin]));
    return view;
}

SettledPurse settle_script_coins(const Purse &purse, const Purse &after_script)
{
    const auto before_script = script_coins(purse);
    SettledPurse result{purse, {}};
    auto &settled = result.purse;
    settled[5] = after_script[5];
    settled[6] = after_script[6];
    Coins owed{};
    for (std::size_t coin = 0; coin < copper_value.size(); ++coin)
    {
        // A purse never holds more of a coin than the script was shown, so a
        // gain always fits in the counter the script itself wrote.
        if (after_script[coin] >= before_script[coin])
            settled[coin] = static_cast<std::uint16_t>(settled[coin] + after_script[coin] -
                                                       before_script[coin]);
        else
            owed[coin] = static_cast<std::uint16_t>(before_script[coin] - after_script[coin]);
    }
    CoinExchange exchange;
    std::uint64_t unpaid = 0;
    for (std::size_t coin = 0; coin < copper_value.size(); ++coin)
    {
        const auto held = std::min(owed[coin], settled[coin]);
        settled[coin] = static_cast<std::uint16_t>(settled[coin] - held);
        exchange.owed[coin] = static_cast<std::uint16_t>(owed[coin] - held);
        unpaid += exchange.owed[coin] * copper_value[coin];
    }
    if (!unpaid)
        return result;
    if (unpaid > coin_value(settled))
        throw std::runtime_error("A script took more coins than the purse holds");
    pay(settled, exchange, unpaid);
    result.exchange = exchange;
    return result;
}
} // namespace opengold
