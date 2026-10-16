#ifndef OPENGOLD_SRD5_WEAPON_MASTERY_H
#define OPENGOLD_SRD5_WEAPON_MASTERY_H
#include "opengold/character_rules.h"
#include <array>
#include "weapons.h"

namespace opengold::srd5::detail
{
std::string_view mastery_name(Mastery mastery);
bool is_mastery_grant(const rules::FeatureGrant &grant);
rules::TrainingChoiceGroup mastery_options(CharacterClass klass, unsigned acquired_level,
                                           std::span<const rules::FeatureGrant> grants = {});
rules::TrainingChoices mastery_choices(std::span<const rules::FeatureGrant> grants,
                                       CharacterClass klass, unsigned level);
unsigned mastery_replacements(CharacterClass klass);
// The training group id of the kinds learned at a level, without building its options.
std::string mastery_source(CharacterClass klass, unsigned acquired_level);
// The levels at which a class learns more Weapon Mastery kinds: 1, then one
// more at 4 and at 10 for a Fighter or a Barbarian.
inline constexpr std::array<unsigned, 3> mastery_levels{1, 4, 10};
// A completed rest supplies the authorization ticket in Core. This pure rule
// operation validates the new complete set and retains each entitlement's source.
std::vector<rules::FeatureGrant> replace_masteries(std::span<const rules::FeatureGrant> grants,
                                                   CharacterClass klass, unsigned level,
                                                   std::span<const std::string> selected);
} // namespace opengold::srd5::detail
#endif
