#ifndef OPENGOLD_CORE_H
#define OPENGOLD_CORE_H

#include <cstdint>
#include <span>

namespace opengold
{

class Core
{
  public:
    [[nodiscard]] std::uint32_t checksum(std::span<const std::uint8_t> bytes) const;
};

} // namespace opengold

#endif