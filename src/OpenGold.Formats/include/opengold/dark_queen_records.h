#ifndef OPENGOLD_DARK_QUEEN_RECORDS_H
#define OPENGOLD_DARK_QUEEN_RECORDS_H

#include "opengold/geo_map.h"
#include "opengold/pool_radiance.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace opengold::dark_queen
{

// Native numeric correspondence used by the Death Knights save importer.
[[nodiscard]] int import_spell_number(int number) noexcept;
[[nodiscard]] int import_effect_number(int number) noexcept;
[[nodiscard]] int import_item_word(int number) noexcept;

struct SpellBlock
{
    std::array<std::uint8_t, 137> memorized{};
    std::array<std::uint8_t, 16> known_bits{};
    std::array<std::array<std::uint8_t, 9>, 4> slots{};
};

struct Creature
{
    por::CharacterRecord character;
    SpellBlock spells;
    std::vector<por::EffectRecord> effects;
    std::vector<por::ItemRecord> items;
    // Expanded character, counts, effects/items and any unused native tail.
    std::vector<std::uint8_t> raw;
};

// One expanded 414-byte character, including its native spell block.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_character(std::span<const std::uint8_t> bytes);

// MONCHA.GLB: big-endian expanded length, then PackBits. The 414-byte
// character is followed by two counts, five-byte effects and 17-byte items.
// Original pointer bytes are retained, never followed.
[[nodiscard]] std::optional<Creature> decode_creature(std::span<const std::uint8_t> bytes);

// ITEM.DAT and the item tails carried in creature records share this layout.
[[nodiscard]] std::optional<std::vector<por::ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes);

struct Area
{
    unsigned rows{}, columns{};
    std::array<std::uint8_t, 3> wall_sets{}, backdrops{};
    // Only even flag bits are passages; odd bits also select the backdrop.
    std::vector<por::MapCell> cells;
    std::vector<std::uint8_t> raw;
};

// GEO.GLB: eight header bytes and four planes of rows * columns cells.
// The native area allocation permits at most 784 squares.
[[nodiscard]] std::optional<Area> decode_area(std::span<const std::uint8_t> bytes);

} // namespace opengold::dark_queen

#endif
