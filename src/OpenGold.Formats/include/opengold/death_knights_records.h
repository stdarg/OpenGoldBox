#ifndef OPENGOLD_DEATH_KNIGHTS_RECORDS_H
#define OPENGOLD_DEATH_KNIGHTS_RECORDS_H

#include "opengold/pool_radiance.h"

#include <array>
#include <optional>
#include <span>

namespace opengold::death_knights
{
struct SpellBlock
{
    std::array<std::uint8_t, 74> memorized{};
    std::array<std::uint8_t, 114> known{};
    std::array<std::array<std::uint8_t, 8>, 4> slots{};
};

// MON1CHA's 216-byte character and MON1WIZ's separate 220-byte spells.
// Pointers in the original records are retained as bytes, never dereferenced.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::optional<SpellBlock> decode_spells(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool attach_spells(por::CharacterRecord &record, std::span<const std::uint8_t> bytes);
// ITEM0 contains numbered 17-byte carried-item tails, starting at template 1.
[[nodiscard]] std::optional<std::vector<por::ItemRecord>>
decode_item_list(std::span<const std::uint8_t> bytes);
// The eight shops, read from the unpacked START.EXE numeric stock tables.
[[nodiscard]] std::optional<std::vector<std::vector<std::uint8_t>>>
decode_stores(std::span<const std::uint8_t> program);
} // namespace opengold::death_knights

#endif
