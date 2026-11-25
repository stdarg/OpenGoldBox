#include "opengold/difficulty.h"

#include <algorithm>

namespace opengold
{

std::vector<std::string> abilities_off(Difficulty difficulty,
                                       std::span<const std::pair<std::string, std::string>> options)
{
    std::vector<std::pair<std::string, std::string>> merged(ability_defaults.begin(),
                                                            ability_defaults.end());
    for (const auto &[ability, value] : options)
    {
        if (value != "never" && !parse_difficulty(value))
        {
            continue;
        }
        const auto found = std::find_if(merged.begin(), merged.end(),
                                        [&](const auto &option)
                                        {
                                            return option.first == ability;
                                        });
        if (found == merged.end())
        {
            merged.emplace_back(ability, value);
        }
        else
        {
            found->second = value;
        }
    }
    std::vector<std::string> off;
    for (const auto &[ability, value] : merged)
    {
        const auto from = parse_difficulty(value);
        if (!from || difficulty < *from)
        {
            off.push_back(ability);
        }
    }
    return off;
}

} // namespace opengold
