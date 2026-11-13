#ifndef OPENGOLD_CHARACTER_CREATOR_H
#define OPENGOLD_CHARACTER_CREATOR_H
#include "opengold/character.h"
#include <optional>
#include <set>

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

// Reincarnate (LEVELS-5): `former`'s creation draft for a new body of
// `species`. The old species' choices go, with the spells a Human's Versatile
// Magic Initiate taught; a species skill the character built Expertise on is
// kept. The new species' own choices are left to make.
[[nodiscard]] rules::CharacterDraft reincarnation_draft(const Character &former,
                                                        std::string_view species);

// What Reincarnate's pages chose: the new species' "species:" training groups,
// and the spells a Human's Versatile Magic Initiate teaches.
struct ReincarnationChoices
{
    rules::TrainingChoices training, initiate_spells;
};

class CharacterCreator
{
  public:
    CharacterCreator(std::shared_ptr<const rules::CharacterRules> rules, std::uint64_t seed);
    // Reincarnate's pages (LEVELS-5): `former`'s new body of `species`. Only
    // the species' choices, a Human's Versatile Magic Initiate spells and the
    // look change: the species page, those spells, the combat icon and the
    // portrait, then the sheet. `rules` replays the character's levels and
    // must outlive the creator: the game view ends the pages before it
    // replaces the campaign that owns it.
    CharacterCreator(std::shared_ptr<const rules::CharacterRules> creation,
                     const rules::RulesModule &rules, const Character &former,
                     std::string_view species);

    [[nodiscard]] bool reincarnating() const noexcept
    {
        return former_.has_value();
    }

    [[nodiscard]] ReincarnationChoices reincarnation_choices() const;
    // The training choices on offer; while reincarnating, a species' skill or
    // cantrip the character already has from elsewhere is not.
    [[nodiscard]] std::vector<rules::TrainingChoiceGroup> training_options() const;
    // The spell choices on offer; while reincarnating, only Versatile Magic
    // Initiate's, less the spells the character already knows.
    [[nodiscard]] rules::SpellChoiceOptions spell_choice_options() const;
    // The cantrips to choose; none while reincarnating.
    [[nodiscard]] rules::TrainingChoiceGroup cantrip_options() const;

    [[nodiscard]] const rules::CharacterRules &rules() const noexcept
    {
        return *rules_;
    }

    [[nodiscard]] const rules::CharacterDraft &draft() const noexcept
    {
        return draft_;
    }

    [[nodiscard]] const por::CharacterAppearance &appearance() const noexcept
    {
        return appearance_;
    }

    [[nodiscard]] CreationStep step() const noexcept
    {
        return step_;
    }

    // The pages in the order these rules ask them: under the SRD every step,
    // less the spell choices when there are none; a new body's species, any
    // spell choices, its look and its sheet; the original's race, class,
    // alignment, roll, look and sheet.
    [[nodiscard]] std::vector<CreationStep> pages() const;

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
    // Rolls the six ability results and, with them, the starting gold. Rules
    // that roll in place (the original's) roll the character whole: its
    // scores, Hit Points, age and gold, replacing any earlier roll.
    void roll();

    // Gold the character joins the party with: under the SRD 3d6 x 10 gp, the
    // original game's 30 to 180 gp for buying equipment; under the original
    // rules what its classes rolled. Zero until the first roll.
    [[nodiscard]] std::uint16_t starting_gold() const noexcept
    {
        return starting_gold_;
    }

    // Platinum it joins with besides: Curse's 300 under the original rules.
    [[nodiscard]] std::uint16_t starting_platinum() const noexcept
    {
        return starting_platinum_;
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
    std::shared_ptr<const rules::CharacterRules> rules_;
    // While reincarnating: the character, its rules module (borrowed) and
    // the grants it has from anything but its species.
    std::optional<Character> former_;
    const rules::RulesModule *module_{};
    std::set<std::string, std::less<>> held_;
    [[nodiscard]] std::string initiate_group_prefix() const;
    void fill_species(rules::CharacterDraft &candidate, const rules::CharacterDraft &before,
                      std::string_view touched) const;
    void prune_spells(rules::CharacterDraft &candidate) const;
    void set_aside_vanished(const rules::CharacterDraft &candidate);
    void check_reincarnation() const;
    RandomState random_;
    // Gold has its own stream so the seeded ability rolls are the same with it.
    RandomState gold_random_;
    rules::CharacterDraft draft_;
    // The picks of species choices that went away (a High Elf's cantrip when
    // the lineage changes, Skilled's skills when Versatile does), for when
    // they come back.
    rules::TrainingChoices set_aside_;
    por::CharacterAppearance appearance_;
    CreationStep step_{CreationStep::race};
    std::uint16_t starting_gold_{};
    std::uint16_t starting_platinum_{};
    void require_editable() const;
    void prune_training(rules::CharacterDraft &candidate) const;
    // Rules that roll in place have their own pages (pages_in_place()).
    [[nodiscard]] bool rolls_in_place() const;
    // Each later choice the earlier ones no longer allow becomes the first one
    // they do (the original limits classes by race, alignments by class).
    void keep_allowed(rules::CharacterDraft &candidate) const;
    void next_in_place();
    void back_in_place();
};
} // namespace opengold
#endif
