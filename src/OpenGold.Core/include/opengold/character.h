#ifndef OPENGOLD_CHARACTER_H
#define OPENGOLD_CHARACTER_H
#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include "opengold/inventory.h"
#include "opengold/rules.h"

namespace opengold
{
// A finished character owns its data. It does not borrow from the creator,
// rules module, art archives or Godot, and can be copied into a roster later.
struct SpellChoiceEdit
{
    unsigned level{};
    std::uint64_t rest_session{}; // Zero is a pending-knowledge completion.
    rules::SpellChoices choices;
    bool operator==(const SpellChoiceEdit &) const = default;
};

class Character
{
  public:
    Character(const rules::CharacterRules &rules, rules::CharacterDraft creation,
              por::CharacterAppearance appearance);

    [[nodiscard]] const rules::CharacterDraft &creation_data() const
    {
        return creation_;
    }

    [[nodiscard]] const rules::CharacterSheet &sheet() const
    {
        return sheet_;
    }

    [[nodiscard]] const por::CharacterAppearance &appearance() const
    {
        return appearance_;
    }

    void appearance(por::CharacterAppearance value);

    [[nodiscard]] const Inventory &inventory() const
    {
        return inventory_;
    }

    [[nodiscard]] Inventory &inventory()
    {
        return inventory_;
    }

    bool advance(const rules::RulesModule &rules, rules::VitalState &state);
    bool advance(const rules::RulesModule &rules, rules::VitalState &state,
                 const rules::AdvancementChoice &choice);
    void choose_spells(const rules::RulesModule &, const rules::SpellChoices &,
                       std::uint64_t rest_session = 0, bool require_complete = true);

    [[nodiscard]] const auto &spell_edits() const
    {
        return spell_edits_;
    }

    [[nodiscard]] const auto &advancements() const
    {
        return advancements_;
    }

    // Reconstructs a candidate with missing training filled and the same history.
    // Existing selections cannot be replaced; this does not mutate live vitals.
    [[nodiscard]] Character preview_training(const rules::CharacterRules &creation_rules,
                                             const rules::RulesModule &rules,
                                             const rules::TrainingChoices &choices,
                                             bool require_complete = true) const;
    [[nodiscard]] rules::TrainingChoices training_choices() const;

  private:
    rules::CharacterDraft creation_;
    rules::CharacterSheet sheet_;
    por::CharacterAppearance appearance_;
    Inventory inventory_;
    std::vector<rules::AdvancementChoice> advancements_;
    std::vector<SpellChoiceEdit> spell_edits_;
};
} // namespace opengold
#endif
