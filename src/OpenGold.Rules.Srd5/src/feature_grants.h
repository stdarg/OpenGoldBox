#ifndef OPENGOLD_SRD5_FEATURE_GRANTS_H
#define OPENGOLD_SRD5_FEATURE_GRANTS_H
#include "opengold/character_rules.h"
#include <iosfwd>

namespace opengold::srd5::detail
{
std::string grant_source_id(std::string_view label);
std::vector<rules::FeatureGrant> starting_grants(std::string_view klass, std::string_view race,
                                                 std::string_view background);
rules::FeatureGrant advancement_grant(std::string_view klass, unsigned level,
                                      const rules::AdvancementChoice &choice);
bool has_grant(std::span<const rules::FeatureGrant> grants, std::string_view id);

struct GrantEffects
{
    unsigned feats{}; // Internal combat mask: Defense, Savage Attacker, Archery.
    std::array<int, 6> abilities{};
};

GrantEffects validate_grants(std::span<const rules::FeatureGrant> grants, std::string_view klass,
                             std::string_view race, std::string_view background, unsigned level,
                             bool damage_traits = true, bool rush_trait = true,
                             bool action_surge = true, bool archery = true,
                             bool starting_styles = true, bool tactical_mind = true,
                             bool champion = true, bool arcane_recovery = true,
                             bool rogue_attacks = true);
void write_grants(std::ostream &out, std::span<const rules::FeatureGrant> grants);
std::vector<rules::FeatureGrant> read_grants(std::istream &in);
} // namespace opengold::srd5::detail
#endif
