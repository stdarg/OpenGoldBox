#ifndef OPENGOLD_SRD5_TRAINING_H
#define OPENGOLD_SRD5_TRAINING_H
#include "character_class.h"
#include "opengold/character_rules.h"

namespace opengold::srd5::detail
{
rules::TrainingChoiceGroup scholar_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup primal_knowledge_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup metamagic_options();
rules::TrainingChoiceGroup lore_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup land_options();
rules::TrainingChoiceGroup hunters_prey_options();
rules::TrainingChoiceGroup invocation_options(unsigned level,
        std::span<const rules::FeatureGrant> grants, std::string_view source);
rules::TrainingChoiceGroup skilled_options(std::span<const rules::FeatureGrant> grants);
// The grant source of skills and Expertise a training hall teaches.
constexpr std::string_view hall_training = "training_hall";
// What a hall teaches a member of `classes`: proficiency in a skill on any of
// their lists the character lacks, or Expertise in a skill it is proficient in.
rules::TrainingChoiceGroup hall_training_options(std::span<const rules::FeatureGrant> grants,
        std::span<const CharacterClass> classes);
std::vector<rules::TrainingChoiceGroup> training_options(const rules::CharacterDraft &draft);
std::vector<rules::FeatureGrant>
training_grants(CharacterClass klass, std::string_view background,
                const rules::TrainingChoices &choices);
bool is_training_grant(const rules::FeatureGrant &grant);
std::vector<rules::FeatureGrant> without_training(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoices training_choices(std::span<const rules::FeatureGrant> grants,
                                        CharacterClass klass, std::string_view background);
rules::TrainingProfile training_profile(std::span<const rules::FeatureGrant> grants,
                                        CharacterClass klass, std::string_view background,
                                        unsigned level, const rules::AbilityArray<int> &scores);
rules::AbilityCheckModifier ability_check(std::span<const rules::FeatureGrant> grants,
        CharacterClass klass, std::string_view background,
        unsigned level, const rules::AbilityArray<int> &scores,
        rules::Ability ability, std::string_view skill);
} // namespace opengold::srd5::detail
#endif
