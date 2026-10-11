#ifndef OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H
#define OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H

#include "opengold/rules.h"
#include <algorithm>
#include <numeric>
#include <span>

namespace presentation
{
// Godot Y grows downward: draw lower rows first, then upper rows on top.
// Sort indices so initiative order and rules snapshots remain untouched.
inline std::vector<std::size_t>
combat_sprite_draw_order(std::span<const opengold::rules::CombatantView> actors)
{
    std::vector<std::size_t> order(actors.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](auto left, auto right)
    {
        const auto &a = actors[left];
        const auto &b = actors[right];
        if (a.cell.y != b.cell.y)
            return a.cell.y > b.cell.y;
        if (a.cell.x != b.cell.x)
            return a.cell.x < b.cell.x;
        return a.id < b.id;
    });
    return order;
}
} // namespace presentation
#endif
