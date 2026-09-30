#include "opengold/rules.h"
#include "opengold/character_rules.h"
#include <stdexcept>

namespace opengold::rules
{
CharacterProfile RulesModule::character_profile(const CharacterSheet &,
        std::span<const std::string>) const
{
    throw std::runtime_error("This rules module does not support campaign characters");
}

EquipmentChange RulesModule::equipment_change(const CharacterSheet &, std::span<const std::string>,
        unsigned, EquipmentOperation) const
{
    throw std::runtime_error("This rules module does not support equipment changes");
}

AbilityCheckModifier RulesModule::ability_check(const CharacterSheet &,
        std::span<const std::string>, unsigned,
        std::string_view) const
{
    throw std::runtime_error("This rules module does not support equipped ability checks");
}

unsigned RulesModule::experience_for_level(unsigned) const
{
    throw std::runtime_error("This rules module does not support advancement");
}

void RulesModule::apply_spell_choices(CharacterSheet &, const SpellChoices &, SpellChoiceContext,
                                      bool) const
{
    throw std::runtime_error("Spell choices are not supported");
}

TrainingChoices RulesModule::replace_rest_training(CharacterSheet &,
        std::span<const std::string>) const
{
    throw std::runtime_error("Long Rest training replacement is not supported");
}

bool RulesModule::advance_character(CharacterSheet &, VitalState &) const
{
    throw std::runtime_error("This rules module does not support advancement");
}

bool RulesModule::advance_character(CharacterSheet &sheet, VitalState &state,
                                    const AdvancementChoice &) const
{
    return advance_character(sheet, state);
}

void RulesModule::recover(VitalState &, const CharacterSheet &) const
{
    throw std::runtime_error("This rules module does not support recovery");
}

RestProgress RulesModule::begin_rest(RestKind) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

RestTransition RulesModule::advance_rest(const RestProgress &, std::uint64_t, RestWork) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

RestTransition RulesModule::interrupt_rest(const RestProgress &, RestInterruption) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

RestProgress RulesModule::resume_rest(const RestProgress &) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

std::uint64_t RulesModule::remaining_rest(const RestProgress &) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

void RulesModule::validate_rest(const RestProgress &) const
{
    throw std::runtime_error("This rules module does not support rest activities");
}

RestPolicy RulesModule::long_rest_policy() const
{
    throw std::runtime_error("This rules module does not support recovery");
}

RestPolicy RulesModule::short_rest_policy() const
{
    throw std::runtime_error("This rules module does not support Short Rests");
}

RecoveryInfo RulesModule::recovery_info(const CharacterSheet &, const VitalState &) const
{
    throw std::runtime_error("This rules module does not support recovery information");
}

void RulesModule::grant_temporary_hit_points(VitalState &, const CharacterSheet &,
        const TemporaryHitPoints &, TemporaryHpChoice) const
{
    throw std::runtime_error("This rules module does not support Temporary Hit Points");
}

void RulesModule::recover_short_rest(VitalState &, const CharacterSheet &) const
{
    throw std::runtime_error("This rules module does not support Short Rests");
}

Message RulesModule::recover_rest_choice(VitalState &, const CharacterSheet &,
        std::string_view) const
{
    throw std::runtime_error("This rules module does not support optional rest recovery");
}

HitDieResult RulesModule::spend_hit_die(VitalState &, const CharacterSheet &, std::uint64_t &) const
{
    throw std::runtime_error("This rules module does not support Hit Dice");
}

void RulesModule::set_hit_points(VitalState &, const CharacterSheet &, int) const
{
    throw std::runtime_error("This rules module does not support script HP changes");
}

void RulesModule::temple_heal(VitalState &, const CharacterSheet &, std::uint64_t &) const
{
    throw std::runtime_error("This rules module does not support temple healing");
}

bool Battlefield::contains(Cell p) const noexcept
{
    return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height;
}

unsigned Battlefield::at(Cell p) const noexcept
{
    if (!contains(p))
        return 1;
    const auto index = static_cast<std::size_t>(p.y) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(p.x);
    return index < terrain.size() ? terrain[index] : 1;
}
} // namespace opengold::rules

void opengold::rules::RulesModule::validate_character_state(const CharacterSheet &,
        const VitalState &) const
{
    throw std::runtime_error("Character state validation is unsupported by this rules module");
}
