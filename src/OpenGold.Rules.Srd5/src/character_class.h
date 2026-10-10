#ifndef OPENGOLD_SRD5_CHARACTER_CLASS_H
#define OPENGOLD_SRD5_CHARACTER_CLASS_H
#include <optional>
#include <string_view>

namespace opengold::rules
{
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

// A sheet stores its class's label, as character profiles in combat
// checkpoints do; this is where rules recover the class from it. Throws for a
// label no class has, rather than treating the character as classless.
[[nodiscard]] CharacterClass class_of(const rules::CharacterSheet &sheet);
} // namespace opengold::srd5::detail
#endif
