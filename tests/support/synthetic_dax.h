#ifndef OPENGOLD_TEST_SYNTHETIC_DAX_H
#define OPENGOLD_TEST_SYNTHETIC_DAX_H
#include "opengold/formats.h"
#include <algorithm>
#include <stdexcept>

namespace opengold::test
{
inline void word(std::vector<std::uint8_t> &bytes, std::size_t at, unsigned value)
{
    bytes.at(at) = value & 255;
    bytes.at(at + 1) = (value >> 8) & 255;
}

// Test-only literal encoder. Expected decoded bytes come from the caller, not
// the production decoder. All fixtures are authored; none contain game assets.
inline std::vector<std::uint8_t> literal_dax(const std::vector<DaxRecord> &records)
{
    if (records.size() > 256)
        throw std::logic_error("Too many fixture records");
    std::vector<std::uint8_t> bytes(2 + 9 * records.size());
    word(bytes, 0, static_cast<unsigned>(9 * records.size()));
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        const auto &record = records[i];
        const auto start = bytes.size();
        const auto offset = start - 2 - 9 * records.size();
        const auto header = 2 + 9 * i;
        bytes[header] = record.id;
        word(bytes, header + 1, static_cast<unsigned>(offset));
        word(bytes, header + 3, static_cast<unsigned>(offset >> 16));
        for (std::size_t at = 0; at < record.bytes.size(); at += 128)
        {
            const auto count = std::min<std::size_t>(128, record.bytes.size() - at);
            bytes.push_back(count - 1);
            bytes.insert(bytes.end(), record.bytes.begin() + at, record.bytes.begin() + at + count);
        }
        if (record.bytes.size() > 65535 || bytes.size() - start > 65535)
            throw std::logic_error("Fixture exceeds DAX record limits");
        word(bytes, header + 5, static_cast<unsigned>(record.bytes.size()));
        word(bytes, header + 7, static_cast<unsigned>(bytes.size() - start));
    }
    return bytes;
}
} // namespace opengold::test
#endif
