#ifndef OPENGOLD_SCROLL_KEY_H
#define OPENGOLD_SCROLL_KEY_H

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::rules
{
// A spell scroll's key (MAGIC-6) names its kind of magic and the spells still
// on it, in the original's order: "scroll:arcane:fireball,haste,hold_person"
// or "scroll:divine:greater_restoration,greater_restoration". A spell cast
// from a higher slot than its own carries the level: "invisibility@3". The
// campaign adds ":unidentified" while the scroll's name is hidden; such a
// scroll cannot be read. A parsed key's spell ids view the key's characters,
// so the key must outlive it.
struct ScrollSpell
{
    std::string_view id;
    unsigned level{}; // 0: the spell's own level.
};

struct ScrollKey
{
    bool arcane{};
    std::vector<ScrollSpell> spells;
    bool unidentified{};
};

inline constexpr std::string_view scroll_prefix = "scroll:";

[[nodiscard]] inline std::optional<ScrollKey> parse_scroll_key(std::string_view key)
{
    if (!key.starts_with(scroll_prefix))
    {
        return std::nullopt;
    }
    key.remove_prefix(scroll_prefix.size());
    ScrollKey scroll;
    constexpr std::string_view unidentified = ":unidentified";
    if (key.ends_with(unidentified))
    {
        scroll.unidentified = true;
        key.remove_suffix(unidentified.size());
    }
    if (key.starts_with("arcane:"))
    {
        scroll.arcane = true;
        key.remove_prefix(7);
    }
    else if (key.starts_with("divine:"))
    {
        key.remove_prefix(7);
    }
    else
    {
        return std::nullopt;
    }
    while (!key.empty())
    {
        const auto comma = key.find(',');
        auto entry = key.substr(0, comma);
        ScrollSpell spell{entry};
        if (const auto at = entry.find('@'); at != std::string_view::npos)
        {
            const auto level = entry.substr(at + 1);
            if (level.size() != 1 || level[0] < '2' || level[0] > '9')
            {
                return std::nullopt;
            }
            spell = {entry.substr(0, at), static_cast<unsigned>(level[0] - '0')};
        }
        if (spell.id.empty())
        {
            return std::nullopt;
        }
        scroll.spells.push_back(spell);
        key.remove_prefix(comma == std::string_view::npos ? key.size() : comma + 1);
    }
    if (scroll.spells.empty() || scroll.spells.size() > 3)
    {
        return std::nullopt;
    }
    return scroll;
}

// The key for a scroll of `entries` ("fireball", "invisibility@3").
[[nodiscard]] inline std::string scroll_key(bool arcane, std::span<const std::string> entries)
{
    std::string key = std::string(scroll_prefix) + (arcane ? "arcane:" : "divine:");
    for (std::size_t n = 0; n < entries.size(); ++n)
    {
        key += (n ? "," : "") + entries[n];
    }
    return key;
}

// Magic Ink (MAGIC-6) writes or copies a spell of its level or lower. Level 1
// keeps the key saves already hold, "magic_ink"; level 3 is "magic_ink:3".
inline constexpr std::string_view magic_ink = "magic_ink";

[[nodiscard]] inline std::optional<unsigned> magic_ink_level(std::string_view key)
{
    if (key == magic_ink)
    {
        return 1;
    }
    if (key.size() != magic_ink.size() + 2 || !key.starts_with(magic_ink) ||
        key[magic_ink.size()] != ':' || key.back() < '2' || key.back() > '9')
    {
        return std::nullopt;
    }
    return static_cast<unsigned>(key.back() - '0');
}

[[nodiscard]] inline std::string magic_ink_key(unsigned level)
{
    return level == 1 ? std::string(magic_ink)
                      : std::string(magic_ink) + ":" + std::to_string(level);
}
} // namespace opengold::rules
#endif
