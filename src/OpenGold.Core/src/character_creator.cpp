#include "opengold/character_creator.h"
#include <algorithm>
#include <stdexcept>
namespace opengold
{
using namespace rules;
CharacterCreator::CharacterCreator(std::unique_ptr<CharacterRules> rules, std::uint64_t seed)
    : rules_(std::move(rules)), random_(seed)
{
    if (!rules_)
        throw std::runtime_error("Character creator requires a rules module");
    restart();
}
CharacterCreator::CharacterCreator(std::unique_ptr<CharacterRules> rules, CharacterDraft draft)
    : rules_(std::move(rules)), random_(0), draft_(std::move(draft)), step_(CreationStep::training)
{
    if (!rules_)
        throw std::runtime_error("Character creator requires a rules module");
    (void)rules_->evaluate(draft_, false);
    locked_training_ = draft_.training;
}
CharacterCreator::CharacterCreator(std::unique_ptr<CharacterRules> rules, Character character,
                                   const RulesModule &module)
    : CharacterCreator(std::move(rules), character.creation_data())
{
    // Retain ordinary creation-choice pruning when no advancement choices exist.
    if (module.training_options(character.sheet()).empty())
        return;
    // The campaign owns the module and outlives this isolated review editor.
    draft_.training = character.training_choices();
    locked_training_ = draft_.training;
    training_character_ = std::move(character);
    training_module_ = std::cref(module);
}
std::vector<TrainingChoiceGroup> CharacterCreator::training_options() const
{
    auto draft = draft_;
    if (training_character_)
        for (const auto &group :
             training_module_->get().training_options(training_character_->sheet()))
            draft.training.erase(group.id);
    auto groups = rules_->training_options(draft);
    if (training_character_)
    {
        const auto candidate = training_character_->preview_training(
            *rules_, training_module_->get(), draft_.training, false);
        const auto later = training_module_->get().training_options(candidate.sheet());
        groups.insert(groups.end(), later.begin(), later.end());
    }
    return groups;
}
void CharacterCreator::restart()
{
    draft_ = {};
    locked_training_.clear();
    training_character_.reset();
    training_module_.reset();
    appearance_ = {};
    step_ = CreationStep::race;
    for (auto field : {CreationField::race, CreationField::gender, CreationField::character_class,
                       CreationField::alignment, CreationField::background})
    {
        const auto options = rules_->choices(field);
        if (options.empty())
            throw std::runtime_error("Rules module has no character choices");
        select(field, options.front().id);
    }
}
void CharacterCreator::require_editable() const
{
    if (step_ == CreationStep::sheet)
        throw std::runtime_error("Return to editing before changing the character.");
}
void CharacterCreator::select(CreationField field, std::string_view id)
{
    require_editable();
    const auto choices = rules_->choices(field);
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto &c)
                     {
                         return c.id == id;
                     }))
        throw std::runtime_error("Unknown character choice");
    if (field == CreationField::character_class && step_ >= CreationStep::character_class &&
        !rules_->class_eligible(draft_, id))
        throw std::runtime_error("This class requires " +
                                 rules_->class_requirements(id).description + ".");
    auto candidate = draft_;
    switch (field)
    {
    case CreationField::race:
        candidate.race = id;
        break;
    case CreationField::gender:
        candidate.gender = id;
        break;
    case CreationField::character_class:
        candidate.character_class = id;
        break;
    case CreationField::alignment:
        candidate.alignment = id;
        break;
    case CreationField::background:
        candidate.background = id;
        candidate.adjustment = 0;
        break;
    }
    if (field == CreationField::character_class &&
        candidate.character_class != draft_.character_class)
    {
        candidate.spells.reset();
        const auto previous = rules_->training_options(draft_),
                   next = rules_->training_options(candidate);
        for (const auto &old : previous)
        {
            if (old.continuity_id.empty())
                continue;
            const auto values = candidate.training.find(old.id);
            const auto group = std::find_if(next.begin(), next.end(),
                                            [&](const auto &g)
                                            {
                                                return g.continuity_id == old.continuity_id;
                                            });
            if (values != candidate.training.end() && group != next.end() && group->id != old.id)
            {
                candidate.training[group->id] = std::move(values->second);
                candidate.training.erase(values);
            }
        }
    }
    const auto cantrips = rules_->cantrip_options(candidate);
    if (!candidate.cantrips)
        candidate.cantrips.emplace();
    std::erase_if(*candidate.cantrips,
                  [&](const auto &id)
                  {
                      return std::none_of(cantrips.options.begin(), cantrips.options.end(),
                                          [&](const auto &o)
                                          {
                                              return o.id == id;
                                          });
                  });
    if (candidate.cantrips->size() > cantrips.count)
        candidate.cantrips->resize(cantrips.count);
    prune_training(candidate);
    draft_ = std::move(candidate);
}
void CharacterCreator::cantrip_choice(std::string_view option, bool selected)
{
    require_editable();
    const auto group = rules_->cantrip_options(draft_);
    if (std::none_of(group.options.begin(), group.options.end(),
                     [&](const auto &o)
                     {
                         return o.id == option;
                     }))
        throw std::runtime_error("Unknown cantrip choice");
    auto candidate = draft_;
    if (!candidate.cantrips)
        candidate.cantrips.emplace();
    auto &choices = *candidate.cantrips;
    const auto found = std::find(choices.begin(), choices.end(), option);
    if (selected && found == choices.end())
    {
        if (choices.size() >= group.count)
            throw std::runtime_error("Cantrip selection limit reached");
        choices.emplace_back(option);
    }
    else if (!selected && found != choices.end())
        choices.erase(found);
    draft_ = std::move(candidate);
}
void CharacterCreator::spell_choice(std::string_view group, std::string_view option, bool selected)
{
    require_editable();
    const auto options = rules_->spell_choice_options(draft_);
    auto next = draft_;
    if (!next.spells)
        next.spells.emplace();
    std::vector<std::string> *values{};
    unsigned count{};
    const std::vector<CreationChoice> *available{};
    if (group == "prepared")
    {
        if (!options.may_prepare)
            throw std::runtime_error("Preparation is unavailable");
        if (!next.spells->prepared)
            next.spells->prepared.emplace();
        values = &*next.spells->prepared;
        count = options.prepared_count;
        available = &options.preparation;
    }
    else
    {
        const auto found = std::find_if(options.learning.begin(), options.learning.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == group;
                                        });
        if (found == options.learning.end())
            throw std::runtime_error("Unknown spell choice group");
        values = &next.spells->learning[std::string(group)];
        count = found->count;
        available = &found->options;
    }
    if (std::none_of(available->begin(), available->end(),
                     [&](const auto &o)
                     {
                         return o.id == option;
                     }))
        throw std::runtime_error("Unknown spell choice");
    const auto found = std::find(values->begin(), values->end(), option);
    if (selected && found == values->end())
    {
        if (values->size() >= count)
            throw std::runtime_error("Spell choice limit reached");
        values->emplace_back(option);
    }
    else if (!selected && found != values->end())
        values->erase(found);
    if (group != "prepared" && next.spells->prepared)
    {
        // A removed book entry cannot remain prepared in a draft.
        const auto before = *next.spells->prepared;
        next.spells->prepared->clear();
        const auto current = rules_->spell_choice_options(next);
        for (const auto &id : before)
            if (std::any_of(current.preparation.begin(), current.preparation.end(),
                            [&](const auto &o)
                            {
                                return o.id == id;
                            }))
                next.spells->prepared->push_back(id);
    }
    (void)rules_->evaluate(next, false);
    draft_ = std::move(next);
}
bool CharacterCreator::spell_choices_complete() const
{
    const auto options = rules_->spell_choice_options(draft_);
    if (!options.may_prepare)
        return true;
    if (!draft_.spells)
        return false;
    const auto cantrips = rules_->cantrip_options(draft_);
    if (!draft_.cantrips ||
        draft_.cantrips->size() != std::min<std::size_t>(cantrips.count, cantrips.options.size()))
        return false;
    for (const auto &group : options.learning)
    {
        const auto found = draft_.spells->learning.find(group.id);
        if ((found == draft_.spells->learning.end() ? 0 : found->second.size()) !=
            std::min<std::size_t>(group.count, group.options.size()))
            return false;
    }
    return draft_.spells->prepared &&
           draft_.spells->prepared->size() ==
               std::min<std::size_t>(options.prepared_count, options.preparation.size());
}
void CharacterCreator::prune_training(CharacterDraft &candidate) const
{
    // Removing a parent selection can invalidate a dependent group. Query the
    // rules again until stable; this only removes choices, never invents them.
    for (;;)
    {
        const auto before = candidate.training;
        const auto groups = rules_->training_options(candidate);
        for (auto it = candidate.training.begin(); it != candidate.training.end();)
        {
            const auto group = std::find_if(groups.begin(), groups.end(),
                                            [&](const auto &g)
                                            {
                                                return g.id == it->first;
                                            });
            if (group == groups.end())
            {
                it = candidate.training.erase(it);
                continue;
            }
            auto &values = it->second;
            std::erase_if(values,
                          [&](const auto &value)
                          {
                              return std::none_of(group->options.begin(), group->options.end(),
                                                  [&](const auto &o)
                                                  {
                                                      return o.id == value;
                                                  });
                          });
            if (values.size() > group->count)
                values.resize(group->count);
            if (values.empty())
                it = candidate.training.erase(it);
            else
                ++it;
        }
        if (before == candidate.training)
            return;
    }
}
void CharacterCreator::training_choice(std::string_view id, std::string_view option, bool selected)
{
    require_editable();
    auto candidate = draft_;
    const auto groups = training_options();
    const auto group = std::find_if(groups.begin(), groups.end(),
                                    [&](const auto &g)
                                    {
                                        return g.id == id;
                                    });
    if (group == groups.end() || std::none_of(group->options.begin(), group->options.end(),
                                              [&](const auto &o)
                                              {
                                                  return o.id == option;
                                              }))
        throw std::runtime_error("Unknown training choice");
    auto &values = candidate.training[std::string(id)];
    const auto found = std::find(values.begin(), values.end(), option);
    if (selected && group->control == TrainingChoiceControl::single_selection)
    {
        if (group->count != 1)
            throw std::runtime_error("Invalid single-selection training group");
        values = {std::string(option)};
    }
    else if (selected && found == values.end())
    {
        if (values.size() >= group->count)
            throw std::runtime_error("Training selection limit reached");
        values.emplace_back(option);
    }
    else if (!selected && found != values.end())
        values.erase(found);
    if (training_character_)
    {
        if (values.empty())
            candidate.training.erase(std::string(id));
        (void)training_character_->preview_training(*rules_, training_module_->get(),
                                                    candidate.training, false);
    }
    else
        prune_training(candidate);
    for (const auto &[group, selected] : locked_training_)
    {
        const auto found = candidate.training.find(group);
        if (found == candidate.training.end() ||
            std::any_of(selected.begin(), selected.end(),
                        [&](const auto &value)
                        {
                            return std::find(found->second.begin(), found->second.end(), value) ==
                                   found->second.end();
                        }))
            throw std::runtime_error("Previously selected training cannot be replaced");
    }
    draft_ = std::move(candidate);
}
bool CharacterCreator::training_complete() const
{
    for (const auto &group : training_options())
    {
        const auto found = draft_.training.find(group.id);
        if (group.count && (found == draft_.training.end() || found->second.size() != group.count))
            return false;
    }
    return true;
}
void CharacterCreator::target_class(std::string_view id, bool selected)
{
    require_editable();
    (void)rules_->class_requirements(id);
    const auto found = std::find(draft_.target_classes.begin(), draft_.target_classes.end(), id);
    if (selected && found == draft_.target_classes.end())
        draft_.target_classes.emplace_back(id);
    else if (!selected && found != draft_.target_classes.end())
        draft_.target_classes.erase(found);
}
void CharacterCreator::select_adjustment(unsigned index)
{
    require_editable();
    if (index >= rules_->adjustments(draft_.background).size())
        throw std::runtime_error("Invalid score adjustment");
    draft_.adjustment = index;
}
void CharacterCreator::roll()
{
    require_editable();
    draft_.rolls = rules_->roll(random_);
    draft_.rolled = true;
    draft_.assignment.fill(6);
}
bool CharacterCreator::scores_assigned() const
{
    return draft_.rolled && std::all_of(draft_.assignment.begin(), draft_.assignment.end(),
                                        [](auto n)
                                        {
                                            return n < 6;
                                        });
}
void CharacterCreator::assign_roll(unsigned roll, unsigned ability)
{
    require_editable();
    if (!draft_.rolled || roll >= 6 || ability >= 6)
        throw std::runtime_error("Invalid roll assignment");
    const auto source = std::find(draft_.assignment.begin(), draft_.assignment.end(), roll);
    if (source != draft_.assignment.end())
        std::swap(*source, draft_.assignment[ability]);
    else
        draft_.assignment[ability] = roll; // A displaced result returns to the unassigned rolls.
}
void CharacterCreator::swap_scores(unsigned first, unsigned second)
{
    require_editable();
    if (!draft_.rolled || first >= 6 || second >= 6)
        throw std::runtime_error("Invalid score swap");
    std::swap(draft_.assignment[first], draft_.assignment[second]);
}
void CharacterCreator::name(std::string text)
{
    require_editable();
    const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
    draft_.name = first == std::string::npos ? "" : text.substr(first, last - first + 1);
}
void CharacterCreator::appearance(por::CharacterAppearance value)
{
    por::validate_character_appearance(value);
    appearance_ = value;
}
CharacterSheet CharacterCreator::sheet() const
{
    if (training_character_)
        return training_character_
            ->preview_training(*rules_, training_module_->get(), draft_.training, false)
            .sheet();
    return rules_->evaluate(draft_, step_ >= CreationStep::combat_icon);
}
Character CharacterCreator::create_character() const
{
    if (step_ != CreationStep::sheet)
        throw std::runtime_error("Finish character creation before exporting the character");
    if (!training_complete())
        throw std::runtime_error("Complete the required training choices.");
    if (!rules_->class_eligible(draft_, draft_.character_class))
        throw std::runtime_error("Starting class prerequisites are not met.");
    return Character(*rules_, draft_, appearance_);
}
void CharacterCreator::next()
{
    if (step_ == CreationStep::sheet)
        return;
    if (step_ >= CreationStep::spell_choices && !spell_choices_complete())
        throw std::runtime_error("Complete available spell choices.");
    if (step_ >= CreationStep::training && !training_complete())
        throw std::runtime_error("Complete the required training choices.");
    if (step_ >= CreationStep::attributes)
        (void)rules_->evaluate(draft_, step_ >= CreationStep::name);
    if (step_ >= CreationStep::character_class &&
        !rules_->class_eligible(draft_, draft_.character_class))
        throw std::runtime_error("Choose a qualified starting class. Requires " +
                                 rules_->class_requirements(draft_.character_class).description +
                                 ".");
    step_ = static_cast<CreationStep>(static_cast<unsigned>(step_) + 1);
    if (step_ == CreationStep::spell_choices && rules_->cantrip_options(draft_).options.empty())
        step_ = CreationStep::name;
    if (step_ == CreationStep::spell_choices && !draft_.spells &&
        rules_->spell_choice_options(draft_).may_prepare)
        draft_.spells = SpellChoices{};
}
void CharacterCreator::back()
{
    if (step_ != CreationStep::race)
        step_ = static_cast<CreationStep>(static_cast<unsigned>(step_) - 1);
    if (step_ == CreationStep::spell_choices && rules_->cantrip_options(draft_).options.empty())
        step_ = CreationStep::training;
}
} // namespace opengold
