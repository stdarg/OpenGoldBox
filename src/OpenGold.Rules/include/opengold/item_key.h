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
// ("longsword+2:flame_tongue", "greatsword+1:undead", "longsword-2:cursed").
// The enchantment lives in the key so that saves, combat checkpoints and every
// rules call that takes item keys carry it without a separate channel.
enum class ItemPower : std::uint8_t
{
    none,
    flame_tongue, // SRD Flame Tongue: a Bonus Action lights it for 2d6 Fire.
    bane_undead,  // The original's "+3 vs Undead": 2 more against Undead.
    cursed        // Cannot be put down until Remove Curse (MAGIC-1).
};

struct ItemKey
{
    std::string_view base;
    int bonus{};
    ItemPower power{ItemPower::none};
};

namespace detail
{
inline constexpr std::array<std::string_view, 4> item_power_names{"", "flame_tongue", "undead",
                                                                  "cursed"};
} // namespace detail

// A key without a recognized enchantment, such as "por:unsupported:36", is all base.
[[nodiscard]] constexpr ItemKey parse_item_key(std::string_view key) noexcept
{
    ItemKey parsed{key};
    const auto colon = key.find(':');
    auto head = key.substr(0, colon);
    if (colon != std::string_view::npos)
    {
        const auto name = key.substr(colon + 1);
        for (std::size_t power = 1; power < detail::item_power_names.size(); ++power)
        {
            if (detail::item_power_names[power] == name)
            {
                parsed.power = static_cast<ItemPower>(power);
            }
        }
        if (parsed.power == ItemPower::none)
        {
            return {key};
        }
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
    else if (parsed.power == ItemPower::none)
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
