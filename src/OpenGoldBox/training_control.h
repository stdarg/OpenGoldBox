#ifndef OPENGOLDBOX_TRAINING_CONTROL_H
#define OPENGOLDBOX_TRAINING_CONTROL_H
#include "localization.h"
#include "opengold/character_creator.h"
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>
#include <array>
#include <string_view>
#include <vector>

namespace godot
{
class Node;
}

namespace presentation
{
inline godot::String training_string(std::string_view s)
{
    return godot::String::utf8(s.data(), s.size());
}

// The label of a training choice group's source (its class, class feature,
// background or species feature), by the group's stable ID.
struct TrainingSourceLabel
{
    std::string_view id;
    const char *label;
};

inline constexpr std::array training_source_labels{
    TrainingSourceLabel{"class:fighter:fighting_style", N_("Fighter Fighting Style")},
    TrainingSourceLabel{"class:barbarian", N_("Barbarian class")},
    TrainingSourceLabel{"class:bard", N_("Bard class")},
    TrainingSourceLabel{"class:cleric", N_("Cleric class")},
    TrainingSourceLabel{"class:cleric:divine_order", N_("Cleric Divine Order")},
    TrainingSourceLabel{"class:druid", N_("Druid class")},
    TrainingSourceLabel{"class:druid:primal_order", N_("Druid Primal Order")},
    TrainingSourceLabel{"class:fighter", N_("Fighter class")},
    TrainingSourceLabel{"class:monk", N_("Monk class")},
    TrainingSourceLabel{"class:paladin", N_("Paladin class")},
    TrainingSourceLabel{"class:ranger", N_("Ranger class")},
    TrainingSourceLabel{"class:sorcerer", N_("Sorcerer class")},
    TrainingSourceLabel{"class:warlock", N_("Warlock class")},
    TrainingSourceLabel{"class:wizard", N_("Wizard class")},
    TrainingSourceLabel{"class:rogue", N_("Rogue class")},
    TrainingSourceLabel{"class:rogue:expertise", N_("Rogue Expertise")},
    TrainingSourceLabel{"background:acolyte", N_("Acolyte background")},
    TrainingSourceLabel{"background:soldier", N_("Soldier background")},
    TrainingSourceLabel{"background:sage", N_("Sage background")},
    TrainingSourceLabel{"class:wizard:scholar", N_("Wizard Scholar")},
    TrainingSourceLabel{"background:criminal", N_("Criminal background")},
    TrainingSourceLabel{"species:human:skillful", N_("Human Skillful")},
    TrainingSourceLabel{"species:human:skilled", N_("Skilled feat")},
    TrainingSourceLabel{"species:elf:keen_senses", N_("Elf Keen Senses")}};

godot::String training_summary(const opengold::rules::TrainingProfile &profile, bool fixed = false);

// What a weapon's mastery property does, keyed by its SRD name.
inline const char *mastery_effect(std::string_view mastery)
{
    if (mastery == "Cleave")
    {
        return N_(
            "After a melee hit, attack a second creature within 5 feet of the first with the same weapon. Once per turn.");
    }
    if (mastery == "Graze")
    {
        return N_("A miss still deals damage equal to the ability modifier used for the attack.");
    }
    if (mastery == "Nick")
    {
        return N_(
            "The Light weapon's extra attack comes with the Attack action instead of a Bonus Action. Once per turn.");
    }
    if (mastery == "Push")
    {
        return N_(
            "On a hit, push a Large or smaller creature up to 10 feet straight away from you.");
    }
    if (mastery == "Sap")
    {
        return N_(
            "On a hit, the target has Disadvantage on its next attack roll before your next turn.");
    }
    if (mastery == "Slow")
    {
        return N_(
            "On a hit that deals damage, the target's Speed drops by 10 feet until your next turn.");
    }
    if (mastery == "Topple")
    {
        return N_(
            "On a hit, the target makes a Constitution save (DC 8 + the attack's ability modifier + proficiency bonus) or falls Prone.");
    }
    if (mastery == "Vex")
    {
        return N_(
            "On a hit that deals damage, your next attack against that target before the end of your next turn has Advantage.");
    }
    return nullptr;
}

inline godot::String signed_bonus(int value)
{
    return godot::String(value >= 0 ? "+" : "") + godot::String::num_int64(value);
}

// The scene owns every node. Reuse controls across refreshes so toggling does
// not destroy the focused checkbox or its keyboard navigation position.
// The training choices of the Training page, or with `species` the species'
// own choices on the race page (in "SpeciesChoices").
void refresh_training_controls(godot::Node &parent, const opengold::CharacterCreator &creator,
                               const godot::Callable &toggled,
                               const opengold::rules::TrainingChoices &locked = {},
                               bool species = false);
} // namespace presentation
#endif
