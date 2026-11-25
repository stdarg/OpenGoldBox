#ifndef OPENGOLD_SRD5_EQUIPMENT_RULES_H
#define OPENGOLD_SRD5_EQUIPMENT_RULES_H
#include "character_class.h"
#include "opengold/character_rules.h"
#include "opengold/message.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What a piece of equipment is to a character: its label, what its
// enchantment adds, who is trained with it and who reads which scrolls.
namespace opengold::srd5::detail
{
using rules::FeatureGrant;
using rules::Message;

// An enchanted item's label carries its bonus ("Longsword +2"); hosts translate
// the base label and keep the bonus.
[[nodiscard]] std::string weapon_label(std::string_view key);

// A message in its English source, for the English-only summaries.
[[nodiscard]] std::string english(const Message &message);

// What an enchanted item's magic adds (MAGIC-1); nothing until it is
// identified, though its numbers already count.
[[nodiscard]] std::vector<Message> enchantment_messages(std::string_view key);

[[nodiscard]] std::string attack_ability(std::string_view key);

// Magic Lock Picks (SECRET-1, OpenGoldBox's own): a Rogue picking a lock with
// them has Advantage, and one set is used up.
inline constexpr std::string_view magic_lock_picks = "magic_lock_picks";

// A Stone of Good Luck (SECRET-1, SRD 5.2.1) works from the pack: +1 to
// ability checks and saving throws. The campaign adds it to the gear of a
// member who carries one; a second stone adds nothing.
inline constexpr std::string_view stone_of_good_luck = "stone_of_good_luck";

// The classes whose spellcasting reads arcane scrolls, and divine ones.
[[nodiscard]] bool reads_arcane_scrolls(CharacterClass klass);

[[nodiscard]] bool reads_divine_scrolls(CharacterClass klass);

// The training a class gives a character who takes it after the first
// (SRD 5.2.1, each class's "As a Multiclass Character", MULTI-1).
struct MulticlassTraining
{
    bool martial_weapons{}, light_armor{}, medium_armor{}, shields{};
};

[[nodiscard]] MulticlassTraining multiclass_training(CharacterClass klass);

[[nodiscard]] bool trained(CharacterClass klass, std::span<const FeatureGrant> grants,
                           std::string_view key);

// The grant a Fighting Style choice adds: a feat, or the Paladin's Blessed
// Warrior or the Ranger's Druidic Warrior.
[[nodiscard]] std::string fighting_style_grant(std::string_view style);

// A Rogue carrying the kit can poison with Cunning Strike (#233); owning it is
// enough, as tool proficiencies are gone (DM-1).
inline constexpr std::string_view poisoners_kit = "poisoners_kit";

// Basic Poison (SRD 5.2.1): a Bonus Action coats a weapon with a vial, and the
// next Piercing or Slashing damage it deals within a minute adds 1d4 Poison.
// There is no ammunition to coat (AMMO-UNLIMITED).
inline constexpr std::string_view basic_poison = "basic_poison";

// A Rogue carrying Thieves' Tools has twice the chance to pick a lock (LOCK-1);
// the campaign rolls the check.
inline constexpr std::string_view thieves_tools = "thieves_tools";
} // namespace opengold::srd5::detail
#endif
