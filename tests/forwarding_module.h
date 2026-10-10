#ifndef OPENGOLD_TEST_FORWARDING_MODULE_H
#define OPENGOLD_TEST_FORWARDING_MODULE_H
#include "opengold/character_rules.h"
#include "opengold/rules.h"
#include <memory>
#include <utility>

namespace opengold::test
{
// A test double that changes a few rules derives from this and overrides only
// those; every other operation goes to the wrapped module. RulesModule has no
// default implementations, so the forwarding is spelled out here once
// (Effective C++ Item 34).
class ForwardingModule : public rules::RulesModule
{
  public:
    explicit ForwardingModule(std::unique_ptr<rules::RulesModule> rules)
        : rules_(std::move(rules))
    {
    }

    rules::Identity identity() const override
    {
        return rules_->identity();
    }

    std::vector<std::string> supported_features() const override
    {
        return rules_->supported_features();
    }

    std::unique_ptr<rules::CombatSession> create(rules::Encounter encounter,
            std::uint64_t seed) const override
    {
        return rules_->create(std::move(encounter), seed);
    }

    std::unique_ptr<rules::CombatSession> restore(std::string_view checkpoint) const override
    {
        return rules_->restore(checkpoint);
    }

    rules::CharacterProfile character_profile(const rules::CharacterSheet &sheet,
            std::span<const std::string> gear) const override
    {
        return rules_->character_profile(sheet, gear);
    }

    rules::EquipmentInfo equipment_info(std::string_view key) const override
    {
        return rules_->equipment_info(key);
    }

    std::vector<rules::EquipmentChoice> equipment_choices(const rules::CharacterSheet &sheet,
            std::span<const std::string> gear, unsigned selected) const override
    {
        return rules_->equipment_choices(sheet, gear, selected);
    }

    rules::EquipmentChange equipment_change(const rules::CharacterSheet &sheet,
                                            std::span<const std::string> candidates,
                                            unsigned selected,
                                            rules::EquipmentOperation operation) const override
    {
        return rules_->equipment_change(sheet, candidates, selected, operation);
    }

    rules::SpellAccess spell_access(const rules::CharacterSheet &sheet) const override
    {
        return rules_->spell_access(sheet);
    }

    rules::SpellChoiceOptions spell_choice_options(const rules::CharacterSheet &sheet,
            rules::SpellChoiceContext context) const override
    {
        return rules_->spell_choice_options(sheet, context);
    }

    void apply_spell_choices(rules::CharacterSheet &sheet, const rules::SpellChoices &choices,
                             rules::SpellChoiceContext context,
                             rules::ChoiceCompleteness completeness) const override
    {
        rules_->apply_spell_choices(sheet, choices, context, completeness);
    }

    rules::AbilityCheckModifier ability_check(const rules::CharacterSheet &sheet,
            std::span<const std::string> gear, rules::Ability ability,
            std::string_view skill) const override
    {
        return rules_->ability_check(sheet, gear, ability, skill);
    }

    rules::AbilityCheckRoll roll_ability_check(const rules::CharacterSheet &sheet,
            std::span<const std::string> gear, rules::Ability ability, std::string_view skill,
            RandomState &random_state) const override
    {
        return rules_->roll_ability_check(sheet, gear, ability, skill, random_state);
    }

    unsigned experience_for_level(unsigned level) const override
    {
        return rules_->experience_for_level(level);
    }

    std::optional<rules::TrainingReplacementOptions>
    rest_training_options(const rules::CharacterSheet &sheet) const override
    {
        return rules_->rest_training_options(sheet);
    }

    rules::TrainingChoices replace_rest_training(rules::CharacterSheet &sheet,
            std::span<const std::string> selected) const override
    {
        return rules_->replace_rest_training(sheet, selected);
    }

    std::vector<rules::TrainingChoiceGroup>
    training_options(const rules::CharacterSheet &sheet) const override
    {
        return rules_->training_options(sheet);
    }

    rules::AdvancementOptions advancement_options(const rules::CharacterSheet &sheet) const override
    {
        return rules_->advancement_options(sheet);
    }

    rules::AdvancementOptions advancement_options(const rules::CharacterSheet &sheet,
            const rules::AdvancementChoice &choice) const override
    {
        return rules_->advancement_options(sheet, choice);
    }

    rules::AdvancementChoice default_advancement(const rules::CharacterSheet &sheet) const override
    {
        return rules_->default_advancement(sheet);
    }

    rules::CharacterSheet spell_choice_sheet(const rules::CharacterSheet &sheet,
            const rules::AdvancementChoice &choice) const override
    {
        return rules_->spell_choice_sheet(sheet, choice);
    }

    bool advance_character(rules::CharacterSheet &sheet, rules::VitalState &state,
                           const rules::AdvancementChoice &choice) const override
    {
        return rules_->advance_character(sheet, state, choice);
    }

    void recover(rules::VitalState &state, const rules::CharacterSheet &sheet) const override
    {
        rules_->recover(state, sheet);
    }

    rules::RecoveryInfo recovery_info(const rules::CharacterSheet &sheet,
                                      const rules::VitalState &state) const override
    {
        return rules_->recovery_info(sheet, state);
    }

    void grant_temporary_hit_points(rules::VitalState &state, const rules::CharacterSheet &sheet,
                                    const rules::TemporaryHitPoints &offered,
                                    rules::TemporaryHpChoice choice) const override
    {
        rules_->grant_temporary_hit_points(state, sheet, offered, choice);
    }

    void recover_short_rest(rules::VitalState &state,
                            const rules::CharacterSheet &sheet) const override
    {
        rules_->recover_short_rest(state, sheet);
    }

    rules::Message recover_rest_choice(rules::VitalState &state,
                                       const rules::CharacterSheet &sheet,
                                       std::string_view choice) const override
    {
        return rules_->recover_rest_choice(state, sheet, choice);
    }

    rules::HitDieResult spend_hit_die(rules::VitalState &state, const rules::CharacterSheet &sheet,
                                      RandomState &random_state) const override
    {
        return rules_->spend_hit_die(state, sheet, random_state);
    }

    void elapse(std::span<rules::Participant> participants, std::chrono::milliseconds elapsed,
                RandomState &random_state) const override
    {
        rules_->elapse(participants, elapsed, random_state);
    }

    void validate_character_state(const rules::CharacterSheet &sheet,
                                  const rules::VitalState &state) const override
    {
        rules_->validate_character_state(sheet, state);
    }

    rules::RestPolicy long_rest_policy() const override
    {
        return rules_->long_rest_policy();
    }

    rules::RestPolicy short_rest_policy() const override
    {
        return rules_->short_rest_policy();
    }

    void set_hit_points(rules::VitalState &state, const rules::CharacterSheet &sheet,
                        int hit_points) const override
    {
        rules_->set_hit_points(state, sheet, hit_points);
    }

    void temple_heal(rules::VitalState &state, const rules::CharacterSheet &sheet,
                     RandomState &random_state) const override
    {
        rules_->temple_heal(state, sheet, random_state);
    }

    rules::HazardAttackResult hazard_attack(rules::VitalState &state,
                                            const rules::CharacterSheet &sheet,
                                            const rules::HazardAttack &attack,
                                            RandomState &random_state) const override
    {
        return rules_->hazard_attack(state, sheet, attack, random_state);
    }

    int hit_point_maximum(const rules::CharacterSheet &sheet,
                          const rules::VitalState &state) const override
    {
        return rules_->hit_point_maximum(sheet, state);
    }

    std::vector<rules::CampAction> camp_actions(const rules::CharacterSheet &sheet,
            const rules::VitalState &state) const override
    {
        return rules_->camp_actions(sheet, state);
    }

    void use_camp_action(const rules::CharacterSheet &user, rules::VitalState &user_state,
                         const rules::CharacterSheet &target, rules::VitalState &target_state,
                         std::string_view action, RandomState &random_state) const override
    {
        rules_->use_camp_action(user, user_state, target, target_state, action, random_state);
    }

    void use_party_camp_action(const rules::CharacterSheet &user, rules::VitalState &user_state,
                               std::span<rules::CampTarget> party, std::string_view action,
                               RandomState &random_state) const override
    {
        rules_->use_party_camp_action(user, user_state, party, action, random_state);
    }

    bool can_cast_exploration_spell(const rules::CharacterSheet &sheet,
                                    const rules::VitalState &state,
                                    std::string_view spell) const override
    {
        return rules_->can_cast_exploration_spell(sheet, state, spell);
    }

    void cast_exploration_spell(const rules::CharacterSheet &caster, rules::VitalState &state,
                                std::string_view spell) const override
    {
        rules_->cast_exploration_spell(caster, state, spell);
    }

  private:
    std::unique_ptr<rules::RulesModule> rules_;
};
} // namespace opengold::test
#endif
