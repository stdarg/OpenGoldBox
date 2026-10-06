#ifndef OPENGOLD_SRD5_STATUS_EFFECTS_H
#define OPENGOLD_SRD5_STATUS_EFFECTS_H

#include "opengold/rules.h"
#include <functional>
#include <iosfwd>

namespace opengold::srd5::detail
{
enum class Ability : unsigned
{
    strength,
    dexterity,
    constitution,
    intelligence,
    wisdom,
    charisma
};
enum class EffectKind : unsigned
{
    blindness = 1,
    ray_of_frost = 2,
    shocking_grasp = 3,
    chill_touch = 4,
    sap = 5,
    vex = 6,
    slow = 7,
    // Searing Smite: Fire damage and a Constitution save at the start of each
    // of the target's turns, for up to 1 minute.
    searing_smite = 8,
    // Spell benefits with no save, SRD 5.2.1. Shield of Faith: +2 AC.
    shield_of_faith = 9,
    // Heroism: Temporary HP equal to `dc` (the caster's spellcasting modifier)
    // at the start of each of the target's turns.
    heroism = 10,
    // Divine Favor: +1d4 Radiant damage on weapon hits.
    divine_favor = 11,
    // Bless: +1d4 to attack rolls and saving throws.
    bless = 12,
    // Protection from Evil and Good: Aberrations, Celestials, Elementals, Fey,
    // Fiends and Undead have Disadvantage on attack rolls against the target.
    protection_from_evil_and_good = 13,
    // Command: the target obeys on its next turn; `dc` holds the option, an
    // index into command_options plus one.
    command = 14,
    // Sacred Weapon: `dc` (the Charisma modifier, at least 1) is added to
    // attack rolls with Melee weapons; they may deal Radiant damage.
    sacred_weapon = 15,
    // Hunter's Mark: the caster's attack-roll hits deal an extra 1d6 Force damage.
    hunters_mark = 16,
    // Longstrider: Speed increases by 10 feet.
    longstrider = 17,
    // Ensnaring Strike: Restrained, 1d6 Piercing damage at the start of each of
    // the target's turns; `dc` is the Strength (Athletics) check that escapes it.
    ensnaring_strike = 18,
    // Entangle: Restrained by the plants; `dc` is the Strength (Athletics)
    // check that frees the creature without ending the spell.
    entangle = 19,
    // Turn Undead: Frightened and Incapacitated, fleeing the Cleric, until it
    // takes damage or the minute ends.
    turned = 20,
    // Aid: the Hit Point maximum (and current Hit Points when cast) rise by `dc`.
    aid = 21,
    // Guiding Bolt: the next attack roll against the target has Advantage,
    // until the end of the caster's next turn.
    guiding_bolt = 22,
    // Bane: the target subtracts 1d4 from attack rolls and saving throws.
    bane = 23,
    // Hold Person: Paralyzed, with a Wisdom save at the end of each of its turns.
    hold_person = 24,
    // Sanctuary: a creature that attacks the target or harms it with a spell
    // makes a Wisdom save against `dc` or loses the attack (CLASS-9).
    sanctuary = 25,
    // Warding Bond: +1 AC and saves, Resistance to all damage; the caster takes
    // the same damage.
    warding_bond = 26,
    // Protection from Poison: Resistance to Poison damage.
    protection_from_poison = 27,
    // Resistance: 1d4 less damage of type `dc` (a DamageType), once per turn.
    resistance = 28,
    // Prayer of Healing: the creature cannot be healed by it again until it
    // finishes a Long Rest.
    prayer_of_healing = 29,
    // Expeditious Retreat: Dash as a Bonus Action each turn.
    expeditious_retreat = 30,
    // Mage Armor: an unarmored creature's base AC becomes 13 + Dexterity.
    mage_armor = 31,
    // Poisoned (Ray of Sickness): Disadvantage on attack rolls and ability checks.
    poisoned = 32,
    // Sleep, first stage: Incapacitated; a failed Wisdom save at the end of its
    // next turn turns it into `asleep`.
    drowsy = 33,
    // Sleep, second stage: Unconscious (Incapacitated, Prone, helpless).
    asleep = 34,
    // Hideous Laughter: Prone and Incapacitated, a Wisdom save at the end of
    // each of its turns and, with Advantage, whenever it takes damage.
    laughing = 35,
    // Color Spray: Blinded until the end of the caster's next turn.
    dazzled = 36,
    // Web: Restrained until an Athletics check breaks free, like Entangle.
    webbed = 37,
    // Shield: +5 AC and no Magic Missile damage until the caster's next turn.
    shield = 38
};

// The longest a spell benefit lasts, in milliseconds.
unsigned benefit_duration_ms(EffectKind kind);
inline constexpr unsigned round_ms = 6000;
inline constexpr std::size_t effect_limit = 128;

struct RollModifiers
{
    bool advantage{}, disadvantage{};

    // Multiple sources never add extra dice. Opposing sources cancel.
    [[nodiscard]] int mode() const
    {
        return int(advantage) - int(disadvantage);
    }
};

struct SaveResult
{
    Ability ability{};
    int natural{}, bonus{}, dc{}, mode{};
    bool success{};
};

[[nodiscard]] SaveResult saving_throw(Ability ability, int bonus, int dc, RollModifiers modifiers,
                                      std::uint64_t &rng);
[[nodiscard]] int d20(RollModifiers modifiers, std::uint64_t &rng);
[[nodiscard]] std::array<unsigned, 2> class_save_proficiencies(std::string_view class_name);

// A source is provenance, never a borrowed Actor pointer. Scope distinguishes
// encounter-local monster IDs when a lasting effect reaches another encounter.
struct Effect
{
    std::uint64_t id{}, source_scope{};
    rules::EntityId source_actor{};
    std::string source_name;
    EffectKind kind{EffectKind::blindness};
    int dc{};
    unsigned remaining_ms{}, save_in_ms{};
    bool operator==(const Effect &) const = default;
};

struct EffectState
{
    std::uint64_t next_id{1};
    std::vector<Effect> active;
    bool prone{};
    bool operator==(const EffectState &) const = default;
};

struct EffectSubject
{
    rules::EntityId id{};
    std::reference_wrapper<EffectState> effects;
    std::array<int, 6> saves{};
    bool dead{}, str_dex_disadvantage{}, dodge{};
};

struct EffectEvent
{
    rules::EntityId target{};
    Effect effect;
    std::optional<SaveResult> save;
    bool removed{};
};

using EffectObserver = std::function<void(const EffectEvent &)>;
// At a future boundary, effects expiring at that boundary no longer prevent healing.
[[nodiscard]] bool healing_blocked(const EffectState &effects, std::uint64_t after_ms = 0);
void apply_chill_touch(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                       std::string name, unsigned duration_ms);
void apply_guiding_bolt(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                        std::string name, unsigned duration_ms);
// Poisoned or Color Spray's Blinded, lasting `duration_ms` (to the end of the
// caster's next turn).
void apply_poisoned(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                    std::string name, unsigned duration_ms, EffectKind kind = EffectKind::poisoned);
[[nodiscard]] bool opportunity_blocked(const EffectState &effects);
void apply_shocking_grasp(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                          std::string name, unsigned duration_ms);
// The net Speed reduction from effects; negative when Longstrider raises Speed,
// and enough to bring any Speed to 0 while Restrained.
[[nodiscard]] int speed_penalty(const EffectState &effects);
[[nodiscard]] bool slowed(const EffectState &effects);
[[nodiscard]] bool frosted(const EffectState &effects);
void apply_ray_of_frost(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                        std::string name, unsigned duration_ms);
[[nodiscard]] bool sapped(const EffectState &effects);
[[nodiscard]] bool vexed_by(const EffectState &effects, std::uint64_t scope,
                            rules::EntityId source);
[[nodiscard]] bool has_attack_mastery(const EffectState &effects);
[[nodiscard]] bool can_apply_attack_mastery(const EffectState &, EffectKind, std::uint64_t scope,
        rules::EntityId source);
void apply_attack_mastery(EffectState &, EffectKind, std::uint64_t scope, rules::EntityId source,
                          std::string name, unsigned duration_ms);
void consume_attack_masteries(EffectState &attacker, EffectState &target, std::uint64_t scope,
                              rules::EntityId source);
[[nodiscard]] bool blinded(const EffectState &effects);
[[nodiscard]] bool can_apply(const EffectState &effects);
void apply_blindness(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                     std::string name, int dc, unsigned first_save_ms);
void apply_ensnaring_strike(EffectState &, std::uint64_t scope, rules::EntityId caster,
                            std::string name, int dc);
void apply_entangle(EffectState &, std::uint64_t scope, rules::EntityId caster, std::string name,
                    int dc, EffectKind kind = EffectKind::entangle);
// Restrained: Speed 0, attacks against it have Advantage, its attacks and
// Dexterity saves have Disadvantage.
[[nodiscard]] bool restrained(const EffectState &effects);
// Paralyzed: Incapacitated and Speed 0; it fails Strength and Dexterity saves,
// attacks against it have Advantage and hits within 5 feet are critical.
[[nodiscard]] bool paralyzed(const EffectState &effects);
// Incapacitated by a spell: Paralyzed, Sleep or Hideous Laughter.
[[nodiscard]] bool incapacitated(const EffectState &effects);
// A spell's repeated save condition with its first save `first_save_ms` away:
// Sleep's drowsiness or Hideous Laughter.
void apply_repeating_condition(EffectState &effects, EffectKind kind, std::uint64_t scope,
                               rules::EntityId caster, std::string name, int dc,
                               unsigned first_save_ms);
void apply_hold_person(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                       std::string name, int dc, unsigned first_save_ms);
void apply_sanctuary(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                     std::string name, int dc);
void apply_searing_smite(EffectState &, std::uint64_t scope, rules::EntityId caster,
                         std::string name, int dc);
// Applies a spell benefit that has no save. `value` is kind-specific.
void apply_spell_benefit(EffectState &, std::uint64_t scope, rules::EntityId caster,
                         std::string name, EffectKind kind, int value);
// Lasts until the end of the target's next turn, `duration_ms` from now.
void apply_command(EffectState &, std::uint64_t scope, rules::EntityId caster, std::string name,
                   int option, unsigned duration_ms);
// The Command the target must obey, if any.
[[nodiscard]] const Effect *command_effect(const EffectState &);
bool has_effect(const EffectState &, EffectKind);
// Aid's increase to the Hit Point maximum; Aid does not stack with itself.
[[nodiscard]] int hit_point_bonus(const EffectState &);
[[nodiscard]] RollModifiers saving_modifiers(Ability ability, bool untrained_armor, bool dodge);
[[nodiscard]] RollModifiers attack_modifiers(bool attacker_blind, bool target_blind,
        bool target_dodging, bool other_disadvantage);
// All subjects share one chronological event queue. Advancing 12 seconds once
// must consume exactly the same rolls as advancing 1 second twelve times.
void elapse_effects(std::span<EffectSubject> subjects, std::uint64_t milliseconds,
                    std::uint64_t &rng, const EffectObserver &observe = {});
void write_effects(std::ostream &out, const EffectState &effects);
[[nodiscard]] EffectState read_effects(std::istream &in);
} // namespace opengold::srd5::detail
#endif
