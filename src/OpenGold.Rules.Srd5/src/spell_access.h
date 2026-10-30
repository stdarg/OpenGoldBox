#ifndef OPENGOLD_SRD5_SPELL_ACCESS_H
#define OPENGOLD_SRD5_SPELL_ACCESS_H
#include "character_class.h"
#include "opengold/character_rules.h"

namespace opengold::srd5::detail
{
bool is_spell_grant(const rules::FeatureGrant &);
// A grant a Magic Initiate feat makes: one of its spells or its spellcasting ability.
bool is_initiate_grant(const rules::FeatureGrant &);
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
// Spells a class feature or Magic Initiate keeps prepared at this level,
// outside the prepared count.
// The Circle Spells of a land a Druid of the level has (Circle of the Land).
std::vector<std::string> circle_spells(unsigned level, std::string_view land);
std::vector<std::string> always_prepared_spells(CharacterClass klass, unsigned level,
                                                std::span<const rules::FeatureGrant> grants);
// Wizards prepare from a spellbook; Clerics and Paladins from their class list.
bool prepares_spells(CharacterClass klass);
// The highest spell slot level a class has at a character level; 0 for a class
// that does not prepare spells.
unsigned highest_slot_level(CharacterClass klass, unsigned level);
// Whether a spell is on a class's spell list in the game.
bool on_class_list(CharacterClass klass, std::string_view spell);
// The grant a Wizard of `level` gains by copying `spell` from a scroll into its
// spellbook (MAGIC-6).
rules::FeatureGrant copied_spell_grant(std::string_view spell, unsigned level);
std::vector<std::string> casting_ids(const rules::SpellAccess &);

// One Magic Initiate feat's spells and the ability it casts them with.
struct InitiateSpells
{
    std::string feat;
    rules::Ability ability{rules::Ability::intelligence}; // until one is chosen
    std::vector<std::string> spells;                      // its cantrips and level-one spell
    std::string free_spell; // the level-one spell, cast once per Long Rest without a slot
};

std::vector<InitiateSpells> magic_initiate_spells(std::span<const rules::FeatureGrant>);
// An Elf's or a Tiefling's lineage spells at a character level (SRD 5.2.1
// pp. 84, 86): its cantrip, and from levels 3 and 5 a spell each, cast once
// per Long Rest without a slot. `feat` names the species; the ability is the one chosen.
std::vector<InitiateSpells> lineage_spells(std::span<const rules::FeatureGrant>, unsigned level);
// Gaining a level lets a Bard, Cleric, Druid, Sorcerer or Warlock replace one
// cantrip of its class, and a Paladin or Ranger one of its Fighting Style's
// (SRD 5.2.1). Offered for the sheet before the level is gained.
rules::SwapOffer cantrip_swap_offer(const rules::CharacterSheet &);
// A Bard, Sorcerer or Warlock may replace one spell on its list with another
// of its class for which it has slots at the new level.
rules::SwapOffer spell_swap_offer(const rules::CharacterSheet &);
// Apply a swap from the offer to the sheet; throws unless the offer has it.
void apply_cantrip_swap(rules::CharacterSheet &, const rules::LevelSwap &,
                        const rules::SwapOffer &);
void apply_spell_swap(rules::CharacterSheet &, const rules::LevelSwap &, const rules::SwapOffer &);
// A Lore Bard may replace one Magical Discoveries spell with another Cleric,
// Druid or Wizard cantrip or spell of a level it has slots for at the new level.
rules::SwapOffer discovery_swap_offer(const rules::CharacterSheet &);
void apply_discovery_swap(rules::CharacterSheet &, const rules::LevelSwap &,
                          const rules::SwapOffer &);
// A level-four feat entry for each list, available unless that list's feat is held.
std::vector<rules::AdvancementOption>
magic_initiate_feats(std::span<const rules::FeatureGrant> grants);
} // namespace opengold::srd5::detail
#endif
