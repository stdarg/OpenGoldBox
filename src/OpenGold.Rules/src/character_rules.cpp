#include "opengold/character_rules.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace opengold::rules
{
bool class_eligible(const CharacterRules &rules, const CharacterDraft &d, std::string_view id)
{
    const auto r = rules.class_requirements(id);
    const auto meets = [&](unsigned ability)
    {
        return rules.ability_score(d, ability).value_or(0) >= r.minimum;
    };
    return r.any ? std::any_of(r.abilities.begin(), r.abilities.end(), meets)
           : std::all_of(r.abilities.begin(), r.abilities.end(), meets);
}

std::array<bool, 6> unmet_targets(const CharacterRules &rules, const CharacterDraft &d)
{
    std::array<bool, 6> result{};
    for (const auto &id : d.target_classes)
    {
        if (class_eligible(rules, d, id))
            continue;
        const auto r = rules.class_requirements(id);
        for (auto ability : r.abilities)
            if (rules.ability_score(d, ability).value_or(0) < r.minimum)
                result.at(ability) = true;
    }
    return result;
}

int AbilityRoll::total() const
{
    if (discarded >= dice.size() ||
            std::any_of(dice.begin(), dice.end(),
                        [](int n)
{
    return n < 1 || n > 6;
}) ||
dice[discarded] != *std::min_element(dice.begin(), dice.end()))
    throw std::runtime_error("Invalid 4d6 roll");
    return std::accumulate(dice.begin(), dice.end(), 0) - dice[discarded];
}
} // namespace opengold::rules
