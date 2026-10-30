#ifndef OPENGOLD_SRD5_CHARACTER_CLASS_H
#define OPENGOLD_SRD5_CHARACTER_CLASS_H
#include <optional>
#include <string_view>

namespace opengold::rules
{
struct CharacterDraft;
struct CharacterSheet;
} // namespace opengold::rules

namespace opengold::srd5::detail
{
// Rules compare a class, never its label: a misspelt class does not compile,
// and relabelling one cannot silently strip its features (Effective C++ Item 18).
// Alphabetical, the order players see.
enum class CharacterClass
{
    barbarian,
    bard,
    cleric,
    druid,
    fighter,
    monk,
    paladin,
    ranger,
    rogue,
    sorcerer,
    warlock,
    wizard
};

// The stable ID a draft selects, which is also the class's grant source ID.
[[nodiscard]] std::string_view class_id(CharacterClass character_class);

// The label players read; presentation only.
[[nodiscard]] std::string_view class_label(CharacterClass character_class);

[[nodiscard]] std::optional<CharacterClass> class_from_label(std::string_view label);

// The class a stable ID names. Throws for an ID no class has, so a misspelt
// ID cannot quietly match no class's rules.
[[nodiscard]] CharacterClass class_from_id(std::string_view id);

// The class a draft selects, or none while the player has not chosen one yet.
// Throws for an ID no class has.
[[nodiscard]] std::optional<CharacterClass> draft_class(const rules::CharacterDraft &draft);

// A sheet stores its class's label, as character profiles in combat
// checkpoints do; this is where rules recover the class from it. Throws for a
// label no class has, rather than treating the character as classless.
[[nodiscard]] CharacterClass class_of(const rules::CharacterSheet &sheet);

// The highest level the class reaches: 15, the game's cap, for every class
// (LEVELS-2).
[[nodiscard]] unsigned level_cap(CharacterClass character_class);

// Whether the class gains an Ability Score Improvement, or another feat, on
// reaching the level: 4, 8, 12, 16 and 19 for every class, 6 and 14 for a
// Fighter too, 10 for a Rogue too (SRD 5.2.1 class tables).
[[nodiscard]] bool gains_feat(CharacterClass character_class, unsigned level);

// How many feats the class has gained by the level.
[[nodiscard]] unsigned feats_by(CharacterClass character_class, unsigned level);
} // namespace opengold::srd5::detail
#endif
