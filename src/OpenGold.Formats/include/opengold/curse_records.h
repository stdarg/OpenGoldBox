#ifndef OPENGOLD_CURSE_RECORDS_H
#define OPENGOLD_CURSE_RECORDS_H

#include <optional>
#include <span>

#include "opengold/pool_radiance.h"

// Curse of the Azure Bonds' creature records (MONnCHA.DAX), the 422-byte layout
// that replaced Pool of Radiance's 285 bytes. Items (63 bytes) and effects
// (9 bytes) keep Pool of Radiance's layout. The fields: docs/games/curse.md.
namespace opengold::curse
{

// The record in the fields every game's records share; nullopt for a record
// that is not 422 bytes or whose name is too long.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes);

} // namespace opengold::curse

#endif
