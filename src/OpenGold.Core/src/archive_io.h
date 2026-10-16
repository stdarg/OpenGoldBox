#ifndef OPENGOLD_ARCHIVE_IO_H
#define OPENGOLD_ARCHIVE_IO_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opengold::por::detail
{
// DAX has at most 256 records of at most 65535 bytes each. Bound allocations.
inline constexpr std::uintmax_t archive_size_limit = 32 * 1024 * 1024;

inline std::string upper_ascii(std::string_view text)
{
    std::string value(text);
    for (auto &c : value)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return value;
}

template <class Error>
std::vector<std::uint8_t> read_archive_bytes(const std::filesystem::path &path,
                                             std::string too_large, std::string cannot_open,
                                             std::string cannot_read)
{
    const auto size = std::filesystem::file_size(path);
    if (size > archive_size_limit)
    {
        throw Error(std::move(too_large));
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw Error(std::move(cannot_open));
    }
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
    if (input.bad() || bytes.size() != size)
    {
        throw Error(std::move(cannot_read));
    }
    return bytes;
}

template <class Error>
std::filesystem::path find_archive(const std::filesystem::path &directory, std::string_view name,
                                   std::string ambiguous, std::string missing)
{
    std::optional<std::filesystem::path> match;
    for (const auto &file : std::filesystem::directory_iterator(directory))
    {
        if (upper_ascii(file.path().filename().string()) != name || !file.is_regular_file())
        {
            continue;
        }
        if (match)
        {
            throw Error(std::move(ambiguous));
        }
        match = file.path();
    }
    if (!match)
    {
        throw Error(std::move(missing));
    }
    return *match;
}
} // namespace opengold::por::detail
#endif
