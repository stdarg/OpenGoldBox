#ifndef OPENGOLD_FORMATS_LITTLE_ENDIAN_H
#define OPENGOLD_FORMATS_LITTLE_ENDIAN_H
#include <cstddef>
#include <cstdint>
#include <span>

// Little-endian fields of the original archives, shared by every decoder
// instead of each keeping its own copy (Effective C++ Item 54). The caller has
// already checked that the bytes are there.
namespace opengold::format
{
[[nodiscard]] inline std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
        std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

[[nodiscard]] inline std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
        std::size_t offset) noexcept
{
    return read_u16(bytes, offset) |
           (static_cast<std::uint32_t>(read_u16(bytes, offset + 2)) << 16);
}
} // namespace opengold::format
#endif
