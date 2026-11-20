#include "opengold/ail_files.h"
#include "little_endian.h"

#include <algorithm>
#include <charconv>
#include <string_view>

namespace opengold::ail
{
namespace
{

using format::read_u16;
using format::read_u32;

constexpr std::string_view copyright = "Copyright (C) 1991 John Miles";

bool is_plain_text(std::span<const std::uint8_t> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(),
                       [](std::uint8_t c)
                       {
                           return c == '\t' || c == '\r' || c == '\n' || c == 0x1a ||
                                  (c >= 32 && c < 127);
                       });
}

std::string_view trimmed(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

// Reads a number and steps past it; nullopt when none is there.
std::optional<unsigned> take_number(std::string_view &text)
{
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{})
    {
        return std::nullopt;
    }
    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    return value;
}

bool take(std::string_view &text, std::string_view expected)
{
    text = trimmed(text);
    if (!text.starts_with(expected))
    {
        return false;
    }
    text.remove_prefix(expected.size());
    return true;
}

// timbre(bank,patch) = BNK_inst("name",...)
std::optional<CatalogTimbre> catalog_line(std::string_view line)
{
    if (!take(line, "timbre("))
    {
        return std::nullopt;
    }
    const auto bank = take_number(line);
    if (!bank || !take(line, ","))
    {
        return std::nullopt;
    }
    const auto patch = take_number(line);
    if (!patch || *bank > 255 || *patch > 255 || !take(line, ")") || !take(line, "=") ||
        !take(line, "BNK_inst(\""))
    {
        return std::nullopt;
    }
    const auto quote = line.find('"');
    if (quote == std::string_view::npos || quote == 0)
    {
        return std::nullopt;
    }
    return CatalogTimbre{static_cast<std::uint8_t>(*bank), static_cast<std::uint8_t>(*patch),
                         std::string(line.substr(0, quote))};
}

} // namespace

std::optional<Driver> decode_driver(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 2 + copyright.size() ||
        !std::equal(copyright.begin(), copyright.end(), bytes.begin() + 2))
    {
        return std::nullopt;
    }
    Driver driver;
    for (std::size_t at = read_u16(bytes, 0);; at += 4)
    {
        if (at + 2 > bytes.size())
        {
            return std::nullopt;
        }
        const auto function = read_u16(bytes, at);
        if (function == 0xffff)
        {
            break;
        }
        if (at + 4 > bytes.size() || read_u16(bytes, at + 2) >= bytes.size())
        {
            return std::nullopt;
        }
        driver.functions.push_back({function, read_u16(bytes, at + 2)});
    }
    if (driver.functions.empty())
    {
        return std::nullopt;
    }
    return driver;
}

std::optional<std::vector<GlobalTimbre>>
decode_global_timbre_library(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t entry_size = 6;
    constexpr std::uint8_t index_end = 0xff;
    std::vector<GlobalTimbre> timbres;
    for (std::size_t at = 0;; at += entry_size)
    {
        if (at + 2 > bytes.size())
        {
            return std::nullopt;
        }
        if (bytes[at + 1] == index_end)
        {
            return timbres;
        }
        if (at + entry_size > bytes.size())
        {
            return std::nullopt;
        }
        const std::size_t offset = read_u32(bytes, at + 2);
        if (offset + 2 > bytes.size())
        {
            return std::nullopt;
        }
        const std::size_t size = read_u16(bytes, offset);
        if (size < 2 || size > bytes.size() - offset)
        {
            return std::nullopt;
        }
        const auto timbre = bytes.subspan(offset, size);
        timbres.push_back({bytes[at], bytes[at + 1], {timbre.begin(), timbre.end()}});
    }
}

std::optional<std::vector<BankInstrument>>
decode_instrument_bank(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t header_size = 28, name_size = 12, instrument_size = 30;
    constexpr std::string_view signature = "ADLIB-";
    if (bytes.size() < header_size ||
        !std::equal(signature.begin(), signature.end(), bytes.begin() + 2))
    {
        return std::nullopt;
    }
    const std::size_t used = read_u16(bytes, 8), names = read_u16(bytes, 10);
    const std::size_t name_list = read_u32(bytes, 12), data = read_u32(bytes, 16);
    if (name_list > bytes.size() || names > (bytes.size() - name_list) / name_size ||
        data != name_list + names * name_size || bytes.size() != data + used * instrument_size)
    {
        return std::nullopt;
    }
    std::vector<BankInstrument> instruments;
    for (std::size_t entry = 0; entry < names; ++entry)
    {
        const auto at = name_list + entry * name_size;
        if (bytes[at + 2] == 0)
        {
            continue; // An unused name slot.
        }
        const auto index = read_u16(bytes, at);
        const auto text = bytes.subspan(at + 3, name_size - 3);
        const auto end = std::find(text.begin(), text.end(), std::uint8_t{0});
        if (index >= used || end == text.begin() || !is_plain_text({text.begin(), end}))
        {
            return std::nullopt;
        }
        instruments.push_back({std::string(text.begin(), end), index});
    }
    return instruments;
}

std::optional<std::vector<CatalogTimbre>> decode_timbre_catalog(std::span<const std::uint8_t> bytes)
{
    if (!is_plain_text(bytes))
    {
        return std::nullopt;
    }
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    std::vector<CatalogTimbre> timbres;
    for (std::size_t start = 0; start < text.size();)
    {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const auto line = trimmed(text.substr(start, end - start));
        start = end + 1;
        if (!line.starts_with("timbre"))
        {
            continue;
        }
        auto timbre = catalog_line(line);
        if (!timbre)
        {
            return std::nullopt;
        }
        timbres.push_back(std::move(*timbre));
    }
    return timbres;
}

} // namespace opengold::ail
