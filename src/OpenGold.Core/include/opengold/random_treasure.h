#ifndef OPENGOLD_RANDOM_TREASURE_H
#define OPENGOLD_RANDOM_TREASURE_H
#include "opengold/pool_radiance.h"
#include <functional>
#include <vector>

namespace opengold::por
{
// Rolls one die: returns 1..sides.
using DieRoller = std::function<unsigned(unsigned sides)>;

// The Gold Box random treasure behind TREASURE item code 128 + count: the item
// type tables and generated records described in docs/QUESTS.md#random-items.
// Each record is an original 63-byte item with an empty stored name.
[[nodiscard]] std::vector<ItemRecord> random_treasure_items(unsigned count, const DieRoller &roll);
} // namespace opengold::por
#endif
