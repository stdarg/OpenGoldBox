#include "ua_detail.h"

#include <algorithm>

namespace opengold::ua
{

namespace detail
{

bool unpack_bits(Bytes packed, std::size_t &position, std::size_t count,
                 std::vector<std::uint8_t> &out)
{
    const auto target = out.size() + count;
    while (out.size() < target)
    {
        if (position >= packed.size())
        {
            return false;
        }
        const auto code = static_cast<std::int8_t>(packed[position++]);
        if (code == -128)
        {
            continue;
        }
        if (code >= 0)
        {
            const std::size_t length = static_cast<std::size_t>(code) + 1;
            if (position + length > packed.size() || out.size() + length > target)
            {
                return false;
            }
            out.insert(out.end(), packed.begin() + static_cast<std::ptrdiff_t>(position),
                       packed.begin() + static_cast<std::ptrdiff_t>(position + length));
            position += length;
        }
        else
        {
            const std::size_t length = static_cast<std::size_t>(1 - code);
            if (position >= packed.size() || out.size() + length > target)
            {
                return false;
            }
            out.insert(out.end(), length, packed[position++]);
        }
    }
    return true;
}

std::optional<std::vector<Chunk>> read_chunks(Bytes bytes, std::size_t offset)
{
    std::vector<Chunk> chunks;
    while (offset < bytes.size())
    {
        if (offset + 8 > bytes.size())
        {
            return std::nullopt;
        }
        const std::size_t size = read_big_u32(bytes, offset + 4);
        if (size > bytes.size() - offset - 8)
        {
            return std::nullopt;
        }
        chunks.push_back({std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4)),
                          bytes.subspan(offset + 8, size)});
        offset += 8 + size + (size & 1);
    }
    return chunks;
}

const Chunk *find_chunk(const std::vector<Chunk> &chunks, std::string_view tag)
{
    const auto found = std::find_if(chunks.begin(), chunks.end(),
                                    [tag](const Chunk &chunk)
                                    {
                                        return chunk.tag == tag;
                                    });
    return found == chunks.end() ? nullptr : &*found;
}

} // namespace detail

} // namespace opengold::ua
