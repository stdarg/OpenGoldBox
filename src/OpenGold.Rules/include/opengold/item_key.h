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
// rules call that takes item keys carry it without a separate channel. The
// campaign adds ":attuned" when it hands the rules an item whose wielder is
// attuned to it; inventories store keys without it.
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
    bool attuned{};
};

// SRD Flame Tongue needs attunement; the original's other powers do not.
[[nodiscard]] constexpr bool needs_attunement(ItemPower power) noexcept
{
    return power == ItemPower::flame_tongue;
}

namespace detail
{
inline constexpr std::array<std::string_view, 4> item_power_names{"", "flame_tongue", "undead",
                                                                  "cursed"};

constexpr ItemPower item_power_named(std::string_view name) noexcept
{
    for (std::size_t power = 1; power < item_power_names.size(); ++power)
    {
        if (item_power_names[power] == name)
        {
            return static_cast<ItemPower>(power);
        }
    }
    return ItemPower::none;
}
} // namespace detail

// A key without a recognized enchantment, such as "por:unsupported:36", is all base.
[[nodiscard]] constexpr ItemKey parse_item_key(std::string_view key) noexcept
{
    ItemKey parsed{key};
    auto head = key.substr(0, key.find(':'));
    if (auto rest = key.substr(head.size()); !rest.empty())
    {
        // ":power", then ":attuned" when the campaign adds it.
        rest.remove_prefix(1);
        const auto name = rest.substr(0, rest.find(':'));
        parsed.power = detail::item_power_named(name);
        rest.remove_prefix(name.size());
        parsed.attuned = rest == ":attuned";
        if (parsed.power == ItemPower::none || (!rest.empty() && !parsed.attuned))
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
                                          ItemPower power = ItemPower::none, bool attuned = false)
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
        if (attuned)
        {
            key += ":attuned";
        }
    }

    return key;
}
} // namespace opengold::rules
#endif
