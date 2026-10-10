#ifndef OPENGOLD_COIN_PURSE_H
#define OPENGOLD_COIN_PURSE_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace opengold
{
// Original purse order: copper, silver, electrum, gold, platinum, gems, jewelry.
using Purse = std::array<std::uint16_t, 7>;
// Coin counts in the purse's first five denominations, copper through platinum.
using Coins = std::array<std::uint16_t, 5>;

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

[[nodiscard]] constexpr std::uint16_t &coins(Purse &purse, Coin coin) noexcept
{
    return purse[static_cast<std::size_t>(coin)];
}

[[nodiscard]] constexpr std::uint16_t coins(const Purse &purse, Coin coin) noexcept
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

// Original New Phlan scripts check and take one particular denomination (the
// inn asks for platinum). They see, in each denomination, what the whole coin
// purse can afford at SRD rates. Gems and jewelry are not coins; they are shown
// as held.
[[nodiscard]] Purse script_coins(const Purse &purse);

// Applies once what a script changed relative to script_coins(purse). Added
// coins are added as given. Taken coins come from that denomination first; the
// rest is paid from other coins with change. Throws if the script took more
// than the purse is worth.
[[nodiscard]] SettledPurse settle_script_coins(const Purse &purse, const Purse &after_script);
} // namespace opengold
#endif
