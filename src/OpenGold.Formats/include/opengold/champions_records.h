#ifndef OPENGOLD_CHAMPIONS_RECORDS_H
#define OPENGOLD_CHAMPIONS_RECORDS_H

#include "opengold/pool_radiance.h"

#include <optional>
#include <span>

namespace opengold::champions
{
// The original 409-byte record (MONnCHA.DAX). Its order, deity, robes and
// gift slots are retained independently; raw bytes remain unchanged.
[[nodiscard]] std::optional<por::CharacterRecord>
decode_creature(std::span<const std::uint8_t> bytes);
} // namespace opengold::champions

#endif
