#ifndef OPENGOLD_SECRET_RECORDS_H
#define OPENGOLD_SECRET_RECORDS_H

#include <optional>
#include <span>

#include "opengold/pool_radiance.h"

// Secret of the Silver Blades' creature records (MON1CHA.DAX), 439 bytes each.
// Items (63 bytes) and effects (9 bytes) keep the earlier games' layout. The
// fields: docs/games/secret.md.
namespace opengold::secret
{

// The bits of a creature's type flags (CharacterRecord::creature_type_flags):
// what effects and spells take it for (effhndlr.pas and magic.pas in its
// reconstruction).
namespace creature_type
{
// Dispel Evil keeps it 7 worse off and sends it away.
inline constexpr std::uint8_t dispel_evil = 0x01;
// Charm Person or Mammal may charm it.
inline constexpr std::uint8_t mammal = 0x02;
// It strikes a dwarf 4 worse (a giant).
inline constexpr std::uint8_t dwarf_defends = 0x04;
// A ranger next to it adds its level to its blows.
inline constexpr std::uint8_t ranger_foe = 0x08;
// Snake Charm may take it.
inline constexpr std::uint8_t snake = 0x10;
// It strikes a gnome 4 worse.
inline constexpr std::uint8_t gnome_defends = 0x20;
// Among those Invisibility to Animals tests.
inline constexpr std::uint8_t animal = 0x40;
// A dwarf strikes it 1 better.
inline constexpr std::uint8_t dwarf_foe = 0x80;
} // namespace creature_type

// The bits of a creature's target flags (CharacterRecord::target_flags).
namespace target
{
// A giant-bane weapon strikes it for 6d6.
inline constexpr std::uint8_t giant_bane = 0x01;
// Charm Person may charm it though it is a monster.
inline constexpr std::uint8_t person = 0x02;
// A gnome strikes it 1 better.
inline constexpr std::uint8_t gnome_foe = 0x04;
// It breathes a stinking cloud unharmed, and Slay Living and Death Spell
// cannot slay it.
inline constexpr std::uint8_t unliving = 0x08;
} // namespace target

// The record in the fields every game's records share; nullopt for a record
// that is not 439 bytes or whose name is too long.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes);

} // namespace opengold::secret

#endif
