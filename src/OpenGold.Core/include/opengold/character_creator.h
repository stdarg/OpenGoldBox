#ifndef OPENGOLD_CHARACTER_CREATOR_H
#define OPENGOLD_CHARACTER_CREATOR_H
#include "opengold/character.h"

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
    combat_icon,
    portrait,
    sheet
};

// One option of a choice group switched on or off. Group and option are both
// strings, so they are named fields rather than adjacent parameters
// (Effective C++ Item 18).
struct ChoiceToggle
{
    std::string_view group, option;
    bool selected{};
};

class CharacterCreator
{
  public:
    CharacterCreator(std::unique_ptr<rules::CharacterRules> rules, std::uint64_t seed);
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
    void training_choice(ChoiceToggle toggle);
    void spell_choice(ChoiceToggle toggle);
    [[nodiscard]] bool has_spell_choices() const;
    [[nodiscard]] bool spell_choices_complete() const;
    void cantrip_choice(std::string_view option, bool selected);
    [[nodiscard]] bool training_complete() const;
    // Whether the species' own choices (an Elf's lineage) are made; the race
    // page asks for them.
    [[nodiscard]] bool species_complete() const;
    [[nodiscard]] bool has_species_choices() const;
    // Rolls the six ability results and, with them, the starting gold.
    void roll();

    // Gold the character joins the party with: 3d6 x 10 gp, the original
    // game's 30 to 180 gp for buying equipment. Zero until the first roll.
    [[nodiscard]] std::uint16_t starting_gold() const
    {
        return starting_gold_;
    }

    void assign_roll(unsigned roll, rules::Ability ability);
    [[nodiscard]] bool scores_assigned() const;
    void swap_scores(rules::Ability first, rules::Ability second);
    void name(std::string text);
    void appearance(por::CharacterAppearance value);
    void next();
    void back();
    void restart();

  private:
    std::unique_ptr<rules::CharacterRules> rules_;
    RandomState random_;
    // Gold has its own stream so the seeded ability rolls are the same with it.
    RandomState gold_random_;
    rules::CharacterDraft draft_;
    por::CharacterAppearance appearance_;
    CreationStep step_{CreationStep::race};
    std::uint16_t starting_gold_{};
    void require_editable() const;
    void prune_training(rules::CharacterDraft &candidate) const;
};
} // namespace opengold
#endif
