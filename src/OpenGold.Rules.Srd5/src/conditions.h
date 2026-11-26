#ifndef OPENGOLD_SRD5_CONDITIONS_H
#define OPENGOLD_SRD5_CONDITIONS_H
#include "actor_model.h"
#include "definition.h"
#include "status_effects.h"
#include "opengold/rules.h"
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// An actor's states and conditions and what ends them: Lesser and Greater
// Restoration, Remove Curse, a temple's services and a rest; and the vitals a
// save keeps between fights.
namespace opengold::srd5::detail
{
using rules::Cell;
using rules::Message;
using rules::TempleService;
using rules::VitalState;

[[nodiscard]] bool unconscious(const Actor &a);

// Raise Dead's penalty to D20 Tests, one less after each Long Rest.
[[nodiscard]] int return_penalty(const Actor &a);

[[nodiscard]] bool conscious(const Actor &a);

// Banished to a demiplane, or lost in Maze's: off the field until the spell ends.
[[nodiscard]] bool absent(const Actor &a);

// Fallen upward by Reverse Gravity: out of melee reach until it ends.
[[nodiscard]] bool aloft(const Actor &a);

// One Arcane Recovery choice: how many slots of each level from 1 to 5 return.
struct ArcaneAllocation
{
    std::string id;
    Message label;
    std::array<unsigned, 5> slots{};
};

// The slot levels a choice restores, highest first: "3, 1, 1".
[[nodiscard]] std::string recovered_levels(const std::array<unsigned, 5> &slots);

// Arcane Recovery's choices: expended slots of levels 1 to 5 whose levels add
// up to at most `budget`, half the Wizard level rounded up (SRD 5.2.1 p. 78).
// The three small choices keep the names they had before level 5.
[[nodiscard]] std::vector<ArcaneAllocation> arcane_allocations(unsigned budget);

// Whether an effect gives the named Lesser Restoration condition.
[[nodiscard]] bool gives_condition(detail::EffectKind kind, std::string_view condition);

[[nodiscard]] std::string condition_name(std::string_view condition);

[[nodiscard]] bool has_condition(const Actor &a, std::string_view condition);

// Poisoned: Disadvantage on attack rolls and ability checks. Heroes' Feast
// makes a creature immune.
[[nodiscard]] bool suffers_poisoned(const Actor &a);

// The conditions Restoring Touch ends, most disabling first, and the effects
// that give each. Deafened is not modeled (DEAF-1).
inline constexpr std::array<std::pair<std::string_view, std::array<detail::EffectKind, 3>>, 5>
    restorable_conditions{
        {{"Paralyzed",
          {detail::EffectKind::hold_person, detail::EffectKind::paralysis_ray,
           detail::EffectKind::clawed}},
         {"Stunned",
          {detail::EffectKind::stunned, detail::EffectKind::stunned, detail::EffectKind::stunned}},
         {"Blinded",
          {detail::EffectKind::blindness, detail::EffectKind::dazzled,
           detail::EffectKind::dazzled}},
         {"Frightened",
          {detail::EffectKind::frightened, detail::EffectKind::abjured,
           detail::EffectKind::abjured}},
         {"Charmed",
          {detail::EffectKind::charmed, detail::EffectKind::enthralled,
           detail::EffectKind::turn_charmed}}}};

[[nodiscard]] bool touch_restorable(const Actor &a);

// A curse on the creature, which Remove Curse ends: Hex so far.
[[nodiscard]] bool curse(detail::EffectKind kind);

// Raise Dead, Resurrection and Reincarnate bring a dead creature back with
// `hit_points`, its poisons neutralized and a body turned to dust restored.
void bring_back(Actor &patient, int hit_points);

// A Poisoned effect other than Contagion's disease.
[[nodiscard]] bool poison(detail::EffectKind kind);

// Whether a temple's service would help the creature (TEMPLE-1).
[[nodiscard]] bool temple_helps(const Actor &a, TempleService service);

// It wields or wears a cursed item and cannot put it down until Remove Curse
// (MAGIC-1). With `weapons_only`, only a cursed weapon counts: a cursed necklace
// does not stop a weapon swap.
[[nodiscard]] bool bound_to_cursed_item(const Actor &a, bool weapons_only = false);

// A snake or scorpion, the original's creature kind that Snake Charm affects
// (MAGIC-6), known by its combat creature's name.
[[nodiscard]] bool charmable_snake(const Actor &c);

[[nodiscard]] bool cursed(const Actor &a);

// "Celestials", "Elementals", "Fey", "Fiends" or "Undead".
[[nodiscard]] std::string ward_label(std::size_t type);

// A Bestow Curse curse as players read it, by its index in curses.
[[nodiscard]] std::string curse_label(std::size_t curse);

// A wall's direction as players read it, by its index in wall_directions.
[[nodiscard]] std::string wall_direction_label(std::size_t direction);

// The directions Compulsion names, 0 north and on clockwise to 7.
inline constexpr std::array<Cell, 8> compass{Cell{0, -1}, Cell{1, -1}, Cell{1, 0},  Cell{1, 1},
                                             Cell{0, 1},  Cell{-1, 1}, Cell{-1, 0}, Cell{-1, -1}};

inline constexpr std::array<std::string_view, 8> compass_names{
    "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"};

// Tsunami's headings, by the index its zone keeps in `dealt`.
inline constexpr std::array<Cell, 8> wave_headings{Cell{0, 1},  Cell{0, -1}, Cell{1, 0},
                                                   Cell{-1, 0}, Cell{1, 1},  Cell{-1, -1},
                                                   Cell{1, -1}, Cell{-1, 1}};

// Tsunami's heading: across its wall, away from the caster at `origin`.
[[nodiscard]] std::size_t wave_heading(std::string_view verb, Cell origin, Cell center);

// The ward type a "magic_circle_fey" verb names.
[[nodiscard]] std::size_t ward_of(std::string_view verb);

// Whether Greater Restoration has a harm to end on the creature, by its
// index in greater_harms.
[[nodiscard]] bool has_greater_harm(const Actor &a, std::size_t harm);

// Whether Lesser Restoration has a condition to end on the creature.
[[nodiscard]] bool restorable(const Actor &a);

// The Hit Point Die a creature with `remaining` of them spends next: the
// largest it has left. A multiclass character's dice differ (MULTI-1); it
// spends its largest first, so those left are always its smallest, and a Long
// Rest gives all of them back.
[[nodiscard]] int next_hit_die(const Definition &d, int remaining);

// The level Arcane Recovery (Wizard) or Natural Recovery (Land Druid) counts:
// that class's, the higher of the two for a character with both (MULTI-1).
[[nodiscard]] int recovery_class_level(const Definition &d);

[[nodiscard]] bool can_recover(const Actor &actor, const ArcaneAllocation &choice);

[[nodiscard]] int movement_left(const Actor &a);

// The only vital-state tag this module reads or writes; see profile_magic.
inline constexpr std::string_view vitals_magic = "SRD13";

void restore_vitals(Actor &a, const VitalState &state);

// An actor for a rules call outside combat: full Second Winds and slots from
// its definition, then its saved vitals. One place, so a new resource cannot be
// missed in one of the many callers.
[[nodiscard]] Actor actor_from(const Definition &definition, const VitalState &state);

// Befuddlement lasts the fight (LEVELS-5).
void end_befuddlement(Actor &a);

[[nodiscard]] VitalState vitals(const Actor &a);
} // namespace opengold::srd5::detail
#endif
