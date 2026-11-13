#include "opengold/character_creator.h"
#include "opengold/diagnostic_log.h"
#include "opengold/dice.h"
#include <algorithm>
#include <stdexcept>

namespace opengold
{
using namespace rules;

namespace
{
bool all_scores_assigned(const CharacterDraft &draft)
{
    return draft.rolled && std::all_of(draft.assignment.begin(), draft.assignment.end(),
                                       [](auto n)
                                       {
                                           return n < 6;
                                       });
}

// A species choice's picks; none when the draft has not made it.
std::vector<std::string> selected_species(const CharacterDraft &draft, const std::string &group)
{
    const auto found = draft.training.find(group);
    return found == draft.training.end() ? std::vector<std::string>{} : found->second;
}
} // namespace

CharacterDraft reincarnation_draft(const Character &former, std::string_view species)
{
    auto draft = former.creation_data();
    const auto versatile = selected_species(draft, "species:human:versatile");
    if (draft.spells && !versatile.empty())
    {
        std::erase_if(draft.spells->learning,
                      [&](const auto &group)
                      {
                          return group.first.starts_with("feat:" + versatile.front() + ":");
                      });
    }
    std::erase_if(draft.training,
                  [](const auto &group)
                  {
                      return group.first.starts_with("species:");
                  });
    draft.race = species;
    std::vector<std::string> kept;
    const auto &grants = former.sheet().grants;
    for (const auto &grant : grants)
    {
        if (grant.source_id.starts_with("species:") && grant.id.starts_with("skill:") &&
            std::any_of(grants.begin(), grants.end(),
                        [&](const auto &other)
                        {
                            return other.id == "expertise:" + grant.id.substr(6);
                        }))
        {
            kept.push_back(grant.id.substr(6));
        }
    }
    if (!kept.empty())
    {
        draft.training["species:" + std::string(species) + ":kept"] = std::move(kept);
    }
    return draft;
}

CharacterCreator::CharacterCreator(std::shared_ptr<const CharacterRules> rules, std::uint64_t seed)
    : rules_(std::move(rules)), random_{seed}, gold_random_{~seed}
{
    if (!rules_)
    {
        throw std::runtime_error("Character creator requires a rules module");
    }
    restart();
}

CharacterCreator::CharacterCreator(std::shared_ptr<const CharacterRules> creation,
                                   const RulesModule &rules, const Character &former,
                                   std::string_view species)
    : rules_(std::move(creation)), former_(former), module_(&rules), random_{0}, gold_random_{0}
{
    if (!rules_)
    {
        throw std::runtime_error("Character creator requires a rules module");
    }
    for (const auto &grant : former.sheet().grants)
    {
        if (!grant.source_id.starts_with("species:"))
        {
            held_.insert(grant.id);
        }
    }
    draft_ = reincarnation_draft(former, species);
    appearance_ = former.appearance();
    fill_species(draft_, CharacterDraft{}, {});
}

std::vector<TrainingChoiceGroup> CharacterCreator::training_options() const
{
    auto groups = rules_->training_options(draft_);
    if (former_)
    {
        for (auto &group : groups)
        {
            if (group.id.starts_with("species:") && !group.id.ends_with(":kept"))
            {
                std::erase_if(group.options,
                              [&](const auto &option)
                              {
                                  return held_.contains("skill:" + option.id) ||
                                         held_.contains("cantrip:" + option.id) ||
                                         held_.contains("spell:" + option.id);
                              });
            }
        }
    }
    return groups;
}

TrainingChoiceGroup CharacterCreator::cantrip_options() const
{
    return former_ ? TrainingChoiceGroup{} : rules_->cantrip_options(draft_);
}

std::string CharacterCreator::initiate_group_prefix() const
{
    const auto versatile = selected_species(draft_, "species:human:versatile");
    return versatile.empty() || !versatile.front().starts_with("magic_initiate_")
               ? std::string{}
               : "feat:" + versatile.front() + ":";
}

SpellChoiceOptions CharacterCreator::spell_choice_options() const
{
    auto options = rules_->spell_choice_options(draft_);
    if (!former_)
    {
        return options;
    }
    SpellChoiceOptions initiate;
    const auto prefix = initiate_group_prefix();
    for (auto &group : options.learning)
    {
        if (!prefix.empty() && group.id.starts_with(prefix))
        {
            std::erase_if(group.options,
                          [&](const auto &option)
                          {
                              return held_.contains("spell:" + option.id) ||
                                     held_.contains("cantrip:" + option.id);
                          });
            group.count = std::min(group.count, static_cast<unsigned>(group.options.size()));
            initiate.learning.push_back(std::move(group));
        }
    }
    return initiate;
}

ReincarnationChoices CharacterCreator::reincarnation_choices() const
{
    if (!former_)
    {
        throw std::runtime_error("Not a reincarnation");
    }
    ReincarnationChoices choices;
    for (const auto &[group, picks] : draft_.training)
    {
        if (group.starts_with("species:") && !group.ends_with(":kept"))
        {
            choices.training[group] = picks;
        }
    }
    const auto prefix = initiate_group_prefix();
    if (draft_.spells && !prefix.empty())
    {
        for (const auto &[group, picks] : draft_.spells->learning)
        {
            if (group.starts_with(prefix))
            {
                choices.initiate_spells[group] = picks;
            }
        }
    }
    return choices;
}

void CharacterCreator::check_reincarnation() const
{
    try
    {
        (void)former_->recreated(*rules_, *module_, draft_);
    }
    catch (const std::exception &)
    {
        throw std::runtime_error(
            "These choices clash with what the character learned later. Choose others.");
    }
}

void CharacterCreator::restart()
{
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character");
    }
    draft_ = {};
    set_aside_.clear();
    appearance_ = {};
    step_ = CreationStep::race;
    starting_gold_ = 0;
    for (auto field : {CreationField::race, CreationField::gender, CreationField::character_class,
                       CreationField::alignment, CreationField::background})
    {
        const auto options = rules_->choices(field);
        if (options.empty())
        {
            throw std::runtime_error("Rules module has no character choices");
        }
        select(field, options.front().id);
    }
}

void CharacterCreator::require_editable() const
{
    if (step_ == CreationStep::sheet)
    {
        throw std::runtime_error("Return to editing before changing the character.");
    }
}

void CharacterCreator::fill_species(CharacterDraft &candidate, const CharacterDraft &before,
                                    std::string_view touched) const
{
    // A species choice that appears starts with its first options, shown on
    // the race page for the player to change: a new species's choices, or
    // Skilled when Versatile comes back to it. One the player made before it
    // went away comes back with their picks. A choice another field
    // took away (Versatile's feat, when the background gives that feat; a
    // Skilled skill the new background gives) is refilled the same way.
    // A filled choice can bring another (Versatile's Skilled, its skills).
    // What the player left, the choice they just changed among it, stays as
    // they left it, even emptied.
    // A refill skips what another choice took: the background taking a
    // Skilled skill must not refill it with one the class already picked.
    std::set<std::string, std::less<>> offered_before;
    for (const auto &group : rules_->training_options(before))
    {
        offered_before.insert(group.id);
    }
    const auto left_by_player = [&](const std::string &group)
    {
        return group == touched ||
               (offered_before.contains(group) &&
                selected_species(candidate, group) == selected_species(before, group));
    };
    const auto taken_elsewhere = [&](std::string_view group, std::string_view option)
    {
        return std::any_of(candidate.training.begin(), candidate.training.end(),
                           [&](const auto &entry)
                           {
                               return entry.first != group &&
                                      std::find(entry.second.begin(), entry.second.end(), option) !=
                                          entry.second.end();
                           });
    };
    const auto offered = [&]
    {
        auto groups = rules_->training_options(candidate);
        if (former_)
        {
            for (auto &group : groups)
            {
                if (!group.id.ends_with(":kept"))
                {
                    std::erase_if(group.options,
                                  [&](const auto &option)
                                  {
                                      return held_.contains("skill:" + option.id) ||
                                             held_.contains("cantrip:" + option.id) ||
                                             held_.contains("spell:" + option.id);
                                  });
                }
            }
        }
        return groups;
    };
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : offered())
        {
            if (!group.id.starts_with("species:") ||
                selected_species(candidate, group.id).size() >= group.count ||
                left_by_player(group.id))
            {
                continue;
            }
            std::vector<std::string> order;
            if (const auto earlier = set_aside_.find(group.id);
                earlier != set_aside_.end() && !offered_before.contains(group.id))
            {
                order = earlier->second;
            }
            for (const auto &option : group.options)
            {
                order.push_back(option.id);
            }
            auto &picks = candidate.training[group.id];
            for (const auto &id : order)
            {
                const bool listed = std::any_of(group.options.begin(), group.options.end(),
                                                [&](const auto &o)
                                                {
                                                    return o.id == id;
                                                });
                if (listed && picks.size() < group.count &&
                    std::find(picks.begin(), picks.end(), id) == picks.end() &&
                    !taken_elsewhere(group.id, id))
                {
                    picks.push_back(id);
                    filled = true;
                }
            }
            if (picks.empty())
            {
                candidate.training.erase(group.id);
            }
        }
    }
}

void CharacterCreator::set_aside_vanished(const CharacterDraft &candidate)
{
    std::set<std::string, std::less<>> offered;
    for (const auto &group : rules_->training_options(candidate))
    {
        offered.insert(group.id);
    }
    for (const auto &[group, picks] : draft_.training)
    {
        if (group.starts_with("species:") && !offered.contains(group))
        {
            set_aside_[group] = picks;
        }
    }
}

void CharacterCreator::select(CreationField field, std::string_view id)
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character");
    }
    const auto choices = rules_->choices(field);
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto &c)
                     {
                         return c.id == id;
                     }))
    {
        throw std::runtime_error("Unknown character choice");
    }
    if (field == CreationField::character_class && step_ >= CreationStep::character_class &&
        !rules::class_eligible(*rules_, draft_, id))
    {
        throw std::runtime_error("This class requires " +
                                 rules_->class_requirements(id).description + ".");
    }
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
            {
                continue;
            }
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
    if (!candidate.cantrips)
    {
        candidate.cantrips.emplace();
    }
    prune_training(candidate);
    fill_species(candidate, draft_, {});
    prune_spells(candidate);
    set_aside_vanished(candidate);
    draft_ = std::move(candidate);
}

void CharacterCreator::cantrip_choice(std::string_view option, bool selected)
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character's cantrips");
    }
    const auto group = rules_->cantrip_options(draft_);
    if (std::none_of(group.options.begin(), group.options.end(),
                     [&](const auto &o)
                     {
                         return o.id == option;
                     }))
    {
        throw std::runtime_error("Unknown cantrip choice");
    }
    auto candidate = draft_;
    if (!candidate.cantrips)
    {
        candidate.cantrips.emplace();
    }
    auto &choices = *candidate.cantrips;
    const auto found = std::find(choices.begin(), choices.end(), option);
    if (selected && found == choices.end())
    {
        if (choices.size() >= group.count)
        {
            throw std::runtime_error("Cantrip selection limit reached");
        }
        choices.emplace_back(option);
    }
    else if (!selected && found != choices.end())
    {
        choices.erase(found);
    }
    draft_ = std::move(candidate);
}

void CharacterCreator::spell_choice(ChoiceToggle toggle)
{
    const auto [group, option, selected] = toggle;
    require_editable();
    const auto options = spell_choice_options();
    auto next = draft_;
    if (!next.spells)
    {
        next.spells.emplace();
    }
    std::vector<std::string> *values{};
    unsigned count{};
    const std::vector<CreationChoice> *available{};
    if (group == "prepared")
    {
        if (!options.may_prepare)
        {
            throw std::runtime_error("Preparation is unavailable");
        }
        if (!next.spells->prepared)
        {
            next.spells->prepared.emplace();
        }
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
        {
            throw std::runtime_error("Unknown spell choice group");
        }
        values = &next.spells->learning[std::string(group)];
        count = found->count;
        available = &found->options;
    }
    if (std::none_of(available->begin(), available->end(),
                     [&](const auto &o)
                     {
                         return o.id == option;
                     }))
    {
        throw std::runtime_error("Unknown spell choice");
    }
    const auto found = std::find(values->begin(), values->end(), option);
    if (selected && found == values->end())
    {
        if (values->size() >= count)
        {
            throw std::runtime_error("Spell choice limit reached");
        }
        values->emplace_back(option);
    }
    else if (!selected && found != values->end())
    {
        values->erase(found);
    }
    // A group with nothing picked holds no entry: the group can go away (the
    // class's cantrips can take every one Magic Initiate offered), and the
    // rules refuse picks for a group they no longer offer.
    if (group != "prepared" && values->empty())
    {
        next.spells->learning.erase(std::string(group));
    }
    if (group != "prepared" && next.spells->prepared)
    {
        // A removed book entry cannot remain prepared in a draft.
        const auto before = *next.spells->prepared;
        next.spells->prepared->clear();
        const auto current = rules_->spell_choice_options(next);
        for (const auto &id : before)
        {
            if (std::any_of(current.preparation.begin(), current.preparation.end(),
                            [&](const auto &o)
                            {
                                return o.id == id;
                            }))
            {
                next.spells->prepared->push_back(id);
            }
        }
    }
    (void)rules_->evaluate(next, NameRequirement::optional);
    draft_ = std::move(next);
}

// The Spell Choices step appears for cantrips, spells to learn or spells to
// prepare; while reincarnating, for Versatile Magic Initiate's spells alone.
bool CharacterCreator::has_spell_choices() const
{
    if (!former_ && !rules_->cantrip_options(draft_).options.empty())
    {
        return true;
    }
    const auto options = spell_choice_options();
    return options.may_prepare || !options.learning.empty();
}

bool CharacterCreator::spell_choices_complete() const
{
    const auto options = spell_choice_options();
    if (!options.may_prepare && options.learning.empty())
    {
        return true;
    }
    if (!draft_.spells)
    {
        return false;
    }
    const auto cantrips = rules_->cantrip_options(draft_);
    if (!former_ &&
        (!draft_.cantrips ||
         draft_.cantrips->size() != std::min<std::size_t>(cantrips.count, cantrips.options.size())))
    {
        return false;
    }
    for (const auto &group : options.learning)
    {
        const auto found = draft_.spells->learning.find(group.id);
        if ((found == draft_.spells->learning.end() ? 0 : found->second.size()) !=
            std::min<std::size_t>(group.count, group.options.size()))
        {
            return false;
        }
    }
    return !options.may_prepare ||
           (draft_.spells->prepared &&
            draft_.spells->prepared->size() ==
                std::min<std::size_t>(options.prepared_count, options.preparation.size()));
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
            {
                values.resize(group->count);
            }
            if (values.empty())
            {
                it = candidate.training.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (before == candidate.training)
        {
            return;
        }
    }
}

namespace
{
// Keeps the picks `offered` still lists, at most `count` of them.
void keep_offered(std::vector<std::string> &picks, const std::vector<CreationChoice> &offered,
                  std::size_t count)
{
    std::erase_if(picks,
                  [&](const auto &id)
                  {
                      return std::none_of(offered.begin(), offered.end(),
                                          [&](const auto &o)
                                          {
                                              return o.id == id;
                                          });
                  });
    if (picks.size() > count)
    {
        picks.resize(count);
    }
}
} // namespace

void CharacterCreator::prune_spells(CharacterDraft &candidate) const
{
    // A change elsewhere can take away what a spell pick came from: the
    // Versatile feat leaving Magic Initiate, another background or species,
    // or the Divine Order no longer giving the extra cantrip. Those picks go;
    // the rest stay. Each step leaves the draft valid for the next one's
    // rules query.
    if (!former_ && candidate.cantrips)
    {
        const auto cantrips = rules_->cantrip_options(candidate);
        keep_offered(*candidate.cantrips, cantrips.options, cantrips.count);
    }
    // The spell options come from the scores; while they are unassigned
    // (rolled again) the options are unknown, so the picks wait.
    if (!candidate.spells || !all_scores_assigned(candidate))
    {
        return;
    }
    auto unpicked = candidate;
    unpicked.spells = SpellChoices{};
    const auto offered = rules_->spell_choice_options(unpicked);
    for (auto entry = candidate.spells->learning.begin();
         entry != candidate.spells->learning.end();)
    {
        const auto group = std::find_if(offered.learning.begin(), offered.learning.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == entry->first;
                                        });
        if (group != offered.learning.end())
        {
            keep_offered(entry->second, group->options, group->count);
        }
        entry = group == offered.learning.end() || entry->second.empty()
                    ? candidate.spells->learning.erase(entry)
                    : std::next(entry);
    }
    if (candidate.spells->prepared)
    {
        const auto current = rules_->spell_choice_options(candidate);
        keep_offered(*candidate.spells->prepared, current.preparation, current.prepared_count);
    }
}

void CharacterCreator::training_choice(ChoiceToggle toggle)
{
    const auto [id, option, selected] = toggle;
    require_editable();
    auto candidate = draft_;
    const auto groups = training_options();
    const auto group = std::find_if(groups.begin(), groups.end(),
                                    [&](const auto &g)
                                    {
                                        return g.id == id;
                                    });
    if (former_ && (!id.starts_with("species:") || id.ends_with(":kept")))
    {
        throw std::runtime_error("A new body changes only the species' choices");
    }
    if (group == groups.end() || std::none_of(group->options.begin(), group->options.end(),
                                              [&](const auto &o)
                                              {
                                                  return o.id == option;
                                              }))
    {
        throw std::runtime_error("Unknown training choice");
    }
    auto &values = candidate.training[std::string(id)];
    const auto found = std::find(values.begin(), values.end(), option);
    if (selected && group->control == TrainingChoiceControl::single_selection)
    {
        if (group->count != 1)
        {
            throw std::runtime_error("Invalid single-selection training group");
        }
        values = {std::string(option)};
    }
    else if (selected && found == values.end())
    {
        if (values.size() >= group->count)
        {
            throw std::runtime_error("Training selection limit reached");
        }
        values.emplace_back(option);
    }
    else if (!selected && found != values.end())
    {
        values.erase(found);
    }
    prune_training(candidate);
    fill_species(candidate, draft_, id);
    prune_spells(candidate);
    set_aside_vanished(candidate);
    draft_ = std::move(candidate);
}

bool CharacterCreator::species_complete() const
{
    for (const auto &group : training_options())
    {
        if (!group.id.starts_with("species:"))
        {
            continue;
        }
        const auto found = draft_.training.find(group.id);
        if (group.count && (found == draft_.training.end() || found->second.size() != group.count))
        {
            return false;
        }
    }
    return true;
}

bool CharacterCreator::has_species_choices() const
{
    const auto groups = training_options();
    return std::any_of(groups.begin(), groups.end(),
                       [](const auto &group)
                       {
                           return group.id.starts_with("species:");
                       });
}

bool CharacterCreator::training_complete() const
{
    for (const auto &group : training_options())
    {
        // A species' choices are made on the race page (species_complete).
        if (group.id.starts_with("species:"))
        {
            continue;
        }
        const auto found = draft_.training.find(group.id);
        if (group.count && (found == draft_.training.end() || found->second.size() != group.count))
        {
            return false;
        }
    }
    return true;
}

void CharacterCreator::target_class(std::string_view id, bool selected)
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character");
    }
    (void)rules_->class_requirements(id);
    const auto found = std::find(draft_.target_classes.begin(), draft_.target_classes.end(), id);
    if (selected && found == draft_.target_classes.end())
    {
        draft_.target_classes.emplace_back(id);
    }
    else if (!selected && found != draft_.target_classes.end())
    {
        draft_.target_classes.erase(found);
    }
}

void CharacterCreator::select_adjustment(unsigned index)
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character");
    }
    if (index >= rules_->adjustments(draft_.background).size())
    {
        throw std::runtime_error("Invalid score adjustment");
    }
    draft_.adjustment = index;
}

void CharacterCreator::roll()
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character's scores");
    }
    const RollPurpose purpose("ability scores and starting gold");
    draft_.rolls = rules_->roll(random_);
    draft_.rolled = true;
    draft_.assignment = {6, 6, 6, 6, 6, 6}; // Every rolled result starts unassigned.
    starting_gold_ = 0;
    for (int die = 0; die < 3; ++die)
    {
        starting_gold_ += static_cast<std::uint16_t>(roll_die(gold_random_, 6) * 10);
    }
}

bool CharacterCreator::scores_assigned() const
{
    return all_scores_assigned(draft_);
}

void CharacterCreator::assign_roll(unsigned roll, rules::Ability ability)
{
    require_editable();
    // The game converts a box index from the UI, so an ability can still be out of range.
    if (!draft_.rolled || roll >= 6 || rules::ability_index(ability) >= 6)
    {
        throw std::runtime_error("Invalid roll assignment");
    }
    const auto source = std::find(draft_.assignment.begin(), draft_.assignment.end(), roll);
    if (source != draft_.assignment.end())
    {
        std::swap(*source, draft_.assignment[ability]);
    }
    else
    {
        draft_.assignment[ability] = roll; // A displaced result returns to the unassigned rolls.
    }
}

void CharacterCreator::swap_scores(rules::Ability first, rules::Ability second)
{
    require_editable();
    // As in assign_roll, an ability converted from a UI index may be out of range.
    if (!draft_.rolled || rules::ability_index(first) >= 6 || rules::ability_index(second) >= 6)
    {
        throw std::runtime_error("Invalid score swap");
    }
    std::swap(draft_.assignment[first], draft_.assignment[second]);
}

void CharacterCreator::name(std::string text)
{
    require_editable();
    if (former_)
    {
        throw std::runtime_error("A new body keeps the character's name");
    }
    const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
    draft_.name = first == std::string::npos ? "" : text.substr(first, last - first + 1);
}

void CharacterCreator::appearance(por::CharacterAppearance value)
{
    por::validate_character_appearance(value);
    appearance_ = std::move(value);
}

CharacterSheet CharacterCreator::sheet() const
{
    // The name is entered on the first page, so only that page may lack one.
    return rules_->evaluate(draft_, step_ == CreationStep::race ? rules::NameRequirement::optional
                                                                : rules::NameRequirement::required);
}

Character CharacterCreator::create_character() const
{
    if (step_ != CreationStep::sheet)
    {
        throw std::runtime_error("Finish character creation before exporting the character");
    }
    if (former_)
    {
        check_reincarnation();
        auto body = former_->recreated(*rules_, *module_, draft_);
        body.appearance(appearance_);
        return body;
    }
    if (!training_complete())
    {
        throw std::runtime_error("Complete the required training choices.");
    }
    if (!rules::class_eligible(*rules_, draft_, draft_.character_class))
    {
        throw std::runtime_error("Starting class prerequisites are not met.");
    }
    return Character(*rules_, draft_, appearance_);
}

void CharacterCreator::next()
{
    if (step_ == CreationStep::sheet)
    {
        return;
    }
    // Reincarnate's pages: the species, any Magic Initiate spells, the combat
    // icon, the portrait, the sheet.
    if (former_)
    {
        if (step_ == CreationStep::race && !species_complete())
        {
            throw std::runtime_error("Make the species' choices.");
        }
        if (step_ == CreationStep::spell_choices && !spell_choices_complete())
        {
            throw std::runtime_error("Complete available spell choices.");
        }
        if (step_ == CreationStep::race && has_spell_choices())
        {
            if (!draft_.spells)
            {
                draft_.spells = SpellChoices{};
            }
            step_ = CreationStep::spell_choices;
            return;
        }
        if (step_ == CreationStep::race || step_ == CreationStep::spell_choices)
        {
            check_reincarnation();
            step_ = CreationStep::combat_icon;
            return;
        }
        step_ = static_cast<CreationStep>(static_cast<unsigned>(step_) + 1);
        return;
    }
    if (step_ >= CreationStep::spell_choices && !spell_choices_complete())
    {
        throw std::runtime_error("Complete available spell choices.");
    }
    if (step_ >= CreationStep::training && !training_complete())
    {
        throw std::runtime_error("Complete the required training choices.");
    }
    // The name is entered on the first page; the rules check it fully once
    // the scores they also need are rolled.
    if (draft_.name.empty())
    {
        throw std::runtime_error("Enter a name for your character.");
    }
    if (step_ >= CreationStep::attributes)
    {
        (void)rules_->evaluate(draft_, rules::NameRequirement::required);
    }
    if (step_ >= CreationStep::character_class &&
        !rules::class_eligible(*rules_, draft_, draft_.character_class))
    {
        throw std::runtime_error("Choose a qualified starting class. Requires " +
                                 rules_->class_requirements(draft_.character_class).description +
                                 ".");
    }
    step_ = static_cast<CreationStep>(static_cast<unsigned>(step_) + 1);
    if (step_ == CreationStep::spell_choices && !has_spell_choices())
    {
        step_ = CreationStep::combat_icon;
    }
    if (step_ == CreationStep::spell_choices && !draft_.spells)
    {
        draft_.spells = SpellChoices{};
    }
}

void CharacterCreator::back()
{
    if (former_ && (step_ == CreationStep::combat_icon || step_ == CreationStep::spell_choices))
    {
        step_ = step_ == CreationStep::combat_icon && has_spell_choices()
                    ? CreationStep::spell_choices
                    : CreationStep::race;
        return;
    }
    if (step_ != CreationStep::race)
    {
        step_ = static_cast<CreationStep>(static_cast<unsigned>(step_) - 1);
    }
    if (step_ == CreationStep::spell_choices && !has_spell_choices())
    {
        step_ = CreationStep::training;
    }
}
} // namespace opengold
