#ifndef OPENGOLD_SRD5_CHARACTER_CLASS_H
#define OPENGOLD_SRD5_CHARACTER_CLASS_H
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

// The class's Hit Point Die: d12 for a Barbarian down to d6 for a Sorcerer or
// Wizard.
[[nodiscard]] int class_hit_die(CharacterClass character_class) noexcept;
// The highest level the class reaches: 15, the game's cap, for every class
// (LEVELS-2).
[[nodiscard]] unsigned level_cap(CharacterClass character_class);

// Whether the class gains an Ability Score Improvement, or another feat, on
// reaching the level: 4, 8, 12, 16 and 19 for every class, 6 and 14 for a
// Fighter too, 10 for a Rogue too (SRD 5.2.1 class tables).
[[nodiscard]] bool gains_feat(CharacterClass character_class, unsigned level);

// How many feats the class has gained by the level.
[[nodiscard]] unsigned feats_by(CharacterClass character_class, unsigned level);

// The levels a character holds in each class (MULTI-1); a single-class
// character holds them all in one.
class ClassLevels
{
  public:
    ClassLevels() = default;
    // Each level's class, by stable ID, as a sheet records them. Throws for
    // an ID no class has.
    explicit ClassLevels(std::span<const std::string> level_classes);

    [[nodiscard]] int operator[](CharacterClass character_class) const noexcept
    {
        return levels_[static_cast<std::size_t>(character_class)];
    }

    void add(CharacterClass character_class) noexcept
    {
        ++levels_[static_cast<std::size_t>(character_class)];
    }

    [[nodiscard]] int total() const noexcept;
    // The classes with a level, in class order.
    [[nodiscard]] std::size_t count() const noexcept;

  private:
    std::array<int, 12> levels_{};
};

// The class taken at each of a character's levels (MULTI-1), for rules that
// ask when a class reached one of its levels.
class ClassHistory
{
  public:
    // Each level's class, by stable ID. Throws for an ID no class has, or
    // for no level at all.
    explicit ClassHistory(std::span<const std::string> level_classes);

    [[nodiscard]] unsigned level() const noexcept
    {
        return static_cast<unsigned>(classes_.size());
    }

    [[nodiscard]] CharacterClass starting() const noexcept
    {
        return classes_.front();
    }

    [[nodiscard]] const ClassLevels &levels() const noexcept
    {
        return levels_;
    }

    // The class taken at character level `level`, and the level in it that
    // made. Throws for a level the character has not reached.
    [[nodiscard]] CharacterClass taken_at(unsigned level) const;
    [[nodiscard]] unsigned class_level_at(unsigned level) const;
    // The character level at which `klass` reached `class_level`; 0 when it
    // has not.
    [[nodiscard]] unsigned reached(CharacterClass klass, unsigned class_level) const noexcept;
    // The levels in `klass` among the character's first `level` levels.
    [[nodiscard]] unsigned class_level_by(CharacterClass klass, unsigned level) const noexcept;

  private:
    std::vector<CharacterClass> classes_;
    ClassLevels levels_;
};

// The levels the sheet holds in each class: its level_classes, or all of its
// level in its starting class when it records none.
[[nodiscard]] ClassLevels class_levels(const rules::CharacterSheet &sheet);
// The class of each of the sheet's levels, by stable ID, the same way.
[[nodiscard]] std::vector<std::string> level_class_ids(const rules::CharacterSheet &sheet);
} // namespace opengold::srd5::detail
#endif
