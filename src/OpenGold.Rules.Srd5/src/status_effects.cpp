#include "status_effects.h"
#include "damage.h"
#include "dice.h"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace opengold::srd5::detail
{
namespace
{
template <class T> void unsigned_field(std::istream &input, T &value)
{
    std::string token;
    input >> token;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (!input || result.ec != std::errc{} || result.ptr != token.data() + token.size())
        throw std::runtime_error("Invalid unsigned effect field");
}
} // namespace

int d20(RollModifiers modifiers, std::uint64_t &rng)
{
    const int first = roll_die(rng, 20), mode = modifiers.mode();
    if (!mode)
        return first;
    const int second = roll_die(rng, 20);
    return mode > 0 ? std::max(first, second) : std::min(first, second);
}

SaveResult saving_throw(Ability ability, int bonus, int dc, RollModifiers modifiers,
                        std::uint64_t &rng)
{
    const int natural = d20(modifiers, rng);
    // Ordinary saves compare totals. The special natural 1/20 death-save and
    // attack rules do not apply here. Widen before adding user-supplied values.
    return {ability, natural, bonus, dc, modifiers.mode(), std::int64_t(natural) + bonus >= dc};
}

std::array<unsigned, 2> class_save_proficiencies(std::string_view name)
{
    constexpr std::array<std::string_view, 12> names{"Barbarian", "Bard",     "Cleric",  "Druid",
            "Fighter",   "Monk",     "Paladin", "Ranger",
            "Rogue",     "Sorcerer", "Warlock", "Wizard"};
    constexpr std::array<std::array<unsigned, 2>, 12> saves{{{0, 2},
            {1, 5},
            {4, 5},
            {3, 4},
            {0, 2},
            {0, 1},
            {4, 5},
            {0, 1},
            {1, 3},
            {2, 5},
            {4, 5},
            {3, 4}
        }};
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end())
        throw std::runtime_error("Unknown saving throw class");
    return saves[found - names.begin()];
}

bool healing_blocked(const EffectState &effects, std::uint64_t after_ms)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == EffectKind::chill_touch && e.remaining_ms > after_ms;
    });
}

void apply_chill_touch(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                       std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 ||
            !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Chill Touch application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::chill_touch, 0, duration_ms, 0});
}

void apply_guiding_bolt(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                        std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 ||
            !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Guiding Bolt application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::guiding_bolt, 0, duration_ms, 0});
}

void apply_poisoned(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                    std::string name, unsigned duration_ms, EffectKind kind)
{
    if ((kind != EffectKind::poisoned && kind != EffectKind::dazzled &&
            kind != EffectKind::shield && kind != EffectKind::acid_arrow &&
            kind != EffectKind::raging && kind != EffectKind::reckless &&
            kind != EffectKind::addled && kind != EffectKind::lit &&
            kind != EffectKind::moonlit && kind != EffectKind::scorched) || !can_apply(effects) ||
            !scope || !caster || name.empty() || name.size() > 160 || !duration_ms ||
            duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid timed condition");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name), kind, 0,
                              duration_ms, 0});
}

bool opportunity_blocked(const EffectState &effects)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::shocking_grasp || e.kind == EffectKind::addled;
    });
}

void apply_shocking_grasp(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                          std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 ||
            !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Shocking Grasp application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::shocking_grasp, 0, duration_ms, 0});
}

int speed_penalty(const EffectState &effects)
{
    // Repeated instances of either source do not stack, but these two distinct
    // features each reduce Speed by 10 feet.
    if (restrained(effects) || paralyzed(effects))
        return 1000;
    return (frosted(effects) ? 10 : 0) + (slowed(effects) ? 10 : 0) -
           (has_effect(effects, EffectKind::longstrider) ? 10 : 0);
}

bool slowed(const EffectState &effects)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::slow;
    });
}

bool frosted(const EffectState &effects)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::ray_of_frost;
    });
}

void apply_ray_of_frost(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                        std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 ||
            !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Ray of Frost application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::ray_of_frost, 0, duration_ms, 0});
}

bool sapped(const EffectState &effects)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::sap;
    });
}

bool vexed_by(const EffectState &effects, std::uint64_t scope, rules::EntityId source)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == EffectKind::vex && e.source_scope == scope &&
               e.source_actor == source;
    });
}

bool has_attack_mastery(const EffectState &effects)
{
    return sapped(effects) || slowed(effects) ||
           std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::vex;
    });
}

bool can_apply_attack_mastery(const EffectState &effects, EffectKind kind, std::uint64_t scope,
                              rules::EntityId source)
{
    if (kind != EffectKind::sap && kind != EffectKind::vex && kind != EffectKind::slow)
        return false;
    // The triggering roll consumes this source's earlier Vex. Repeated Sap from
    // one source replaces its application rather than filling the bounded store.
    return effects.next_id < std::numeric_limits<std::uint64_t>::max() &&
           (effects.active.size() < effect_limit ||
            std::any_of(
                effects.active.begin(), effects.active.end(),
                [&](const auto & e)
    {
        return e.source_scope == scope && e.source_actor == source &&
               (e.kind == kind || e.kind == EffectKind::vex);
    }));
}

void apply_attack_mastery(EffectState &effects, EffectKind kind, std::uint64_t scope,
                          rules::EntityId source, std::string name, unsigned duration)
{
    if ((kind != EffectKind::sap && kind != EffectKind::vex && kind != EffectKind::slow) ||
            !scope || !source || name.empty() || name.size() > 160 || !duration ||
            duration > (kind == EffectKind::vex ? 2 * round_ms : round_ms))
        throw std::runtime_error("Invalid attack mastery effect");
    const auto existing = std::find_if(effects.active.begin(), effects.active.end(),
                                       [&](const auto & e)
    {
        return e.kind == kind && e.source_scope == scope &&
               e.source_actor == source;
    });
    if (existing != effects.active.end())
    {
        existing->remaining_ms = duration;
        existing->source_name = std::move(name);
        return;
    }
    if (!can_apply(effects))
        throw std::runtime_error("Attack mastery effect storage exhausted");
    effects.active.push_back(
    {effects.next_id++, scope, source, std::move(name), kind, 0, duration, 0});
}

void consume_attack_masteries(EffectState &attacker, EffectState &target, std::uint64_t scope,
                              rules::EntityId source)
{
    std::erase_if(attacker.active,
                  [](const auto & e)
    {
        return e.kind == EffectKind::sap;
    });
    std::erase_if(target.active,
                  [&](const auto & e)
    {
        return e.kind == EffectKind::vex && e.source_scope == scope &&
               e.source_actor == source;
    });
}

bool paralyzed(const EffectState &effects)
{
    return has_effect(effects, EffectKind::hold_person);
}

bool incapacitated(const EffectState &effects)
{
    return paralyzed(effects) || has_effect(effects, EffectKind::drowsy) ||
           has_effect(effects, EffectKind::asleep) || has_effect(effects, EffectKind::laughing);
}

void apply_repeating_condition(EffectState &effects, EffectKind kind, std::uint64_t scope,
                               rules::EntityId caster, std::string name, int dc,
                               unsigned first_save_ms)
{
    if ((kind != EffectKind::drowsy && kind != EffectKind::laughing &&
            kind != EffectKind::enfeebled) || !can_apply(effects) ||
            !scope || !caster || name.empty() || name.size() > 160 || dc < -2 || dc > 38 ||
            !first_save_ms || first_save_ms > round_ms)
        throw std::runtime_error("Invalid repeated condition");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name), kind, dc, 60000,
                              first_save_ms});
}

void apply_hold_person(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                       std::string name, int dc, unsigned first_save_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 || dc < -2 ||
            dc > 38 || !first_save_ms || first_save_ms > round_ms)
        throw std::runtime_error("Invalid Hold Person application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::hold_person, dc, 60000, first_save_ms});
}

void apply_sanctuary(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                     std::string name, int dc)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 || dc < -2 ||
            dc > 38)
        throw std::runtime_error("Invalid Sanctuary application");
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::sanctuary;
    });
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::sanctuary, dc, 60000, 0});
}

bool restrained(const EffectState &effects)
{
    return has_effect(effects, EffectKind::ensnaring_strike) ||
           has_effect(effects, EffectKind::entangle) || has_effect(effects, EffectKind::webbed);
}

void apply_entangle(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                    std::string name, int dc, EffectKind kind)
{
    if ((kind != EffectKind::entangle && kind != EffectKind::webbed) || !can_apply(effects) ||
            !scope || !caster || name.empty() || name.size() > 160 || dc < -2 || dc > 38)
        throw std::runtime_error("Invalid Entangle application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name), kind, dc, 60000, 0});
}

void apply_ensnaring_strike(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                            std::string name, int dc)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 || dc < -2 ||
            dc > 38)
        throw std::runtime_error("Invalid Ensnaring Strike application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::ensnaring_strike, dc, 60000, 0});
}

bool blinded(const EffectState &effects)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::blindness || e.kind == EffectKind::dazzled;
    });
}

bool can_apply(const EffectState &effects)
{
    return effects.active.size() < effect_limit &&
           effects.next_id < std::numeric_limits<std::uint64_t>::max();
}

void apply_blindness(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                     std::string name, int dc, unsigned first_save_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 || dc < -2 ||
            dc > 38 || !first_save_ms || first_save_ms > round_ms)
        throw std::runtime_error("Invalid blindness application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::blindness, dc, 60000, first_save_ms});
}

void apply_searing_smite(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                         std::string name, int dc)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 || dc < -2 ||
            dc > 38)
        throw std::runtime_error("Invalid Searing Smite application");
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::searing_smite, dc, 60000, 0});
}

// The largest value a benefit carries: a damage type for Resistance, else a
// bonus of at most 10.
int benefit_value_limit(EffectKind kind)
{
    return kind == EffectKind::resistance || kind == EffectKind::dragons_breath
           ? int(DamageType::count) - 1
           : kind == EffectKind::metamagic ? 15
           : 10;
}

unsigned benefit_duration_ms(EffectKind kind)
{
    switch (kind)
    {
    case EffectKind::shield_of_faith:
    case EffectKind::protection_from_evil_and_good:
    case EffectKind::expeditious_retreat:
        return 600000; // Concentration, up to 10 minutes
    case EffectKind::sacred_weapon:
        return 600000; // 10 minutes
    case EffectKind::hunters_mark:
    case EffectKind::longstrider:
    case EffectKind::warding_bond:
    case EffectKind::protection_from_poison:
    case EffectKind::charmed:
    case EffectKind::hex:
    case EffectKind::inspired:
    case EffectKind::magic_weapon:
    case EffectKind::barkskin:
    case EffectKind::invisible:
    case EffectKind::see_invisibility:
        return 3600000; // 1 hour
    case EffectKind::aid:
    case EffectKind::mage_armor:
        return 28800000; // 8 hours
    case EffectKind::prayer_of_healing:
        return 86400000; // until a Long Rest, at most a day
    case EffectKind::heroism:
    case EffectKind::divine_favor:
    case EffectKind::bless:
    case EffectKind::turned:
    case EffectKind::bane:
    case EffectKind::resistance:
    case EffectKind::blur:
    case EffectKind::mirror_image:
    case EffectKind::enlarged:
    case EffectKind::reduced:
    case EffectKind::dragons_breath:
    case EffectKind::innate_sorcery:
    case EffectKind::outlined:
    case EffectKind::shillelagh:
    case EffectKind::heated:
        return 60000; // 1 minute
    case EffectKind::produce_flame:
    case EffectKind::flame_blade:
        return 600000; // 10 minutes
    case EffectKind::metamagic:
        return 6000; // the rest of the turn, cleared when it ends
    case EffectKind::extended:
        return 1200000; // a doubled 10-minute Concentration
    default:
        return 0;
    }
}

void apply_spell_benefit(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                         std::string name, EffectKind kind, int value)
{
    const auto duration = benefit_duration_ms(kind);
    if (!duration || !can_apply(effects) || !scope || !caster || name.empty() ||
            name.size() > 160 || value < 0 || value > benefit_value_limit(kind))
        throw std::runtime_error("Invalid spell benefit");
    effects.active.push_back(
    {effects.next_id++, scope, caster, std::move(name), kind, value, duration, 0});
}

void apply_command(EffectState &effects, std::uint64_t scope, rules::EntityId caster,
                   std::string name, int option, unsigned duration_ms)
{
    if (!can_apply(effects) || !scope || !caster || name.empty() || name.size() > 160 ||
            option < 1 || option > 4 || !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Command application");
    // A newer Command replaces an earlier one.
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::command;
    });
    effects.active.push_back({effects.next_id++, scope, caster, std::move(name),
                              EffectKind::command, option, duration_ms, 0});
}

const Effect *command_effect(const EffectState &effects)
{
    const auto found = std::find_if(effects.active.begin(), effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == EffectKind::command;
    });
    return found == effects.active.end() ? nullptr : &*found;
}

int hit_point_bonus(const EffectState &effects)
{
    int bonus = 0;
    for (const auto &e : effects.active)
        if (e.kind == EffectKind::aid)
            bonus = std::max(bonus, e.dc);
    return bonus;
}

bool has_effect(const EffectState &effects, EffectKind kind)
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == kind;
    });
}

RollModifiers saving_modifiers(Ability ability, bool armor, bool dodge)
{
    return {dodge && ability == Ability::dexterity,
            armor && (ability == Ability::strength || ability == Ability::dexterity)};
}

RollModifiers attack_modifiers(bool attacker_blind, bool target_blind, bool dodging, bool other)
{
    return {target_blind, attacker_blind || (dodging && !target_blind) || other};
}

void elapse_effects(std::span<EffectSubject> subjects, std::uint64_t milliseconds,
                    std::uint64_t &rng, const EffectObserver &observe)
{
    // Stable entity and application ordering makes simultaneous saves repeatable,
    // independent of roster layout and initiative order.
    std::vector<EffectSubject> ordered(subjects.begin(), subjects.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const auto & a, const auto & b)
    {
        return a.id < b.id;
    });
    while (milliseconds)
    {
        std::uint64_t step = milliseconds;
        bool any = false;
        for (const auto &subject : ordered)
            for (const auto &e : subject.effects.get().active)
            {
                any = true;
                step = std::min(step, std::uint64_t(e.remaining_ms));
                if (e.save_in_ms)
                    step = std::min(step, std::uint64_t(e.save_in_ms));
            }
        if (!any)
            return;
        milliseconds -= step;
        for (auto &subject : ordered)
        {
            auto &effects = subject.effects.get().active;
            for (auto &e : effects)
            {
                e.remaining_ms -= static_cast<unsigned>(step);
                if (e.save_in_ms)
                    e.save_in_ms -= static_cast<unsigned>(step);
                EffectEvent event{subject.id, e};
                if (!e.remaining_ms)
                    event.removed = true;
                else if ((e.kind == EffectKind::blindness || e.kind == EffectKind::hold_person ||
                          e.kind == EffectKind::drowsy || e.kind == EffectKind::laughing ||
                          e.kind == EffectKind::enfeebled) &&
                         !e.save_in_ms)
                {
                    // Blindness and Ray of Enfeeblement repeat a Constitution save,
                    // the others a Wisdom one, at the end of each of the target's turns.
                    const auto ability =
                        e.kind == EffectKind::blindness || e.kind == EffectKind::enfeebled
                        ? Ability::constitution
                        : Ability::wisdom;
                    e.save_in_ms = round_ms;
                    if (!subject.dead)
                    {
                        // Bless adds 1d4 to the repeated save; Bane subtracts 1d4.
                        int bless = has_effect(subject.effects.get(), EffectKind::bless)
                                    ? roll_die(rng, 4)
                                    : 0;
                        if (has_effect(subject.effects.get(), EffectKind::bane))
                            bless -= roll_die(rng, 4);
                        event.save = saving_throw(ability,
                                                  subject.saves[static_cast<unsigned>(ability)] + bless,
                                                  e.dc,
                                                  saving_modifiers(ability,
                                                      subject.str_dex_disadvantage,
                                                      subject.dodge),
                                                  rng);
                        event.removed = event.save->success;
                        // A drowsy sleeper that fails falls Unconscious, no more saves.
                        if (!event.save->success && e.kind == EffectKind::drowsy)
                        {
                            e.kind = EffectKind::asleep;
                            e.save_in_ms = 0;
                            subject.effects.get().prone = true;
                        }
                    }
                }
                if (event.removed)
                    e.remaining_ms = 0;
                if (observe && (event.save || event.removed))
                    observe(event);
            }
            std::erase_if(effects,
                          [](const auto & e)
            {
                return !e.remaining_ms;
            });
        }
    }
}

// The only effect-state tag written or read. It lives inside campaign saves and
// combat checkpoints, whose format numbers reject older data.
constexpr std::string_view effects_magic = "FX8";

void write_effects(std::ostream &out, const EffectState &effects)
{
    out << effects_magic << ' ' << effects.next_id << ' ' << effects.active.size();
    for (const auto &e : effects.active)
        out << ' ' << e.id << ' ' << unsigned(e.kind) << ' ' << e.source_scope << ' '
            << e.source_actor << ' ' << std::quoted(e.source_name) << ' ' << e.dc << ' '
            << e.remaining_ms << ' ' << e.save_in_ms;
    out << ' ' << effects.prone;
}

EffectState read_effects(std::istream &in)
{
    std::string magic;
    std::size_t count{};
    EffectState result;
    in >> magic;
    unsigned_field(in, result.next_id);
    unsigned_field(in, count);
    if (!in || magic != effects_magic || !result.next_id || count > effect_limit)
        throw std::runtime_error("Invalid effect state");
    std::uint64_t previous{};
    for (std::size_t n = 0; n < count; ++n)
    {
        Effect e;
        unsigned kind{};
        unsigned_field(in, e.id);
        unsigned_field(in, kind);
        unsigned_field(in, e.source_scope);
        unsigned_field(in, e.source_actor);
        in >> std::quoted(e.source_name) >> e.dc;
        unsigned_field(in, e.remaining_ms);
        unsigned_field(in, e.save_in_ms);
        const bool timed = kind == unsigned(EffectKind::ray_of_frost) ||
                           kind == unsigned(EffectKind::shocking_grasp) ||
                           kind == unsigned(EffectKind::chill_touch) ||
                           kind == unsigned(EffectKind::sap) || kind == unsigned(EffectKind::vex) ||
                           kind == unsigned(EffectKind::slow) ||
                           kind == unsigned(EffectKind::guiding_bolt) ||
                           kind == unsigned(EffectKind::poisoned) ||
                           kind == unsigned(EffectKind::dazzled) ||
                           kind == unsigned(EffectKind::shield) ||
                           kind == unsigned(EffectKind::acid_arrow) ||
                           kind == unsigned(EffectKind::raging) ||
                           kind == unsigned(EffectKind::reckless) ||
                           kind == unsigned(EffectKind::addled) ||
                           kind == unsigned(EffectKind::lit) ||
                           kind == unsigned(EffectKind::moonlit) ||
                           kind == unsigned(EffectKind::scorched);
        // Searing Smite, Ensnaring Strike and Entangle act at the start of the
        // target's turn or on its escape, not on a timer.
        const bool turn_save = kind == unsigned(EffectKind::searing_smite) ||
                               kind == unsigned(EffectKind::ensnaring_strike) ||
                               kind == unsigned(EffectKind::entangle) ||
                               kind == unsigned(EffectKind::webbed) ||
                               kind == unsigned(EffectKind::sanctuary) ||
                               kind == unsigned(EffectKind::asleep);
        // A spell benefit has no save; `dc` carries its value.
        const auto benefit = benefit_duration_ms(static_cast<EffectKind>(kind));
        if (kind == unsigned(EffectKind::command) && in)
        {
            if (e.id <= previous || e.id >= result.next_id || !e.source_scope ||
                    !e.source_actor || e.source_name.empty() || e.source_name.size() > 160 ||
                    e.dc < 1 || e.dc > 4 || !e.remaining_ms || e.remaining_ms > 2 * round_ms ||
                    e.save_in_ms)
                throw std::runtime_error("Invalid active effect");
            e.kind = EffectKind::command;
            previous = e.id;
            result.active.push_back(std::move(e));
            continue;
        }
        if (benefit && in)
        {
            if (e.id <= previous || e.id >= result.next_id || !e.source_scope ||
                    !e.source_actor || e.source_name.empty() || e.source_name.size() > 160 ||
                    e.dc < 0 || e.dc > benefit_value_limit(static_cast<EffectKind>(kind)) ||
                    !e.remaining_ms || e.remaining_ms > benefit ||
                    e.save_in_ms)
                throw std::runtime_error("Invalid active effect");
            e.kind = static_cast<EffectKind>(kind);
            previous = e.id;
            result.active.push_back(std::move(e));
            continue;
        }
        if (!in || (!timed && !turn_save && kind != unsigned(EffectKind::blindness) &&
                    kind != unsigned(EffectKind::hold_person) &&
                    kind != unsigned(EffectKind::drowsy) && kind != unsigned(EffectKind::laughing) &&
                    kind != unsigned(EffectKind::enfeebled)) ||
                e.id <= previous || e.id >= result.next_id || !e.source_scope || !e.source_actor ||
                e.source_name.empty() || e.source_name.size() > 160 ||
                (!timed && (e.dc < -2 || e.dc > 38 || !e.remaining_ms || e.remaining_ms > 60000 ||
                            (turn_save ? e.save_in_ms != 0
                             : (!e.save_in_ms || e.save_in_ms > round_ms)))) ||
                (timed && ((e.dc != 0 && !(kind == unsigned(EffectKind::reckless) && e.dc == 1)) ||
                           e.save_in_ms != 0 || !e.remaining_ms ||
                           e.remaining_ms > ((kind == unsigned(EffectKind::chill_touch) ||
                                              kind == unsigned(EffectKind::vex) ||
                                              kind == unsigned(EffectKind::guiding_bolt) ||
                                              kind == unsigned(EffectKind::poisoned) ||
                                              kind == unsigned(EffectKind::dazzled) ||
                                              kind == unsigned(EffectKind::acid_arrow) ||
                                              kind == unsigned(EffectKind::raging) ||
                                              kind == unsigned(EffectKind::lit))
                                             ? 2 * round_ms
                                             : round_ms))))
            throw std::runtime_error("Invalid active effect");
        e.kind = static_cast<EffectKind>(kind);
        previous = e.id;
        result.active.push_back(std::move(e));
    }
    unsigned prone{};
    unsigned_field(in, prone);
    if (!in || prone > 1)
        throw std::runtime_error("Invalid posture state");
    result.prone = prone;
    return result;
}
} // namespace opengold::srd5::detail
