#ifndef OPENGOLD_CHARACTER_H
#define OPENGOLD_CHARACTER_H
#include "opengold/character_art.h"
#include "opengold/character_rules.h"
#include "opengold/inventory.h"
#include "opengold/rules.h"
#include <span>

namespace opengold
{
// A finished character owns its data. It does not borrow from the creator,
// rules module, art archives or Godot, and can be copied into a roster later.
struct SpellChoiceEdit
{
    unsigned level{};
    std::uint64_t rest_session{};
    rules::SpellChoices choices;
    bool operator==(const SpellChoiceEdit &) const = default;
};

struct TrainingChoiceEdit
{
    unsigned level{};
    std::uint64_t rest_session{};
    std::vector<std::string> selections;
    bool operator==(const TrainingChoiceEdit &) const = default;
};

// A skill or Expertise ("skill:stealth") learned at a training hall at a level.
struct HallTraining
{
    unsigned level{};
    std::string grant;
    bool operator==(const HallTraining &) const = default;
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
    // A new name, as a simulacrum takes: creation and the sheet both carry it,
    // so a saved character rebuilds the same.
    void rename(std::string name);
    // This character built on another creation draft, such as the species of
    // a body Reincarnate forms, with its level-ups, rest choices and hall
    // training made again. Throws when one of them no longer fits.
    [[nodiscard]] Character recreated(const rules::CharacterRules &creation,
                                      const rules::RulesModule &rules,
                                      rules::CharacterDraft draft) const;
    // Makes a level-1 character's saved history again: each level's rest
    // choices and hall training, then the next level-up. Throws on one that
    // does not fit.
    void replay(const rules::RulesModule &rules, std::span<const rules::AdvancementChoice> history,
                std::span<const SpellChoiceEdit> spell_edits,
                std::span<const TrainingChoiceEdit> training_edits,
                std::span<const HallTraining> hall_training);

    [[nodiscard]] const Inventory &inventory() const
    {
        return inventory_;
    }

    // The inventory changes only through these calls rather than through a
    // writable handle, so each change is visible at its call site (Effective
    // C++ Item 28).
    std::uint64_t add_item(NewItem item)
    {
        return inventory_.add(std::move(item));
    }

    void remove_item(std::uint64_t id, std::uint32_t quantity = 1)
    {
        inventory_.remove(id, quantity);
    }

    void replace_inventory(Inventory inventory)
    {
        inventory_ = std::move(inventory);
    }

    bool advance(const rules::RulesModule &rules, rules::VitalState &state);
    bool advance(const rules::RulesModule &rules, rules::VitalState &state,
                 const rules::AdvancementChoice &choice);
    // A Short Rest's choices (Memorize Spell) change less than a Long Rest's;
    // either replays as a Long Rest's, which allows more.
    void choose_spells(const rules::RulesModule &, const rules::SpellChoices &,
                       std::uint64_t rest_session,
                       rules::SpellChoiceContext context = rules::SpellChoiceContext::long_rest,
                       rules::ChoiceCompleteness completeness = rules::ChoiceCompleteness::complete);
    void replace_rest_training(const rules::RulesModule &, std::span<const std::string>,
                               std::uint64_t rest_session);
    void train_at_hall(const rules::RulesModule &, std::string grant);

    [[nodiscard]] const auto &hall_training() const
    {
        return hall_training_;
    }

    [[nodiscard]] const auto &training_edits() const
    {
        return training_edits_;
    }

    [[nodiscard]] const auto &spell_edits() const
    {
        return spell_edits_;
    }

    [[nodiscard]] const auto &advancements() const
    {
        return advancements_;
    }


  private:
    rules::CharacterDraft creation_;
    rules::CharacterSheet sheet_;
    por::CharacterAppearance appearance_;
    Inventory inventory_;
    std::vector<rules::AdvancementChoice> advancements_;
    std::vector<SpellChoiceEdit> spell_edits_;
    std::vector<TrainingChoiceEdit> training_edits_;
    std::vector<HallTraining> hall_training_;
};
} // namespace opengold
#endif
