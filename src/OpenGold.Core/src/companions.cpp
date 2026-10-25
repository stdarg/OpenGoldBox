#include "opengold/companions.h"
#include <algorithm>
#include <array>
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

// The background's adjustment of +2 to `raised` and +1 to `lifted`, by its index.
unsigned adjustment(const rules::CharacterRules &creation, const std::string &background,
                    rules::Ability raised, rules::Ability lifted)
{
    const auto adjustments = creation.adjustments(background);
    const auto found = std::find_if(adjustments.begin(), adjustments.end(),
                                    [&](const auto &a)
                                    {
                                        return a.bonuses[raised] == 2 && a.bonuses[lifted] == 1;
                                    });
    if (found == adjustments.end())
    {
        throw std::runtime_error("No such adjustment for a companion's background");
    }
    return static_cast<unsigned>(found - adjustments.begin());
}

// The Soldier background's +2 Strength and +1 Constitution, by its index.
unsigned soldier_adjustment(const rules::CharacterRules &creation, const std::string &background)
{
    return adjustment(creation, background, rules::Ability::strength, rules::Ability::constitution);
}

// `draft` at `level`, each level after the first with the rules' default
// choices for the level it reaches.
Character advanced(const rules::CharacterRules &creation, const rules::RulesModule &rules,
                   const rules::CharacterDraft &draft, int level)
{
    std::vector<rules::AdvancementChoice> history;
    Character companion(creation, draft, {});
    while (companion.sheet().level < level)
    {
        history.push_back(rules.default_advancement(companion.sheet()));
        companion = Character(creation, draft, {});
        companion.replay(rules, history, {}, {}, {});
    }
    return companion;
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
    draft.adjustment = soldier_adjustment(creation, draft.background);
    choose_training(creation, draft);
    return advanced(creation, rules, draft, 4);
}

// The mad fighter of Mendor's Library (MON2CHA 25): Strength 17 and 10 in the
// rest at level 1, as his record has them; a Soldier's +2 Strength and +1
// Constitution reach them from the rolls.
Character mad_fighter(const rules::CharacterRules &creation)
{
    rules::CharacterDraft draft;
    draft.name = "Mad Man";
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "fighter";
    draft.alignment = "neutral";
    draft.background = "soldier";
    draft.rolled = true;
    constexpr std::array<int, 6> rolled{15, 10, 9, 10, 10, 10};
    for (unsigned ability = 0; ability < rolled.size(); ++ability)
    {
        draft.rolls[ability] = roll_of(rolled[ability]);
    }
    draft.adjustment = soldier_adjustment(creation, draft.background);
    choose_training(creation, draft);
    return Character(creation, draft, {});
}

// Dirten, the priest the bishop sends to the Temple of Bane (MON3CHA 107): a
// level-5 Cleric with Strength 10, Dexterity 10, Constitution 9, Intelligence
// 15, Wisdom 16 and Charisma 15, as his record has them. An Acolyte's +2
// Wisdom and +1 Intelligence and the level-4 Ability Score Improvement's +2
// Wisdom reach them from the rolls. He brings his mace, and for his chain
// armor the Chain Shirt, the chain a Cleric is trained to wear (BANE-1).
Character dirten(const rules::CharacterRules &creation, const rules::RulesModule &rules)
{
    rules::CharacterDraft draft;
    draft.name = "Dirten";
    draft.race = "human";
    draft.gender = "male";
    draft.character_class = "cleric";
    draft.alignment = "lawful_good";
    draft.background = "acolyte";
    draft.rolled = true;
    constexpr std::array<int, 6> rolled{10, 10, 9, 14, 12, 15};
    for (unsigned ability = 0; ability < rolled.size(); ++ability)
    {
        draft.rolls[ability] = roll_of(rolled[ability]);
    }
    draft.adjustment = adjustment(creation, draft.background, rules::Ability::wisdom,
                                  rules::Ability::intelligence);
    choose_training(creation, draft);
    auto companion = advanced(creation, rules, draft, 5);
    constexpr int original_mace = 23;
    companion.add_item({"mace", "Mace", 1, original_mace});
    companion.add_item({"chain_shirt", "Chain Shirt"});
    return companion;
}

// Princess Fatima of the kobold caves (MON8CHA 104): a level-4 Fighter with
// Strength 18, Dexterity 18, Constitution 16, Intelligence 14, Wisdom 10 and
// Charisma 15, as her record has them; a Soldier's +2 Strength and +1
// Constitution and the level-4 Ability Score Improvement's +2 Strength reach
// them from the rolls. She brings her short sword, leather armor and shield.
Character fatima(const rules::CharacterRules &creation, const rules::RulesModule &rules)
{
    rules::CharacterDraft draft;
    draft.name = "Princess Fatima";
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "fighter";
    draft.alignment = "lawful_good";
    draft.background = "soldier";
    draft.rolled = true;
    constexpr std::array<int, 6> rolled{14, 18, 15, 14, 10, 15};
    for (unsigned ability = 0; ability < rolled.size(); ++ability)
    {
        draft.rolls[ability] = roll_of(rolled[ability]);
    }
    draft.adjustment = soldier_adjustment(creation, draft.background);
    choose_training(creation, draft);
    auto companion = advanced(creation, rules, draft, 4);
    constexpr int original_short_sword = 37, original_leather = 50, original_shield = 59;
    companion.add_item({"shortsword", "Shortsword", 1, original_short_sword});
    companion.add_item({"leather", "Leather Armor", 1, original_leather});
    companion.add_item({"shield", "Shield", 1, original_shield});
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
    if (bank == 2 && record == 25)
    {
        return mad_fighter(creation);
    }
    if (bank == 3 && record == 107)
    {
        return dirten(creation, rules);
    }
    if (bank == 8 && record == 104)
    {
        return fatima(creation, rules);
    }
    return std::nullopt;
}
} // namespace opengold
