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

void Character::rename(std::string name)
{
    if (name.empty())
    {
        throw std::runtime_error("A character needs a name");
    }
    creation_.name = name;
    sheet_.name = std::move(name);
}

Character Character::recreated(const rules::CharacterRules &creation,
                               const rules::RulesModule &rules, rules::CharacterDraft draft) const
{
    Character result(creation, std::move(draft), appearance_);
    result.replay(rules, advancements_, spell_edits_, training_edits_, hall_training_);
    result.inventory_ = inventory_;
    return result;
}

void Character::replay(const rules::RulesModule &rules,
                       std::span<const rules::AdvancementChoice> history,
                       std::span<const SpellChoiceEdit> spell_edits,
                       std::span<const TrainingChoiceEdit> training_edits,
                       std::span<const HallTraining> hall_training)
{
    rules::VitalState scratch;
    // Each list is in the order its entries were made, and an entry is
    // replayed once its turn comes and the character is at its level: a
    // level drain may bring a character back to a level it has left, so
    // the same level can hold entries made before and after the drain.
    std::size_t next_spells = 0, next_training = 0, next_hall = 0;
    const auto is_petition = [](const auto &choice)
    {
        return choice.training.contains("hall_petition");
    };
    auto remaining_petitions =
        rules.plays_original_records() ? std::ranges::count_if(history, is_petition) : 0;
    const bool has_petition_history = remaining_petitions != 0;
    const auto at_level = [&]
    {
        const auto level = static_cast<unsigned>(sheet_.level);

        // Original scroll grants only add to the spellbook. A later rest
        // at this same level may already prepare that spell, while these
        // older journals keep spell choices and hall grants in separate
        // lists. Make the spells available before validating the choices;
        // record the grants in their original order in the hall loop below.
        if (rules.plays_original_records())
        {
            for (auto at = next_hall; at < hall_training.size() && hall_training[at].level == level;
                 ++at)
            {
                if (hall_training[at].grant.starts_with("boon:scroll:"))
                {
                    rules.train_at_hall(sheet_, hall_training[at].grant);
                }
            }
        }

        for (; next_spells < spell_edits.size() && spell_edits[next_spells].level == level;
             ++next_spells)
        {
            const auto &edit = spell_edits[next_spells];
            // A Crown knight can revisit its earlier level after a Sword
            // petition. Its new divine preparation cannot belong to the
            // earlier noncaster visit; leave it for the actual later grant.
            if (remaining_petitions > 0 && edit.choices.prepared &&
                !edit.choices.prepared->empty() &&
                !rules.spell_choice_options(sheet_, rules::SpellChoiceContext::long_rest)
                     .may_prepare)
            {
                break;
            }
            choose_spells(rules, edit.choices, edit.rest_session,
                          rules::SpellChoiceContext::long_rest, rules::ChoiceCompleteness::partial);
        }
        for (;
             next_training < training_edits.size() && training_edits[next_training].level == level;
             ++next_training)
        {
            const auto &edit = training_edits[next_training];
            replace_rest_training(rules, edit.selections, edit.rest_session);
        }
        for (; next_hall < hall_training.size() && hall_training[next_hall].level == level;
             ++next_hall)
        {
            train_at_hall(rules, hall_training[next_hall].grant);
        }
    };
    at_level();
    for (const auto &choice : history)
    {
        if (!advance(rules, scratch, choice))
        {
            throw std::runtime_error("Unsupported saved advancement choice");
        }
        if (remaining_petitions > 0 && is_petition(choice))
        {
            --remaining_petitions;
        }
        at_level();
    }
    if (has_petition_history && next_spells != spell_edits.size())
    {
        throw std::runtime_error("Saved spell choices do not match the petition history");
    }
}

void Character::appearance(por::CharacterAppearance value)
{
    por::validate_character_appearance(value);
    appearance_ = std::move(value);
}

bool Character::advance(const rules::AdvancementRules &rules, rules::VitalState &state)
{
    return advance(rules, state, rules.default_advancement(sheet_));
}

bool Character::advance(const rules::AdvancementRules &rules, rules::VitalState &state,
                        const rules::AdvancementChoice &choice)
{
    auto sheet = sheet_;
    auto vitals = state;
    auto history = advancements_;
    if (!rules.advance_character(sheet, vitals, choice))
    {
        return false;
    }
    history.push_back(choice);
    sheet_ = std::move(sheet);
    state = std::move(vitals);
    advancements_ = std::move(history);
    return true;
}

void Character::choose_spells(const rules::AdvancementRules &rules,
                              const rules::SpellChoices &choices, std::uint64_t rest_session,
                              rules::SpellChoiceContext context,
                              rules::ChoiceCompleteness completeness)
{
    if (!rest_session)
    {
        throw std::runtime_error("Spell choices require a completed Long Rest");
    }
    if (std::any_of(spell_edits_.begin(), spell_edits_.end(),
                    [&](const auto &edit)
                    {
                        return edit.rest_session >= rest_session;
                    }))
    {
        throw std::runtime_error("Spell choices already used for this rest");
    }
    auto candidate = sheet_;
    auto history = spell_edits_;
    rules.apply_spell_choices(candidate, choices, context, completeness);
    // The same spells chosen again at the same level change nothing on
    // replay: the last choice only moves to this rest, so a long game's
    // nightly choices do not grow its save without end.
    const auto level = static_cast<unsigned>(sheet_.level);
    if (!history.empty() && history.back().level == level && history.back().choices == choices)
    {
        history.back().rest_session = rest_session;
    }
    else
    {
        history.push_back({level, rest_session, choices});
    }
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
    {
        throw std::runtime_error("Training choices already used for this rest");
    }
    const auto *resources = rules.rest_resources();
    if (!resources)
    {
        throw std::runtime_error("These rules have no training a rest replaces");
    }
    auto candidate = sheet_;
    auto history = training_edits_;
    (void)resources->replace_rest_training(candidate, selections);
    history.push_back(
        {static_cast<unsigned>(sheet_.level), session, {selections.begin(), selections.end()}});
    (void)rules.character_profile(candidate, {});
    sheet_ = std::move(candidate);
    training_edits_ = std::move(history);
}

void Character::train_at_hall(const rules::AdvancementRules &rules, std::string grant)
{
    auto candidate = sheet_;
    auto history = hall_training_;
    rules.train_at_hall(candidate, grant);
    history.push_back({static_cast<unsigned>(sheet_.level), std::move(grant)});
    sheet_ = std::move(candidate);
    hall_training_ = std::move(history);
}
} // namespace opengold
