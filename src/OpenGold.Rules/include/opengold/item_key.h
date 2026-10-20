#ifndef OPENGOLD_ITEM_KEY_H
#define OPENGOLD_ITEM_KEY_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace opengold::rules
{
// An item definition key is a catalog key ("longsword"), optionally followed by
// the original's enchantment ("longsword+2") and one special power
// ("longsword+2:flame_tongue", "greatsword+1:undead", "longsword-2:cursed",
// "javelin:lightning", "wand:magic_missiles").
// The enchantment lives in the key so that saves, combat checkpoints and every
// rules call that takes item keys carry it without a separate channel. The
// campaign adds ":unidentified" when it hands the rules an item whose name is
// still partly hidden, so the rules name it without its enchantment;
// inventories store keys without it.
enum class ItemPower : std::uint8_t
{
    none,
    flame_tongue, // SRD Flame Tongue: a Bonus Action lights it for 2d6 Fire.
    bane_undead,  // The original's "+3 vs Undead": 2 more against Undead.
    cursed,       // Cannot be put down until Remove Curse (MAGIC-1).
    lightning,    // SRD Javelin of Lightning: once a day its throw is a bolt (MAGIC-3).
    // The original's charged wands (MAGIC-3): a held wand that casts its spell.
    magic_missiles,
    lightning_bolts,
    paralysis
};

struct ItemKey
{
    std::string_view base;
    int bonus{};
    ItemPower power{ItemPower::none};
    bool unidentified{};
};

namespace detail
{
inline constexpr std::array<std::string_view, 8> item_power_names{
    "", "flame_tongue", "undead", "cursed", "lightning", "magic_missiles", "lightning_bolts",
    "paralysis"};
} // namespace detail

// A key without a recognized enchantment or flag, such as "por:unsupported:36",
// is all base. A magic item without an enchantment, such as a Ring of Fire
// Resistance, can still be ":unidentified".
[[nodiscard]] constexpr ItemKey parse_item_key(std::string_view key) noexcept
{
    ItemKey parsed{key};
    auto head = key.substr(0, key.find(':'));
    // The flags follow in this order: the power, ":unidentified".
    auto rest = key.substr(head.size());
    const auto take = [&rest](std::string_view flag)
    {
        const bool present = rest.starts_with(':') && rest.substr(1, rest.find(':', 1) - 1) == flag;
        if (present)
        {
            rest.remove_prefix(flag.size() + 1);
        }
        return present;
    };
    for (std::size_t power = 1; power < detail::item_power_names.size(); ++power)
    {
        if (take(detail::item_power_names[power]))
        {
            parsed.power = static_cast<ItemPower>(power);
            break;
        }
    }
    parsed.unidentified = take("unidentified");
    if (!rest.empty())
    {
        return {key};
    }

    const auto size = head.size();
    const bool enchanted = size > 2 && (head[size - 2] == '+' || head[size - 2] == '-') &&
                           head[size - 1] >= '1' && head[size - 1] <= '9';
    if (enchanted)
    {
        const int amount = head[size - 1] - '0';
        parsed.bonus = head[size - 2] == '+' ? amount : -amount;
        head.remove_suffix(2);
    }
    else if (parsed.power == ItemPower::none && !parsed.unidentified)
    {
        return {key};
    }

    parsed.base = head;
    return parsed;
}

[[nodiscard]] constexpr std::string_view base_item_key(std::string_view key) noexcept
{
    return parse_item_key(key).base;
}

[[nodiscard]] inline std::string item_key(std::string_view base, int bonus,
                                          ItemPower power = ItemPower::none)
{
    if (bonus < -9 || bonus > 9)
    {
        throw std::invalid_argument("An item bonus is one digit");
    }

    std::string key(base);
    if (bonus)
    {
        key += (bonus > 0 ? '+' : '-');
        key += static_cast<char>('0' + (bonus > 0 ? bonus : -bonus));
    }

    if (power != ItemPower::none)
    {
        key += ':';
        key += detail::item_power_names[static_cast<std::size_t>(power)];
    }

    return key;
}
} // namespace opengold::rules
#endif
