#include "opengold/character.h"
#include <algorithm>
#include <stdexcept>

namespace opengold
{
namespace
{
// Checked before the member is built, so a Character never holds an invalid look.
por::CharacterAppearance validated(por::CharacterAppearance appearance)
{
    por::validate_character_appearance(appearance);
    return appearance;
}
} // namespace

Character::Character(const rules::CharacterRules &rules, rules::CharacterDraft creation,
                     por::CharacterAppearance appearance)
    : creation_(std::move(creation)),
      sheet_(rules.evaluate(creation_, rules::NameRequirement::required)),
      appearance_(validated(std::move(appearance)))
{
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
                              std::uint64_t rest_session,
                              rules::ChoiceCompleteness completeness)
{
    if (!rest_session)
        throw std::runtime_error("Spell choices require a completed Long Rest");
    if (std::any_of(spell_edits_.begin(), spell_edits_.end(),
                    [&](const auto & edit)
{
    return edit.rest_session >= rest_session;
}))
    throw std::runtime_error("Spell choices already used for this rest");
    auto candidate = sheet_;
    auto history = spell_edits_;
    rules.apply_spell_choices(candidate, choices, rules::SpellChoiceContext::long_rest,
                              completeness);
    history.push_back({static_cast<unsigned>(sheet_.level), rest_session, choices});
    (void)rules.character_profile(candidate, {});
    sheet_ = std::move(candidate);
    spell_edits_ = std::move(history);
}

void Character::replace_rest_training(const rules::RulesModule &rules,
                                      std::span<const std::string> selections,
                                      std::uint64_t session)
{
    if (!session || std::any_of(training_edits_.begin(), training_edits_.end(),
                                [&](const auto & edit)
{
    return edit.rest_session >= session;
}))
    throw std::runtime_error("Training choices already used for this rest");
    auto candidate = sheet_;
    auto history = training_edits_;
    (void)rules.replace_rest_training(candidate, selections);
    history.push_back({static_cast<unsigned>(sheet_.level), session,
                       {selections.begin(), selections.end()}});
    (void)rules.character_profile(candidate, {});
    sheet_ = std::move(candidate);
    training_edits_ = std::move(history);
}
} // namespace opengold
