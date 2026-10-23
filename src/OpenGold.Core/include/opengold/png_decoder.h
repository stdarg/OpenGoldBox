#ifndef OPENGOLD_PNG_DECODER_H
#define OPENGOLD_PNG_DECODER_H

#include "opengold/formats.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace opengold::por
{
// The core has no PNG codec; the host supplies one. It returns nothing for
// bytes it cannot decode.
using PngDecoder = std::function<std::optional<Image>(std::span<const std::uint8_t>)>;
} // namespace opengold::por
#endif
