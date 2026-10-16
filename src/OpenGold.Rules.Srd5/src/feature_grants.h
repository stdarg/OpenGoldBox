#ifndef OPENGOLD_SRD5_FEATURE_GRANTS_H
#define OPENGOLD_SRD5_FEATURE_GRANTS_H
#include "character_class.h"
#include "opengold/character_rules.h"
#include <iosfwd>

namespace opengold::srd5::detail
{
// The module's creation rules, which hold no state, made once; and a
// creation field's choices, listed once (labels, IDs and descriptions).
const rules::CharacterRules &creation_rules();
const std::vector<rules::CreationChoice> &creation_choices(rules::CreationField field);
// A sheet stores its race's and background's labels; these recover the
// stable IDs that grants are sourced from. Each throws for a label no choice
// has, rather than deriving an ID from the label's spelling.
std::string race_id(std::string_view label);
std::string background_id(std::string_view label);
// The Origin feat a background gives ("feat:alert"), or empty.
std::string background_feat(std::string_view background);
std::vector<rules::FeatureGrant> starting_grants(CharacterClass klass, std::string_view race,
        std::string_view background);
rules::FeatureGrant advancement_grant(CharacterClass klass, unsigned level,
                                      const rules::AdvancementChoice &choice);
std::vector<rules::AdvancementOption> fighting_styles();
// The class and subclass features gained on reaching `level`, from level 5 on
// (SRD 5.2.1 class tables); choices such as feats are granted separately.
std::vector<rules::FeatureGrant> higher_level_features(CharacterClass klass, unsigned level);
// The Champion's Additional Fighting Style (level 7): its feat's source.
inline constexpr std::string_view champion_style = "subclass:fighter:champion:fighting_style";
// The Hunter's Defensive Tactics (Ranger level 7): its option's source.
inline constexpr std::string_view defensive_tactics = "subclass:ranger:hunter:defensive_tactics";
// The Cleric's Blessed Strikes (level 7): its option's source.
inline constexpr std::string_view blessed_strikes = "class:cleric:blessed_strikes";
// The Draconic Sorcerer's Elemental Affinity (level 6): its type's source.
inline constexpr std::string_view elemental_affinity = "subclass:sorcerer:elemental_affinity";
// The Fiend Warlock's Fiendish Resilience (level 10): its type's source.
inline constexpr std::string_view fiendish_resilience = "subclass:warlock:fiendish_resilience";
// The Druid's Elemental Fury (level 7): its option's source.
inline constexpr std::string_view elemental_fury = "class:druid:elemental_fury";
bool has_grant(std::span<const rules::FeatureGrant> grants, std::string_view id);

struct GrantEffects
{
    unsigned
    feats{}; // Internal combat mask: Defense, Savage Attacker, Archery, Great Weapon Fighting.
    rules::AbilityArray<int> abilities{};
};

GrantEffects validate_grants(std::span<const rules::FeatureGrant> grants, CharacterClass klass,
                             std::string_view race, std::string_view background, unsigned level);
void write_grants(std::ostream &out, std::span<const rules::FeatureGrant> grants);
std::vector<rules::FeatureGrant> read_grants(std::istream &in);
} // namespace opengold::srd5::detail
#endif
