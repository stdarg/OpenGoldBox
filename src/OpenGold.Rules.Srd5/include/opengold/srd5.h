#ifndef OPENGOLD_SRD5_H
#define OPENGOLD_SRD5_H
#include "opengold/rules.h"
#include "opengold/character_rules.h"
#include <filesystem>

namespace opengold::srd5
{
// Reads a pinned, curated content pack. Unknown definitions/mechanics fail.
[[nodiscard]] std::unique_ptr<rules::RulesModule> load(const std::filesystem::path &content_pack);
// The returned module owns its parsed content; the input may be discarded.
[[nodiscard]] std::unique_ptr<rules::RulesModule> parse_content(std::string_view content);
[[nodiscard]] std::unique_ptr<rules::CharacterRules> character_rules();
// Pure SRD arithmetic, also used by the deterministic combat resolver.
[[nodiscard]] std::string equipment_note(const rules::CharacterSheet &sheet, std::string_view item);
// What one charged item's quantity counts ("charges", "missiles"); empty when the
// quantity counts items (MAGIC-3).
[[nodiscard]] std::string_view charge_noun(std::string_view item) noexcept;
// A spell's English label, such as "Fireball"; empty for an unknown id.
[[nodiscard]] std::string_view spell_label(std::string_view spell) noexcept;
// Whether the inventory's Use works for the item outside combat: the Manual of
// Bodily Health, a healing or Giant Strength potion (MAGIC-3), or a Wand of
// Secrets (SECRET-1).
[[nodiscard]] bool usable_outside_combat(std::string_view item) noexcept;
[[nodiscard]] int ability_modifier(int score) noexcept;
// Ordinary saving throws: 1 means every roll saves; 21 means no d20 roll saves.
[[nodiscard]] int minimum_save_roll(int difficulty_class, int bonus) noexcept;
[[nodiscard]] bool attack_hits(int natural_roll, int bonus, int armor_class) noexcept;
} // namespace opengold::srd5
#endif
