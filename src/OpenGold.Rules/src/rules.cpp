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

AbilityCheckRoll RulesModule::roll_ability_check(const CharacterSheet &,
        std::span<const std::string>, unsigned, std::string_view,
        RandomState &) const
{
    throw std::runtime_error("This rules module does not support rolled ability checks");
}

unsigned RulesModule::experience_for_level(unsigned) const
{
    throw std::runtime_error("This rules module does not support advancement");
}

CharacterSheet RulesModule::spell_choice_sheet(const CharacterSheet &sheet,
        const AdvancementChoice &) const
{
    auto next = sheet;
    ++next.level;
    return next;
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

bool RulesModule::advance_character(CharacterSheet &, VitalState &,
                                    const AdvancementChoice &) const
{
    throw std::runtime_error("This rules module does not support advancement");
}

void RulesModule::recover(VitalState &, const CharacterSheet &) const
{
    throw std::runtime_error("This rules module does not support recovery");
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

HitDieResult RulesModule::spend_hit_die(VitalState &, const CharacterSheet &, RandomState &) const
{
    throw std::runtime_error("This rules module does not support Hit Dice");
}

void RulesModule::set_hit_points(VitalState &, const CharacterSheet &, int) const
{
    throw std::runtime_error("This rules module does not support script HP changes");
}

void RulesModule::temple_heal(VitalState &, const CharacterSheet &, RandomState &) const
{
    throw std::runtime_error("This rules module does not support temple healing");
}

HazardAttackResult RulesModule::hazard_attack(VitalState &, const CharacterSheet &,
        const HazardAttack &, RandomState &) const
{
    throw std::runtime_error("This rules module does not support hazard attacks");
}

int RulesModule::hit_point_maximum(const CharacterSheet &sheet, const VitalState &) const
{
    return sheet.hit_points;
}

void RulesModule::use_camp_action(const CharacterSheet &, VitalState &, const CharacterSheet &,
                                  VitalState &, std::string_view, RandomState &) const
{
    throw std::runtime_error("This rules module has no camp actions");
}

void RulesModule::use_party_camp_action(const CharacterSheet &, VitalState &,
                                        std::span<const CampTarget>, std::string_view,
                                        RandomState &) const
{
    throw std::runtime_error("This rules module has no camp actions");
}

void RulesModule::cast_exploration_spell(const CharacterSheet &, VitalState &,
        std::string_view) const
{
    throw std::runtime_error("This rules module has no exploration spells");
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
bool has_line_of_sight(const Battlefield &board, Cell from, Cell to)
{
    if (board.at(from) == 1 || board.at(to) == 1)
        return false;
    const int columns = std::abs(to.x - from.x), rows = std::abs(to.y - from.y);
    const int step_x = to.x > from.x ? 1 : -1, step_y = to.y > from.y ? 1 : -1;
    int crossed_x = 0, crossed_y = 0;
    Cell current = from;
    while (crossed_x < columns || crossed_y < rows)
    {
        // Compare the next vertical and horizontal boundary crossings without
        // division. Cell centers are half a cell from their first boundary:
        // t_x = (1 + 2*crossed_x)/(2*columns), similarly for t_y.
        // Cross multiplication also handles horizontal/vertical rays (a zero
        // denominator means that boundary is never crossed).
        const int order = (1 + 2 * crossed_x) * rows - (1 + 2 * crossed_y) * columns;
        if (order == 0)
        {
            // Exact corner contact touches both side cells as well as the
            // diagonal cell. Checking all three prevents sight through walls.
            if (board.at({current.x + step_x, current.y}) == 1 ||
                    board.at({current.x, current.y + step_y}) == 1)
                return false;
            current.x += step_x;
            current.y += step_y;
            ++crossed_x;
            ++crossed_y;
        }
        else if (order < 0)
        {
            current.x += step_x;
            ++crossed_x;
        }
        else
        {
            current.y += step_y;
            ++crossed_y;
        }
        if (board.at(current) == 1)
            return false;
    }
    return true;
}

} // namespace opengold::rules

void opengold::rules::RulesModule::validate_character_state(const CharacterSheet &,
        const VitalState &) const
{
    throw std::runtime_error("Character state validation is unsupported by this rules module");
}
