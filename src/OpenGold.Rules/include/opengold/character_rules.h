#ifndef OPENGOLD_CHARACTER_RULES_H
#define OPENGOLD_CHARACTER_RULES_H
#include "opengold/rules.h"
#include "opengold/message.h"
#include <array>
#include <map>

namespace opengold::rules
{
enum class CreationField
{
    race,
    gender,
    character_class,
    alignment,
    background
};

struct AbilityRoll
{
    std::array<int, 4> dice{};
    unsigned discarded{};
    [[nodiscard]] int total() const;
    bool operator==(const AbilityRoll &) const = default;
};

struct ScoreAdjustment
{
    std::string label;
    std::array<int, 6> bonuses{};
};

// Derived from creation and advancement choices, with stable source identity.
// Labels are presentation only; acquisition level distinguishes repeated grants.
struct AbilityAdjustment
{
    std::string source_id, label;
    unsigned level{};
    std::array<int, 6> bonuses{};
    Message label_message;
};

// Stable rules IDs, never translated labels. A source and acquisition level
// identify an entitlement; choices retain the selections made for that grant.
struct FeatureGrant
{
    std::string id, source_id;
    unsigned level{};
    std::map<std::string, std::string> choices;
    bool operator==(const FeatureGrant &) const = default;
};

struct SkillTraining
{
    std::string id, label;
    unsigned ability{};
    int bonus{};
    bool proficient{}, expertise{};
    std::vector<FeatureGrant> sources;
};

struct TrainingEntry
{
    std::string id, label;
    std::vector<FeatureGrant> sources;
};

struct TrainingProfile
{
    bool complete{};
    std::vector<SkillTraining> skills;
    std::vector<TrainingEntry> tools, languages;
};

struct AbilityCheckModifier
{
    int ability_modifier{}, proficiency{}, total{};
    bool expertise{}, tool_advantage{};
    std::vector<FeatureGrant> sources;
    bool advantage{};    // All sourced advantage, including tool advantage.
    bool disadvantage{}; // Independent of tool_advantage; both cancel on a roll.
};

struct ClassRequirements
{
    std::vector<unsigned> abilities;
    bool any{};
    int minimum{13};
    std::string description;
};

struct CharacterDraft
{
    std::string race, gender, character_class, alignment, background, name;
    std::vector<std::string> target_classes; // Future intentions, not acquired class levels.
    std::array<AbilityRoll, 6> rolls{};
    // Each assigned ability owns a unique result; 6 means not yet assigned.
    std::array<unsigned, 6> assignment{0, 1, 2, 3, 4, 5};
    unsigned adjustment{};
    bool rolled{};
    TrainingChoices training;
    // Missing means the historical creation preset. An explicit empty choice
    // means no cantrips selected yet, never an instruction to refill the preset.
    std::optional<std::vector<std::string>> cantrips;
    std::optional<SpellChoices> spells;
};

struct CharacterSheet
{
    Identity identity;
    std::string name, race, gender, character_class, alignment, background;
    std::array<int, 6> base{}, bonuses{}, scores{},
        modifiers{}; // bonuses is the sum of all sources.
    int level{1}, hit_die{}, hit_points{};
    std::string hp_explanation;
    std::array<int, 6> saving_throws{};
    std::array<bool, 6> save_proficiencies{};
    std::string racial_modifiers, class_modifiers, background_modifiers;
    // Derived presentation messages; not character identity or save-file keys.
    std::vector<Message> hp_messages, racial_messages, class_messages, background_messages;
    std::vector<FeatureGrant> grants;
    TrainingProfile training;
    std::vector<std::string> prepared_spells;
    // Constitution modifier after each attained level, rebuilt from choices.
    // Keeps minimum-one HP gains separate from retroactive modifier changes.
    std::vector<int> hit_point_modifiers;
    std::vector<AbilityAdjustment> ability_adjustments;
};

// Creation is a separate optional capability: campaign and Godot code do not
// embed edition-specific tables, rolling policies, or HP arithmetic.
class CharacterRules
{
  public:
    virtual ~CharacterRules() = default;
    [[nodiscard]] virtual Identity identity() const = 0;
    [[nodiscard]] virtual std::vector<CreationChoice> choices(CreationField field) const = 0;
    [[nodiscard]] virtual std::vector<ScoreAdjustment>
    adjustments(std::string_view background) const = 0;
    [[nodiscard]] virtual std::array<AbilityRoll, 6> roll(std::uint64_t &random_state) const = 0;
    [[nodiscard]] virtual std::optional<int> ability_score(const CharacterDraft &draft,
                                                           unsigned ability) const = 0;

    [[nodiscard]] virtual std::array<unsigned, 6> preset_ability_priority(std::string_view,
                                                                          unsigned) const
    {
        return {0, 1, 2, 3, 4, 5};
    }

    [[nodiscard]] virtual ClassRequirements class_requirements(std::string_view id) const = 0;
    [[nodiscard]] bool class_eligible(const CharacterDraft &draft, std::string_view id) const;
    [[nodiscard]] std::array<bool, 6> unmet_targets(const CharacterDraft &draft) const;
    [[nodiscard]] virtual CharacterSheet evaluate(const CharacterDraft &draft,
                                                  bool require_name) const = 0;

    [[nodiscard]] virtual std::vector<TrainingChoiceGroup>
    training_options(const CharacterDraft &) const
    {
        return {};
    }

    [[nodiscard]] virtual SpellChoiceOptions spell_choice_options(const CharacterDraft &) const
    {
        return {};
    }

    [[nodiscard]] virtual TrainingChoiceGroup cantrip_options(const CharacterDraft &) const
    {
        return {};
    }

    [[nodiscard]] virtual AbilityCheckModifier ability_check(const CharacterSheet &,
                                                             unsigned ability,
                                                             std::string_view skill = {},
                                                             std::string_view tool = {}) const;
};
} // namespace opengold::rules
#endif
