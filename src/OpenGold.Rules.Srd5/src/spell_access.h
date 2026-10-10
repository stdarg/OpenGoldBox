#ifndef OPENGOLD_SRD5_SPELL_ACCESS_H
#define OPENGOLD_SRD5_SPELL_ACCESS_H
#include "character_class.h"
#include "opengold/character_rules.h"

namespace opengold::srd5::detail
{
bool is_spell_grant(const rules::FeatureGrant &);
std::vector<rules::FeatureGrant> without_spell_grants(std::span<const rules::FeatureGrant>);
std::vector<rules::FeatureGrant>
starting_spell_grants(CharacterClass klass,
                      const std::optional<std::vector<std::string>> &cantrips = {});
rules::TrainingChoiceGroup starting_cantrip_options(CharacterClass klass);
rules::SpellAccess spell_access(std::span<const rules::FeatureGrant>, CharacterClass klass,
                                unsigned level, std::span<const std::string> prepared);
// Existing advancement selections learn any newly selected book spell and
// retain every earlier entry. Copying and preparation controls are separate.
rules::SpellChoiceOptions spell_choice_options(const rules::CharacterSheet &,
        rules::SpellChoiceContext);
void apply_spell_choices(rules::CharacterSheet &, const rules::SpellChoices &,
                         rules::SpellChoiceContext, rules::ChoiceCompleteness);
// Known and prepared spells as ids. Ids rather than a packed mask because an
// int caps the catalog at 31 spells.
std::vector<std::string> known_cantrip_ids(const rules::SpellAccess &);
// Spells a class feature keeps prepared at this level, outside the prepared count.
std::vector<std::string> always_prepared_spells(CharacterClass klass, unsigned level,
        std::span<const rules::FeatureGrant> grants);
// Wizards prepare from a spellbook; Clerics and Paladins from their class list.
bool prepares_spells(CharacterClass klass);
std::vector<std::string> casting_ids(const rules::SpellAccess &);
} // namespace opengold::srd5::detail
#endif
