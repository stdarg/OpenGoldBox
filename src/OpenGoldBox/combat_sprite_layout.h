#ifndef OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H
#define OPENGOLDBOX_COMBAT_SPRITE_LAYOUT_H

#include "opengold/rules.h"
#include <algorithm>
#include <numeric>
#include <span>

namespace presentation
{
// Godot Y grows downward: draw lower rows first, then upper rows on top. A
// creature filling more than one row stands on its lowest (FOOT-1).
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
                  const int a_row = a.cell.y + a.footprint.height - 1;
                  const int b_row = b.cell.y + b.footprint.height - 1;
                  if (a_row != b_row)
                  {
                      return a_row > b_row;
                  }
                  if (a.cell.x != b.cell.x)
                  {
                      return a.cell.x < b.cell.x;
                  }
                  return a.id < b.id;
              });
    return order;
}
} // namespace presentation
#endif
