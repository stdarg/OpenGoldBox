#include "opengold/character_rules.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace opengold::rules
{
AbilityCheckModifier CharacterRules::ability_check(const CharacterSheet &, unsigned,
        std::string_view) const
{
    throw std::runtime_error("Ability checks are unsupported by this rules module");
}

bool CharacterRules::class_eligible(const CharacterDraft &d, std::string_view id) const
{
    const auto r = class_requirements(id);
    const auto meets = [&](unsigned ability)
    {
        return ability_score(d, ability).value_or(0) >= r.minimum;
    };
    return r.any ? std::any_of(r.abilities.begin(), r.abilities.end(), meets)
           : std::all_of(r.abilities.begin(), r.abilities.end(), meets);
}

std::array<bool, 6> CharacterRules::unmet_targets(const CharacterDraft &d) const
{
    std::array<bool, 6> result{};
    for (const auto &id : d.target_classes)
    {
        if (class_eligible(d, id))
            continue;
        const auto r = class_requirements(id);
        for (auto ability : r.abilities)
            if (ability_score(d, ability).value_or(0) < r.minimum)
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
