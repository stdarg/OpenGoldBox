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
    slow = 7
};
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
    bool sleeping{}, prone{}; // Natural sleep persists until a host wake event.
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
[[nodiscard]] bool opportunity_blocked(const EffectState &effects);
void apply_shocking_grasp(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                          std::string name, unsigned duration_ms);
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
