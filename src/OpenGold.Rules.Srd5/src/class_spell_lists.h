#ifndef OPENGOLD_SRD5_CLASS_SPELL_LISTS_H
#define OPENGOLD_SRD5_CLASS_SPELL_LISTS_H
#include "character_class.h"
#include "opengold/character_rules.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::srd5::detail
{
// The spells a class may legitimately have stored at its class `level`.
[[nodiscard]] std::vector<std::string> allowed_spells(CharacterClass klass, unsigned level);
// A Bard's Magical Discoveries (Lore 6) and Magical Secrets (10): a Cleric,
// Druid or Wizard spell of a level those classes cast at the Bard's level.
[[nodiscard]] bool bard_secret(CharacterClass klass, unsigned level,
                               std::span<const rules::FeatureGrant> grants, std::string_view id);
} // namespace opengold::srd5::detail
#endif
