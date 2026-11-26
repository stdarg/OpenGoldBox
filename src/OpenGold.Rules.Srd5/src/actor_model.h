#ifndef OPENGOLD_SRD5_ACTOR_MODEL_H
#define OPENGOLD_SRD5_ACTOR_MODEL_H
#include "definition.h"
#include "action_budget.h"
#include "character_class.h"
#include "concentration.h"
#include "effect_labels.h"
#include "life_cycle.h"
#include "status_effects.h"
#include "weapons.h"
#include "opengold/rules.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The SRD module's value model of a fight and of a character in it: a
// creature's Definition (its statistics as the content and the profile give
// them), an Actor (one definition fielded, with its life, resources and
// effects), the Content a rules module loads, and the helpers that read and
// change them. Session (the combat) and Module (the campaign rules) build on
// these; none of this draws dice or knows the presentation.
namespace opengold::srd5::detail
{
using rules::Cell;
using rules::EntityId;
using rules::Participant;

// A class resource's count, one for each class (MULTI-1): a multiclass
// character's Bardic Inspiration and Arcane Recovery are its own, though both
// kinds are kept in Actor::arcane. A monster's resources sit under the class
// whose feature they copy.
struct ClassCounts
{
    std::array<int, 12> by_class{};

    [[nodiscard]] int &operator[](CharacterClass klass) noexcept
    {
        return by_class[static_cast<std::size_t>(klass)];
    }

    [[nodiscard]] int operator[](CharacterClass klass) const noexcept
    {
        return by_class[static_cast<std::size_t>(klass)];
    }

    bool operator==(const ClassCounts &) const = default;
};

// An actor has a life state (hit points, death saves, recovery); it is not
// one, so it holds it rather than inheriting it, and two actors do not compare
// equal merely because their hit points match (Effective C++ Item 38).
struct Actor
{
    detail::LifeState life;
    Participant source;
    Definition definition;
    int initiative{}, movement{}, winds{};
    SpellSlots slots{};
    int hit_dice{}, rushes{}, dashes{};
    // Class resources, each class's its own (MULTI-1): Action Surge and Focus;
    // the once-a-rest features; Lay On Hands' pool and other point pools; free
    // casts (Paladin's Smite, Favored Enemy, Innate Sorcery and the like);
    // Channel Divinity (a Cleric's and a Paladin's together), Rage and Wild Shape.
    ClassCounts surges, arcane, lay_on_hands, free_casts, channel_divinity;
    // Magic Initiate's unspent free casts: bit n is Definition::initiate_spells[n].
    int initiate_casts{};
    // The uses each of Definition::innate's pools has spent, four bits a pool.
    int innate_spent{};
    // The creature this actor just hit with a melee weapon on its own turn, which
    // a smite may follow; cleared by any other command or a new turn.
    EntityId smite_target{};
    bool smite_critical{};
    bool
        smite_melee{}; // Divine and Searing Smite need a Melee hit; Ensnaring Strike any weapon hit
    bool rush_used{}, surge_used{};
    detail::ActionBudget actions;
    bool bonus{true}, reaction{true}, dodge{}, disengaged{};
    bool spent_slot{}, savage_used{}, facing_left{};
    bool sneak_used{}, aim_used{}, aim_ready{}, moved{};
    bool attacked{};    // made an attack roll on its own turn (Reckless Attack's first)
    int pounce{};       // Instinctive Pounce's feet, usable only right after entering a Rage
    bool brutal_used{}; // Brutal Strike, once on each of its turns
    bool stun_used{};   // Stunning Strike, once on each turn
    // Fleet Step (Monk 11): after another Bonus Action, Step of the Wind may
    // still be used this turn.
    bool fleet_step{};
    // Heightened Focus (Monk 10): the ally Step of the Wind carries this turn.
    EntityId carried{};
    // The straight line just moved along this turn, for a Boar's charge: its
    // first and last cells, the step repeated and its length. Any command but
    // moving or a melee attack ends it.
    Cell run_start{}, run_end{}, run_step{};
    int run_feet{};
    unsigned selected_weapon{};
    std::vector<unsigned> light_origins;
    unsigned nick_origin{}; // Light weapon used by the current Attack action.
    unsigned light_extra{}; // 0: unused, 1: Bonus Action, 2: Nick; shared once per turn.
    bool cleave_used{}, cleave_damage{};
    // Hunter's Prey, once per turn. horde_origin is the first creature this
    // actor attacked with a weapon this turn, which Horde Breaker attacks beside.
    bool colossus_used{}, horde_used{};
    // Superior Hunter's Prey, once per turn: prey_origin is the marked creature
    // just damaged, within 30 feet of which the mark's damage may strike another.
    bool prey_used{};
    EntityId prey_origin{};
    bool strike_used{}; // Divine Strike, once on each of its turns
    // Eldritch Smite, Lifedrinker and Hurl Through Hell (Warlock), each once
    // on each of its turns.
    bool smite_used{}, drink_used{}, hurl_used{};
    // Dominated (Dominate Beast): it fights on its controller's side, not its
    // own. A controlled creature (dominated or raised) waits out the turn it
    // would get again in the round it came under control after it had acted.
    bool turned{}, skips_turn{};
    bool resistance_used{};      // Resistance (the cantrip) reduces damage once per turn
    bool regeneration_blocked{}; // Took Acid or Fire damage since its last turn began
    // Downed with Regeneration: the round it may get back up, 3d6 rounds after it
    // fell (the original game's troll); 0 otherwise.
    int rises_on_round{};
    // Thrown gear (Acid, Alchemist's Fire, Oil, by ThrownGearEffect) still carried:
    // counted from the inventory when combat begins and spent by each throw.
    std::array<unsigned, 3> thrown_gear_left{};
    // Charged items (MAGIC-3) by inventory ID: the charges, missiles or potions
    // left, counted from the inventory when combat begins and spent by each use.
    std::vector<std::pair<std::uint64_t, unsigned>> item_uses;
    bool burning{};            // Burning: 1d4 Fire damage at the start of each of its turns
    bool flame_lit{};          // Its Flame Tongue's blade is lit (a Bonus Action lights it)
    bool lightning_throw{};    // Its Javelin of Lightning's bolt flies, within one command
    bool keeps_charge{};       // Use Magic Device saved this use's charge, within one command
    bool curse_lifted{};       // Remove Curse freed it from its cursed weapon (MAGIC-1)
    bool displacement_spent{}; // Its Cloak of Displacement's miss is used this fight (MAGIC-4)
    // The enemy its last command on its own turn aimed at, for the computer (#266).
    rules::EntityId last_target{};
    int oiled_until_round{};  // Covered in Oil through this round: Fire damage deals 5 more
    int coated_until_round{}; // Its weapon carries Basic Poison through this round; 0 for none
    bool shield_off{};        // Took its shield off this fight (for a two-handed bow)
    // The weapon it drew to fire, in place of the one it holds, until it strikes in
    // melee or selects a weapon. Its Definition is unchanged: drawing costs nothing.
    std::string drawn_weapon;
    rules::WeaponSetShown weapon_set_shown{rules::WeaponSetShown::staged};
    // Tried to run off the field as fast as the fastest enemy and failed: a
    // party member stays until the fight ends (the original's rule). A broken
    // monster that still cannot get away charges in panic instead (MORALE-2).
    bool must_stay{};
    // Ran off the field this turn; it leaves the fight when the turn ends.
    bool fled{};
    // Its morale broke: it runs for the edge until it rallies.
    bool panicked{};
    // How near its flight has come to the field's edge, and for how many of its
    // turns since then it has come no nearer.
    int closest_to_edge{}, turns_stuck{};
    // Gave up: it leaves the fight, counted as defeated.
    bool surrendered{};
    EntityId horde_origin{};
    detail::ConcentrationState concentration; // the one Concentration spell this actor keeps
    // Wild Shape: the Beast form's statistics, derived from the wild_shape effect.
    std::optional<Definition> form;
    bool light_damage{};        // Lent for one attack only; pending hits carry their own flag.
    bool involuntary_overlap{}; // Interrupted in an occupied space; retained through recovery until
    // separated.
    detail::EffectState effects;
    // Spell effects an Antimagic Field suppresses while the creature stands in
    // it; their time still runs, and they return when it leaves.
    std::vector<detail::Effect> suppressed;
};

// The actor's effects with those an Antimagic Field suppresses, in the order
// they were applied: what a save or a checkpoint keeps.
[[nodiscard]] detail::EffectState all_effects(const Actor &a);

// What the value placeholder names in an effect's description.
[[nodiscard]] rules::MessageArgument effect_value(const detail::Effect &effect,
                                                  detail::EffectValue value);

// Durations past this are the "until Greater Restoration" kinds, never counted
// down to an end.
inline constexpr unsigned lasting_effect_ms = 2'592'000'000u; // 30 days

// Every status effect a player would notice on the actor, in the order they
// were applied, then the states that are not effects: Prone, Burning, oil,
// panic and Concentration.
[[nodiscard]] std::vector<rules::StatusEffectView> status_effect_views(const Actor &a, bool oiled);

// The actor's AC with its lasting effects. Mage Armor replaces an unarmored
// base AC. Barkskin: an AC of 17 if it was lower. Shield of Faith: +2. Warding
// Bond: +1. Shield: +5.
[[nodiscard]] int armor_class_of(const Actor &target);

struct ChampionMove
{
    EntityId actor{}, target{};
    int natural{}, remaining{};
    bool spell{};
    Cell origin;
    bool triggered{}, cleave{}, helpless{};
    Cell trigger_origin{}, target_origin{};
    // Tactical Shift (Fighter 5): the same free move, after Second Wind.
    bool shift{};
    // Forceful Blow (Barbarian 9): the free move must close on this creature.
    EntityId toward{};
    // Cunning Strike's Withdraw (Rogue 5): the same free move after the attack.
    bool withdraw{};
};

struct PendingMastery
{
    EntityId actor{}, target{};
    detail::Mastery kind{detail::Mastery::none};
    int natural{};
    bool ranged{}, targeting{};
    std::string weapon;
    Cell origin;
    unsigned thrown_item{};
    bool tactical{}; // Tactical Master replaced the weapon's own mastery
};

struct EffectReaction
{
    EntityId actor{};
    Cell source{}, mover{};
    int movement{};
    bool prone{};
};

struct PendingGraze
{
    EntityId actor{}, target{};
    int natural{};
};

struct PendingCheck
{
    EntityId actor{}, target{};
    int natural{};
    bool surge_spent{};
    int guidance{}; // Guidance's 1d4, rolled with the check; 0 without it.
};
} // namespace opengold::srd5::detail
#endif
