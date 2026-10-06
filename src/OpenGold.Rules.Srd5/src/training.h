#ifndef OPENGOLD_SRD5_TRAINING_H
#define OPENGOLD_SRD5_TRAINING_H
#include "opengold/character_rules.h"

namespace opengold::srd5::detail
{
rules::TrainingChoiceGroup scholar_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup primal_knowledge_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup metamagic_options();
rules::TrainingChoiceGroup lore_options(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoiceGroup land_options();
rules::TrainingChoiceGroup invocation_options(unsigned level,
        std::span<const rules::FeatureGrant> grants, std::string_view source);
rules::TrainingChoiceGroup skilled_options(std::span<const rules::FeatureGrant> grants);
std::vector<rules::TrainingChoiceGroup> training_options(const rules::CharacterDraft &draft);
std::vector<rules::FeatureGrant>
training_grants(std::string_view klass, std::string_view background,
                const rules::TrainingChoices &choices);
bool is_training_grant(const rules::FeatureGrant &grant);
std::vector<rules::FeatureGrant> without_training(std::span<const rules::FeatureGrant> grants);
rules::TrainingChoices training_choices(std::span<const rules::FeatureGrant> grants,
                                        std::string_view klass, std::string_view background);
rules::TrainingProfile training_profile(std::span<const rules::FeatureGrant> grants,
                                        std::string_view klass, std::string_view background,
                                        unsigned level, const std::array<int, 6> &scores);
rules::AbilityCheckModifier ability_check(std::span<const rules::FeatureGrant> grants,
        std::string_view klass, std::string_view background,
        unsigned level, const std::array<int, 6> &scores,
        unsigned ability, std::string_view skill);
} // namespace opengold::srd5::detail
#endif
