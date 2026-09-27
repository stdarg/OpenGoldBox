#ifndef OPENGOLD_SRD5_WEAPON_MASTERY_H
#define OPENGOLD_SRD5_WEAPON_MASTERY_H
#include "opengold/character_rules.h"
#include "weapons.h"

namespace opengold::srd5::detail
{
std::string_view mastery_name(Mastery mastery);
bool is_mastery_grant(const rules::FeatureGrant &grant);
rules::TrainingChoiceGroup mastery_options(std::string_view klass, unsigned acquired_level,
                                           std::span<const rules::FeatureGrant> grants = {});
rules::TrainingChoices mastery_choices(std::span<const rules::FeatureGrant> grants,
                                       std::string_view klass, unsigned level);
unsigned mastery_replacements(std::string_view klass);
// A completed rest supplies the authorization ticket in Core. This pure rule
// operation validates the new complete set and retains each entitlement's source.
std::vector<rules::FeatureGrant> replace_masteries(std::span<const rules::FeatureGrant> grants,
                                                   std::string_view klass, unsigned level,
                                                   std::span<const std::string> selected);
} // namespace opengold::srd5::detail
#endif
