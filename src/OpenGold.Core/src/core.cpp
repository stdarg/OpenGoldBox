#include "opengold/core.h"

#include "opengold/formats.h"

namespace opengold
{

std::uint32_t Core::checksum(std::span<const std::uint8_t> bytes) const
{
    return formats_checksum(bytes);
}

} // namespace opengold
