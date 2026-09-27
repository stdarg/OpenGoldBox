#include "opengold/character.h"
#include <algorithm>
#include <stdexcept>

namespace opengold
{
Character::Character(const rules::CharacterRules &rules, rules::CharacterDraft creation,
                     por::CharacterAppearance appearance)
    : creation_(std::move(creation)), sheet_(rules.evaluate(creation_, true))
{
    this->appearance(appearance);
}

void Character::appearance(por::CharacterAppearance value)
{
    por::validate_character_appearance(value);
    appearance_ = value;
}

bool Character::advance(const rules::RulesModule &rules, rules::VitalState &state)
{
    return advance(rules, state, rules.default_advancement(sheet_));
}

bool Character::advance(const rules::RulesModule &rules, rules::VitalState &state,
                        const rules::AdvancementChoice &choice)
{
    auto sheet = sheet_;
    auto vitals = state;
    auto history = advancements_;
    if (!rules.advance_character(sheet, vitals, choice))
        return false;
    history.push_back(choice);
    sheet_ = std::move(sheet);
    state = std::move(vitals);
    advancements_ = std::move(history);
    return true;
}

void Character::choose_spells(const rules::RulesModule &rules, const rules::SpellChoices &choices,
                              std::uint64_t rest_session, bool complete)
{
    if (rest_session && std::any_of(spell_edits_.begin(), spell_edits_.end(),
                                    [&](const auto &edit)
                                    {
                                        return edit.rest_session >= rest_session;
                                    }))
        throw std::runtime_error("Spell choices already used for this rest");
    auto candidate = sheet_;
    auto history = spell_edits_;
    rules.apply_spell_choices(candidate, choices,
                              rest_session ? rules::SpellChoiceContext::long_rest
                                           : rules::SpellChoiceContext::pending,
                              complete);
    history.push_back({unsigned(sheet_.level), rest_session, choices});
    (void)rules.character_profile(candidate, {});
    sheet_ = std::move(candidate);
    spell_edits_ = std::move(history);
}

void Character::replace_rest_training(const rules::RulesModule &rules,
                                      std::span<const std::string> selections,
                                      std::uint64_t session)
{
    if (!session || std::any_of(training_edits_.begin(), training_edits_.end(),
                                [&](const auto &edit)
                                {
                                    return edit.rest_session >= session;
                                }))
        throw std::runtime_error("Training choices already used for this rest");
    auto candidate = sheet_;
    auto history = training_edits_;
    auto choices = rules.replace_rest_training(candidate, selections);
    history.push_back({unsigned(sheet_.level), session, {selections.begin(), selections.end()}});
    (void)rules.character_profile(candidate, {});
    sheet_ = std::move(candidate);
    training_edits_ = std::move(history);
    replaced_training_ = std::move(choices);
}

rules::TrainingChoices Character::training_choices() const
{
    auto result = creation_.training;
    for (const auto &advancement : advancements_)
        for (const auto &[id, values] : advancement.training)
            result.emplace(id, values);
    for (const auto &[id, values] : replaced_training_)
        result[id] = values;
    return result;
}

Character Character::preview_training(const rules::CharacterRules &creation_rules,
                                      const rules::RulesModule &rules,
                                      const rules::TrainingChoices &choices,
                                      bool require_complete) const
{
    if (creation_rules.identity() != sheet_.identity)
        throw std::runtime_error("Character creation rules do not match");
    if (require_complete && sheet_.training.complete)
        throw std::runtime_error("This character has no pending training choices");
    for (const auto &[group, selected] : training_choices())
    {
        const auto found = choices.find(group);
        if (found == choices.end() ||
            std::any_of(selected.begin(), selected.end(),
                        [&](const auto &value)
                        {
                            return std::find(found->second.begin(), found->second.end(), value) ==
                                   found->second.end();
                        }))
            throw std::runtime_error("Previously selected training cannot be replaced");
    }
    auto draft = creation_;
    draft.training = choices;
    // Replay edits from their original acquisition choices, while adding only
    // the genuinely pending groups. Effective replacement choices remain locked.
    for (const auto &[id, values] : replaced_training_)
    {
        draft.training.erase(id);
        if (const auto found = creation_.training.find(id); found != creation_.training.end())
            draft.training.emplace(*found);
    }
    const auto later = rules.training_options(sheet_);
    for (const auto &group : later)
        draft.training.erase(group.id);
    Character candidate(creation_rules, std::move(draft), appearance_);
    // Replay on an isolated healthy state, as during save reconstruction. The
    // campaign keeps the real wounds, effects and spent resources unchanged.
    rules::VitalState replay{candidate.sheet_.hit_points, false, {}};
    auto replay_spells = [&]
    {
        for (const auto &edit : spell_edits_)
            if (edit.level == unsigned(candidate.sheet_.level))
                candidate.choose_spells(rules, edit.choices, edit.rest_session, false);
        for (const auto &edit : training_edits_)
            if (edit.level == unsigned(candidate.sheet_.level))
                candidate.replace_rest_training(rules, edit.selections, edit.rest_session);
    };
    replay_spells();
    for (auto choice : advancements_)
    {
        for (const auto &group : later)
            if (group.acquired_level == candidate.sheet_.level + 1 &&
                !replaced_training_.contains(group.id))
            {
                choice.training.erase(group.id);
                if (const auto found = choices.find(group.id); found != choices.end())
                    choice.training.emplace(*found);
            }
        if (!candidate.advance(rules, replay, choice))
            throw std::runtime_error("Cannot reconstruct character advancement");
        replay_spells();
    }
    if (require_complete && !candidate.sheet_.training.complete)
        throw std::runtime_error("Complete all pending training choices");
    candidate.inventory_ = inventory_;
    (void)rules.character_profile(candidate.sheet_, {});
    return candidate;
}
} // namespace opengold
