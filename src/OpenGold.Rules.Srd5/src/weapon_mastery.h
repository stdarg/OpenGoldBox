#ifndef OPENGOLD_SRD5_WEAPON_MASTERY_H
#define OPENGOLD_SRD5_WEAPON_MASTERY_H
#include "opengold/character_rules.h"
#include <array>
#include "character_class.h"
#include "weapons.h"

namespace opengold::srd5::detail
{
std::string_view mastery_name(Mastery mastery);
bool is_mastery_grant(const rules::FeatureGrant &grant);
rules::TrainingChoiceGroup mastery_options(CharacterClass klass, unsigned acquired_level,
                                           std::span<const rules::FeatureGrant> grants = {});
// Each held class's Weapon Mastery kinds, by the group they were chosen in
// (MULTI-1). Throws for a kind a class cannot master or has not reached.
rules::TrainingChoices mastery_choices(std::span<const rules::FeatureGrant> grants,
                                       const ClassLevels &levels);
// How many kinds a Long Rest may replace: each class's own number.
unsigned mastery_replacements(const ClassLevels &levels);
// The training group id of the kinds learned at a level, without building its options.
std::string mastery_source(CharacterClass klass, unsigned acquired_level);
// The levels at which a class learns more Weapon Mastery kinds: 1, then one
// more at 4 and at 10 for a Fighter or a Barbarian.
inline constexpr std::array<unsigned, 3> mastery_levels{1, 4, 10};
// A completed rest supplies the authorization ticket in Core. This pure rule
// operation validates the new complete set and retains each entitlement's source.
std::vector<rules::FeatureGrant> replace_masteries(std::span<const rules::FeatureGrant> grants,
                                                   const ClassLevels &levels,
                                                   std::span<const std::string> selected);
} // namespace opengold::srd5::detail
#endif
