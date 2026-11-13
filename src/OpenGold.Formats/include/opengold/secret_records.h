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

// The record in the fields every game's records share; nullopt for a record
// that is not 439 bytes or whose name is too long.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes);

} // namespace opengold::secret

#endif
