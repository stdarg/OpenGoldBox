#include "opengold/treasures_texts.h"

#include <algorithm>

namespace opengold::treasures
{
namespace
{

constexpr std::uint8_t end_of_run = 0xfe;
constexpr std::uint8_t end_of_file = 0x1a;

bool printable(std::uint8_t letter)
{
    return letter >= 32 && letter <= 126;
}

bool luck_control(std::uint8_t code)
{
    const bool romance_name = code >= 2 && code <= 8;
    return romance_name || code == 10 || code == 12 || code == 13;
}

} // namespace

std::optional<std::vector<std::vector<std::uint8_t>>>
decode_border_runs(std::span<const std::uint8_t> record)
{
    if (record.empty() || record.back() != end_of_run)
    {
        return std::nullopt;
    }
    std::vector<std::vector<std::uint8_t>> runs(1);
    for (const auto tile : record.first(record.size() - 1))
    {
        if (tile == end_of_run)
        {
            runs.emplace_back();
        }
        else
        {
            runs.back().push_back(tile);
        }
    }
    return runs;
}

std::optional<std::vector<std::string>> decode_name_table(std::span<const std::uint8_t> record,
                                                          std::size_t capacity)
{
    const std::size_t entry_size = capacity + 1;
    if (capacity == 0 || record.empty() || record.size() % entry_size != 0)
    {
        return std::nullopt;
    }
    std::vector<std::string> names;
    for (std::size_t entry = 0; entry < record.size(); entry += entry_size)
    {
        const std::size_t length = record[entry];
        const auto letters = record.subspan(entry + 1, std::min(length, capacity));
        if (length > capacity || !std::all_of(letters.begin(), letters.end(), printable))
        {
            return std::nullopt;
        }
        names.emplace_back(letters.begin(), letters.end());
    }
    return names;
}

std::optional<std::string> decode_luck_text(std::span<const std::uint8_t> record)
{
    if (record.empty() || record.back() != end_of_file)
    {
        return std::nullopt;
    }
    const auto text = record.first(record.size() - 1);
    if (!std::all_of(text.begin(), text.end(),
                     [](std::uint8_t letter)
                     {
                         return printable(letter) || luck_control(letter);
                     }))
    {
        return std::nullopt;
    }
    return std::string(text.begin(), text.end());
}

} // namespace opengold::treasures
