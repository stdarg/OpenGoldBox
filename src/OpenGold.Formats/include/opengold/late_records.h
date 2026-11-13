#ifndef OPENGOLD_LATE_RECORDS_H
#define OPENGOLD_LATE_RECORDS_H

#include <optional>
#include <span>
#include <vector>

#include "opengold/pool_radiance.h"

// The records Pools of Darkness and Treasures of the Savage Frontier share:
// 510-byte creatures (MON1CHA.DAX) and 17-byte items (MON1ITM.DAX, ITEM0.DAX).
// The fields: docs/games/darkness.md and docs/games/treasure.md.
namespace opengold::late
{

// The record in the fields every game's records share; nullopt for a record
// that is not 510 bytes, whose name is too long, or a game of another layout.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes, GoldBoxGame game);

// A list of 17-byte items. They are the last 17 bytes of the earlier games'
// 63-byte item, so each item's raw record is that item with no name.
[[nodiscard]] std::optional<std::vector<por::ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes);

} // namespace opengold::late

#endif
