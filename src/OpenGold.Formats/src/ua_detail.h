#ifndef OPENGOLD_FORMATS_UA_DETAIL_H
#define OPENGOLD_FORMATS_UA_DETAIL_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What Unlimited Adventures' decoders share: big-endian fields, PackBits and
// IFF chunks. The kit's own records and the common formats it reads (ILBM,
// XMI) both use them.
namespace opengold::ua::detail
{

using Bytes = std::span<const std::uint8_t>;

[[nodiscard]] inline std::uint16_t read_big_u16(Bytes bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(bytes[offset] << 8 | bytes[offset + 1]);
}

[[nodiscard]] inline std::uint32_t read_big_u32(Bytes bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(read_big_u16(bytes, offset)) << 16 |
           read_big_u16(bytes, offset + 2);
}

[[nodiscard]] inline bool has_tag(Bytes bytes, std::size_t offset, std::string_view tag) noexcept
{
    return offset + tag.size() <= bytes.size() &&
           std::equal(tag.begin(), tag.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

// PackBits (IFF's ByteRun1), as the kit unpacks it: a code of 0 to 127 copies
// the next code + 1 bytes, -1 to -127 repeats the next byte 1 - code times,
// -128 is skipped. Appends count bytes to out and moves position past what it
// read; false when the input ends first or a run would pass count.
[[nodiscard]] bool unpack_bits(Bytes packed, std::size_t &position, std::size_t count,
                               std::vector<std::uint8_t> &out);

struct Chunk
{
    std::string tag;
    Bytes data;
};

// The chunks from offset to the end: a tag, a big-endian size and the data,
// padded to an even length. nullopt when a chunk runs past the end.
[[nodiscard]] std::optional<std::vector<Chunk>> read_chunks(Bytes bytes, std::size_t offset);

// The first chunk with the tag, or nullptr.
[[nodiscard]] const Chunk *find_chunk(const std::vector<Chunk> &chunks, std::string_view tag);

} // namespace opengold::ua::detail

#endif
