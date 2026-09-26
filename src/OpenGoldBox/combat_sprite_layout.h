#ifndef OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H
#define OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H

#include "opengold/rules.h"
#include <godot_cpp/variant/rect2.hpp>
#include <algorithm>
#include <numeric>
#include <span>

namespace presentation
{
// Rendering bounds only. The occupied cell remains the rules module's one cell.
inline godot::Rect2 bottom_aligned_sprite(godot::Vector2 source, godot::Rect2 visible,
                                          godot::Rect2 cell, godot::Vector2 visible_squares)
{
    if (source.x <= 0 || source.y <= 0 || visible.size.x <= 0 || visible.size.y <= 0)
        return {};
    const auto extent = cell.size * visible_squares;
    const auto scale = extent / visible.size;
    const auto top =
        cell.position + godot::Vector2((cell.size.x - extent.x) * .5, cell.size.y - extent.y);
    return {top - visible.position * scale, source * scale};
}

inline godot::Rect2 combat_sprite_rect(godot::Vector2 source, godot::Rect2 visible,
                                       godot::Rect2 cell, bool goliath)
{
    if (goliath)
        return bottom_aligned_sprite(source, visible, cell, {1, 1.25});
    if (source.x <= 0 || source.y <= 0 || visible.size.x <= 0 || visible.size.y <= 0)
        return {};
    // Transparent margins in the source image must not shrink the figure.
    const auto scale = cell.size.x * .9 / std::max(visible.size.x, visible.size.y);
    const auto size = source * scale;
    const auto visible_center = visible.get_center() * scale;
    return {cell.get_center() - visible_center, size};
}

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
