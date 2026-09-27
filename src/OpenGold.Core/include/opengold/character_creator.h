#ifndef OPENGOLD_CHARACTER_CREATOR_H
#define OPENGOLD_CHARACTER_CREATOR_H
#include "opengold/character.h"
#include <functional>

namespace opengold
{
enum class CreationStep
{
    race,
    alignment,
    attributes,
    character_class,
    training,
    spell_choices,
    name,
    combat_icon,
    sheet
};

class CharacterCreator
{
  public:
    CharacterCreator(std::unique_ptr<rules::CharacterRules> rules, std::uint64_t seed);
    // Isolated training editor: existing choices are locked; live characters are unchanged.
    CharacterCreator(std::unique_ptr<rules::CharacterRules> rules, rules::CharacterDraft draft);
    CharacterCreator(std::unique_ptr<rules::CharacterRules> rules, Character character,
                     const rules::RulesModule &module);
    [[nodiscard]] std::vector<rules::TrainingChoiceGroup> training_options() const;

    [[nodiscard]] const rules::CharacterRules &rules() const
    {
        return *rules_;
    }

    [[nodiscard]] const rules::CharacterDraft &draft() const
    {
        return draft_;
    }

    [[nodiscard]] const por::CharacterAppearance &appearance() const
    {
        return appearance_;
    }

    [[nodiscard]] CreationStep step() const
    {
        return step_;
    }

    [[nodiscard]] rules::CharacterSheet sheet() const;
    [[nodiscard]] Character create_character() const;
    void select(rules::CreationField field, std::string_view id);
    void target_class(std::string_view id, bool selected);
    void select_adjustment(unsigned index);
    void training_choice(std::string_view group, std::string_view option, bool selected);
    void spell_choice(std::string_view group, std::string_view option, bool selected);
    [[nodiscard]] bool spell_choices_complete() const;
    void cantrip_choice(std::string_view option, bool selected);
    [[nodiscard]] bool training_complete() const;
    void roll();
    void assign_roll(unsigned roll, unsigned ability);
    [[nodiscard]] bool scores_assigned() const;
    void swap_scores(unsigned first, unsigned second);
    void name(std::string text);
    void appearance(por::CharacterAppearance value);
    void next();
    void back();
    void restart();

  private:
    std::unique_ptr<rules::CharacterRules> rules_;
    std::uint64_t random_;
    rules::CharacterDraft draft_;
    rules::TrainingChoices locked_training_;
    std::optional<Character> training_character_;
    std::optional<std::reference_wrapper<const rules::RulesModule>> training_module_;
    por::CharacterAppearance appearance_;
    CreationStep step_{CreationStep::race};
    void require_editable() const;
    void prune_training(rules::CharacterDraft &candidate) const;
};
} // namespace opengold
#endif
