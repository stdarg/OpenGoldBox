#ifndef OPENGOLD_COIN_PURSE_H
#define OPENGOLD_COIN_PURSE_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace opengold
{
// Original purse order: copper, silver, electrum, gold, platinum, gems, jewelry.
// The party's one purse holds what six original purses could, and more, so its
// counts are wider than the scripts' 16-bit coin variables.
using Purse = std::array<std::uint32_t, 7>;
// Coin counts in the purse's first five denominations, copper through platinum.
using Coins = std::array<std::uint32_t, 5>;
constexpr std::uint32_t purse_limit = std::numeric_limits<std::uint32_t>::max();
// The most of one denomination a script's coin variable can show.
constexpr std::uint32_t script_coin_limit = 65535;

// A purse entry by name rather than by position: wealth[3] meant gold
// (Effective C++ Item 18).
enum class Coin : std::size_t
{
    copper,
    silver,
    electrum,
    gold,
    platinum,
    gems,
    jewelry
};

[[nodiscard]] constexpr std::uint32_t &coins(Purse &purse, Coin coin) noexcept
{
    return purse[static_cast<std::size_t>(coin)];
}

[[nodiscard]] constexpr std::uint32_t coins(const Purse &purse, Coin coin) noexcept
{
    return purse[static_cast<std::size_t>(coin)];
}

// The purse made change because a script took coins the payer did not hold:
// `owed` is what the purse lacked, `paid` the coins given for it, `change` the
// coins handed back.
struct CoinExchange
{
    Coins owed{}, paid{}, change{};
};

struct SettledPurse
{
    Purse purse;
    std::optional<CoinExchange> exchange;
};

// What each coin, copper to platinum, is worth in 200ths of a gold piece:
// the SRD's rates (10 sp, 2 ep or 1/10 pp to the gold piece), or the
// original's (20 sp, 2 ep or 1/5 pp, gdconsts.pas coinValues), as the rules
// played say (RulesModule::coin_worth).
struct CoinRates
{
    std::array<std::uint64_t, 5> worth;
};

inline constexpr CoinRates srd_coin_rates{{2, 20, 100, 200, 2000}};

// What the purse's coins are worth in whole gold pieces at these rates; gems
// and jewelry are not coins.
[[nodiscard]] std::uint64_t gold_value(const Purse &purse, const CoinRates &rates = srd_coin_rates);
// gold_value, at most what a 32-bit count holds.
[[nodiscard]] std::uint32_t spendable_gold(const Purse &purse,
                                           const CoinRates &rates = srd_coin_rates);

// Pays a price of `gold` gold pieces with any coins: with gold when it
// covers the price, otherwise with the largest coins that do not overpay and
// then one larger coin, with change in gold, silver and copper. Throws if the
// coins are worth less than the price.
[[nodiscard]] SettledPurse pay_gold(const Purse &purse, std::uint64_t gold,
                                    const CoinRates &rates = srd_coin_rates);

// Changes the purse's coins up into larger ones, copper through gold, each as
// far as it makes whole coins of the next (Gateway's CALL 8810h): the
// original's ten copper a silver, ten silver an electrum, two electrum a gold
// and five gold a platinum at its rates. The purse is worth what it was.
[[nodiscard]] Purse change_coins_up(Purse purse, const CoinRates &rates = srd_coin_rates);

// Original New Phlan scripts check and take one particular denomination (the
// inn asks for platinum). They see, in each denomination, what the whole coin
// purse can afford at its rates. Gems and jewelry are not coins; they are
// shown as held. A script variable shows at most script_coin_limit.
[[nodiscard]] Purse script_coins(const Purse &purse, const CoinRates &rates = srd_coin_rates);

// Applies once what a script changed relative to script_coins(purse). Added
// coins are added as given. Taken coins come from that denomination first; the
// rest is paid from other coins with change. Throws if the script took more
// than the purse is worth.
[[nodiscard]] SettledPurse settle_script_coins(const Purse &purse, const Purse &after_script,
                                               const CoinRates &rates = srd_coin_rates);
} // namespace opengold
#endif
