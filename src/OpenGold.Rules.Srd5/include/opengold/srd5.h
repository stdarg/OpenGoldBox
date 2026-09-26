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
[[nodiscard]] int ability_modifier(int score) noexcept;
// Ordinary saving throws: 1 means every roll saves; 21 means no d20 roll saves.
[[nodiscard]] int minimum_save_roll(int difficulty_class, int bonus) noexcept;
[[nodiscard]] bool attack_hits(int natural_roll, int bonus, int armor_class) noexcept;
} // namespace opengold::srd5
#endif
