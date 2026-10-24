#include "opengold/companions.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace opengold
{
namespace
{
// A 4d6-drop-lowest roll for `score` (3 to 18): three kept dice and a 1.
rules::AbilityRoll roll_of(int score)
{
    const int high = std::min(6, score - 2);
    const int middle = std::min(6, score - high - 1);
    return {{high, middle, score - high - middle, 1}, 3};
}

// Each training group's first offered choices, refreshed after each group so
// a later group sees the choices before it, as the character pool chooses.
void choose_training(const rules::CharacterRules &creation, rules::CharacterDraft &draft)
{
    for (const auto &requested : creation.training_options(draft))
    {
        const auto current = creation.training_options(draft);
        const auto group = std::find_if(current.begin(), current.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == requested.id;
                                        });
        if (group == current.end() || group->options.size() < group->count)
        {
            throw std::runtime_error("Incomplete companion training options");
        }
        for (unsigned n = 0; n < group->count; ++n)
        {
            draft.training[group->id].push_back(group->options[n].id);
        }
    }
}

// Skullcrusher (MON4CHA 27): Strength 18, Dexterity 16, Constitution 18,
// Intelligence 10, Wisdom 8 and Charisma 12 at level 4, as his record has
// them. A Soldier's +2 Strength and +1 Constitution and the level-4 Ability
// Score Improvement's +2 Strength reach them from the rolls.
Character skullcrusher(const rules::CharacterRules &creation, const rules::RulesModule &rules)
{
    rules::CharacterDraft draft;
    draft.name = "Skullcrusher";
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "fighter";
    draft.alignment = "neutral";
    draft.background = "soldier";
    draft.rolled = true;
    constexpr std::array<int, 6> rolled{14, 16, 17, 10, 8, 12};
    for (unsigned ability = 0; ability < rolled.size(); ++ability)
    {
        draft.rolls[ability] = roll_of(rolled[ability]);
    }
    const auto adjustments = creation.adjustments(draft.background);
    const auto soldier =
        std::find_if(adjustments.begin(), adjustments.end(),
                     [](const auto &a)
                     {
                         return a.bonuses[rules::Ability::strength] == 2 &&
                                a.bonuses[rules::Ability::constitution] == 1;
                     });
    if (soldier == adjustments.end())
    {
        throw std::runtime_error("No Soldier adjustment for Skullcrusher");
    }
    draft.adjustment = static_cast<unsigned>(soldier - adjustments.begin());
    choose_training(creation, draft);
    // Levels 2 to 4, each with the rules' default choices for the level it
    // reaches.
    std::vector<rules::AdvancementChoice> history;
    Character companion(creation, draft, {});
    while (companion.sheet().level < 4)
    {
        history.push_back(rules.default_advancement(companion.sheet()));
        companion = Character(creation, draft, {});
        companion.replay(rules, history, {}, {}, {});
    }
    return companion;
}
} // namespace

std::optional<Character> original_companion(unsigned bank, unsigned record,
                                            const rules::CharacterRules &creation,
                                            const rules::RulesModule &rules)
{
    if (bank == 4 && record == 27)
    {
        return skullcrusher(creation, rules);
    }
    return std::nullopt;
}
} // namespace opengold
