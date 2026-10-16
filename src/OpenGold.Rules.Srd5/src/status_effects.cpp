#include "status_effects.h"
#include "beast_forms.h"
#include "damage.h"
#include "dice.h"
#include "spell_table.h"
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

// Every effect records who imposed it, by scoped ID and by a name to show.
bool recorded(EffectSource source, const std::string &name)
{
    return source.scope && source.actor && !name.empty() && name.size() <= 160;
}
} // namespace

int d20(RollModifiers modifiers, std::uint64_t &rng, bool *lucky_reroll)
{
    const auto die = [&]
    {
        const int rolled = roll_die(rng, 20);
        if (!modifiers.lucky || rolled != 1)
            return rolled;
        if (lucky_reroll)
            *lucky_reroll = true;
        return roll_die(rng, 20);
    };
    const int first = die(), mode = modifiers.mode();
    if (!mode)
        return first;
    const int second = die();
    return mode > 0 ? std::max(first, second) : std::min(first, second);
}

SaveResult saving_throw(Ability ability, int bonus, int dc, RollModifiers modifiers,
                        std::uint64_t &rng, bool *lucky_reroll)
{
    const int natural = d20(modifiers, rng, lucky_reroll);
    // Ordinary saves compare totals. The special natural 1/20 death-save and
    // attack rules do not apply here. Widen before adding user-supplied values.
    return {ability, natural, bonus, dc, modifiers.mode(),
            static_cast<std::int64_t>(natural) + bonus >= dc};
}

std::array<Ability, 2> class_save_proficiencies(CharacterClass character_class)
{
    using enum Ability;
    // Indexed by CharacterClass.
    constexpr std::array<std::array<Ability, 2>, 12> saves{{{strength, constitution},
            {dexterity, charisma},
            {wisdom, charisma},
            {intelligence, wisdom},
            {strength, constitution},
            {strength, dexterity},
            {wisdom, charisma},
            {strength, dexterity},
            {dexterity, intelligence},
            {constitution, charisma},
            {wisdom, charisma},
            {intelligence, wisdom}
        }};
    return saves.at(static_cast<std::size_t>(character_class));
}

bool save_proficient(CharacterClass character_class, unsigned level, Ability ability)
{
    const auto trained = class_save_proficiencies(character_class);
    // Disciplined Survivor (Monk 14): every save. Slippery Mind (Rogue 15):
    // Wisdom and Charisma too.
    return ability == trained[0] || ability == trained[1] ||
           (character_class == CharacterClass::monk && level >= 14) ||
           (character_class == CharacterClass::rogue && level >= 15 &&
            (ability == Ability::wisdom || ability == Ability::charisma));
}

bool healing_blocked(const EffectState &effects, std::uint64_t after_ms) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return (e.kind == EffectKind::chill_touch && e.remaining_ms > after_ms) ||
               e.kind == EffectKind::simulacrum;
    });
}

void apply_chill_touch(EffectState &effects, EffectSource source,
                       std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) ||
            !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Chill Touch application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::chill_touch, 0, duration_ms, 0});
}

void apply_guiding_bolt(EffectState &effects, EffectSource source,
                        std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) ||
            !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Guiding Bolt application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::guiding_bolt, 0, duration_ms, 0});
}

void apply_poisoned(EffectState &effects, EffectSource source,
                    std::string name, unsigned duration_ms, EffectKind kind)
{
    if ((kind != EffectKind::poisoned && kind != EffectKind::dazzled &&
            kind != EffectKind::shield && kind != EffectKind::acid_arrow &&
            kind != EffectKind::raging && kind != EffectKind::reckless &&
            kind != EffectKind::addled && kind != EffectKind::lit &&
            kind != EffectKind::moonlit && kind != EffectKind::scorched &&
            kind != EffectKind::stunned && kind != EffectKind::staggered &&
            kind != EffectKind::dazed && kind != EffectKind::smite_protected &&
            kind != EffectKind::faith_tested && kind != EffectKind::pack_tested &&
            kind != EffectKind::beings_tested && kind != EffectKind::veiled &&
            kind != EffectKind::guarded && kind != EffectKind::swarmed &&
            kind != EffectKind::bladed && kind != EffectKind::bathed &&
            kind != EffectKind::lethargic && kind != EffectKind::retching &&
            kind != EffectKind::ethereal && kind != EffectKind::sleeted &&
            kind != EffectKind::groped && kind != EffectKind::vitriol &&
            kind != EffectKind::wall_burned && kind != EffectKind::breathed &&
            kind != EffectKind::lifted && kind != EffectKind::hand_ready &&
            kind != EffectKind::hurled && kind != EffectKind::fey_fright &&
            kind != EffectKind::thorned && kind != EffectKind::swum &&
            kind != EffectKind::clouded && kind != EffectKind::rooted &&
            kind != EffectKind::jigging) ||
            !can_apply(effects) ||
            !recorded(source, name) || !duration_ms ||
            duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid timed condition");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name), kind,
                              0, duration_ms, 0});
}

bool opportunity_blocked(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::shocking_grasp || e.kind == EffectKind::addled ||
               e.kind == EffectKind::staggering;
    });
}

void apply_brutal_blow(EffectState &effects, EffectKind kind, EffectSource source,
                       std::string name, unsigned duration_ms)
{
    if ((kind != EffectKind::staggering && kind != EffectKind::sundered) || !can_apply(effects) ||
            !recorded(source, name) || !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Brutal Strike blow");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name), kind,
                              kind == EffectKind::staggering ? 1 : 0, duration_ms, 0});
}

bool spend_staggering(EffectState &effects)
{
    for (auto &e : effects.active)
        if (e.kind == EffectKind::staggering && e.dc == 1)
        {
            e.dc = 0;
            return true;
        }
    return false;
}

bool spend_sundering(EffectState &effects, EffectSource attacker)
{
    const auto found = std::find_if(effects.active.begin(), effects.active.end(),
                                    [&](const auto & e)
    {
        return e.kind == EffectKind::sundered &&
               !(e.source_scope == attacker.scope && e.source_actor == attacker.actor);
    });
    if (found == effects.active.end())
        return false;
    effects.active.erase(found);
    return true;
}

void apply_shocking_grasp(EffectState &effects, EffectSource source,
                          std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) ||
            !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Shocking Grasp application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::shocking_grasp, 0, duration_ms, 0});
}

int speed_penalty(const EffectState &effects) noexcept
{
    // Repeated instances of one source do not stack, but these distinct
    // features each reduce Speed by 10 feet.
    if (restrained(effects) || paralyzed(effects) || has_effect(effects, EffectKind::lethargic) ||
            has_effect(effects, EffectKind::entranced) || has_effect(effects, EffectKind::petrified) ||
            has_effect(effects, EffectKind::jarred) || has_effect(effects, EffectKind::rooted) ||
            has_effect(effects, EffectKind::dancing) || has_effect(effects, EffectKind::jigging) ||
            has_effect(effects, EffectKind::aloft))
        return 1000;
    // Freedom of Movement: magic (Ray of Frost) cannot lower its Speed.
    const bool free = has_effect(effects, EffectKind::free_moving);
    return (frosted(effects) && !free ? 10 : 0) + (slowed(effects) ? 10 : 0) +
           (has_effect(effects, EffectKind::frosts_chill) ? 10 : 0) +
           (has_effect(effects, EffectKind::hamstrung) ? 15 : 0) -
           (has_effect(effects, EffectKind::longstrider) ? 10 : 0);
}

bool slowed(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::slow;
    });
}

bool frosted(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::ray_of_frost;
    });
}

void apply_ray_of_frost(EffectState &effects, EffectSource source,
                        std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) ||
            !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Ray of Frost application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::ray_of_frost, 0, duration_ms, 0});
}

bool heightened_against(const EffectState &effects, std::uint64_t source_scope,
                        rules::EntityId source_actor, std::string_view spell)
{
    const int index = spell_index(spell);
    return std::any_of(effects.active.begin(), effects.active.end(), [&](const auto & e)
    {
        return e.kind == EffectKind::heightened && e.source_scope == source_scope &&
               e.source_actor == source_actor && e.dc == index;
    });
}

namespace
{
// The spell whose repeated save a lasting effect calls for.
std::string_view repeated_save_spell(EffectKind kind)
{
    switch (kind)
    {
    case EffectKind::blindness:
        return "blindness";
    case EffectKind::hold_person:
        return "hold_person";
    case EffectKind::drowsy:
        return "sleep";
    case EffectKind::laughing:
        return "hideous_laughter";
    case EffectKind::confused:
        return "confusion";
    case EffectKind::time_slowed:
        return "slow";
    default:
        return "ray_of_enfeeblement";
    }
}
} // namespace

void apply_hamstring(EffectState &effects, EffectSource source, std::string name,
                     unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) || !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Hamstring Blow application");
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::hamstrung;
    });
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::hamstrung, 0, duration_ms, 0});
}

void apply_frosts_chill(EffectState &effects, EffectSource source,
                        std::string name, unsigned duration_ms)
{
    if (!can_apply(effects) || !recorded(source, name) ||
            !duration_ms || duration_ms > round_ms)
        throw std::runtime_error("Invalid Frost's Chill application");
    // A second chill renews the first rather than slowing further.
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::frosts_chill;
    });
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::frosts_chill, 0, duration_ms, 0});
}

bool sapped(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::sap;
    });
}

bool vexed_by(const EffectState &effects, EffectSource source) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == EffectKind::vex && e.source_scope == source.scope &&
               e.source_actor == source.actor;
    });
}

bool studied_by(const EffectState &effects, EffectSource source) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == EffectKind::studied && e.source_scope == source.scope &&
               e.source_actor == source.actor;
    });
}

void apply_studied(EffectState &effects, EffectSource source, std::string name,
                   unsigned duration_ms)
{
    std::erase_if(effects.active, [&](const auto & e)
    {
        return e.kind == EffectKind::studied && e.source_scope == source.scope &&
               e.source_actor == source.actor;
    });
    if (!can_apply(effects) || !recorded(source, name) || !duration_ms ||
            duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Studied Attacks application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::studied, 0, duration_ms, 0});
}

bool has_attack_mastery(const EffectState &effects) noexcept
{
    return sapped(effects) || slowed(effects) ||
           std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::vex;
    });
}

bool can_apply_attack_mastery(const EffectState &effects, EffectKind kind, EffectSource source)
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
        return e.source_scope == source.scope && e.source_actor == source.actor &&
               (e.kind == kind || e.kind == EffectKind::vex);
    }));
}

void apply_attack_mastery(EffectState &effects, EffectKind kind, EffectSource source,
                          std::string name, unsigned duration)
{
    if ((kind != EffectKind::sap && kind != EffectKind::vex && kind != EffectKind::slow) ||
            !recorded(source, name) || !duration ||
            duration > (kind == EffectKind::vex ? 2 * round_ms : round_ms))
        throw std::runtime_error("Invalid attack mastery effect");
    const auto existing = std::find_if(effects.active.begin(), effects.active.end(),
                                       [&](const auto & e)
    {
        return e.kind == kind && e.source_scope == source.scope &&
               e.source_actor == source.actor;
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
    {effects.next_id++, source.scope, source.actor, std::move(name), kind, 0, duration, 0});
}

void consume_attack_masteries(EffectState &attacker, EffectState &target, EffectSource source)
{
    std::erase_if(attacker.active,
                  [](const auto & e)
    {
        return e.kind == EffectKind::sap;
    });
    std::erase_if(target.active,
                  [&](const auto & e)
    {
        return e.kind == EffectKind::vex && e.source_scope == source.scope &&
               e.source_actor == source.actor;
    });
}

// Every Paralyzed and Restrained effect in the game is magic, which Freedom of
// Movement keeps off.
bool paralyzed(const EffectState &effects) noexcept
{
    return has_effect(effects, EffectKind::hold_person) &&
           !has_effect(effects, EffectKind::free_moving);
}

bool stunned(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(), [](const auto & e)
    {
        return e.kind == EffectKind::stunned || e.kind == EffectKind::word_stunned ||
               (e.kind == EffectKind::worded && e.dc == 1);
    });
}

void apply_asleep(EffectState &effects, EffectSource source, std::string name)
{
    if (!can_apply(effects) || !recorded(source, name))
        throw std::runtime_error("Invalid sleep");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::asleep, 0, 60000, 0});
    effects.prone = true;
}

bool incapacitated(const EffectState &effects) noexcept
{
    return paralyzed(effects) || stunned(effects) || has_effect(effects, EffectKind::petrified) ||
           has_effect(effects, EffectKind::jarred) ||
           has_effect(effects, EffectKind::lethargic) ||
           has_effect(effects, EffectKind::entranced) ||
           has_effect(effects, EffectKind::banished) || has_effect(effects, EffectKind::mazed) ||
           has_effect(effects, EffectKind::hurled) ||
           has_effect(effects, EffectKind::drowsy) ||
           has_effect(effects, EffectKind::asleep) || has_effect(effects, EffectKind::laughing) ||
           has_effect(effects, EffectKind::knocked_out);
}

void apply_repeating_condition(EffectState &effects, EffectKind kind, EffectSource source,
                               std::string name, RepeatingSave save)
{
    if ((kind != EffectKind::drowsy && kind != EffectKind::laughing &&
            kind != EffectKind::enfeebled && kind != EffectKind::frightened &&
            kind != EffectKind::knocked_out && kind != EffectKind::confused &&
            kind != EffectKind::time_slowed) ||
            !can_apply(effects) ||
            !recorded(source, name) || save.dc < -2 || save.dc > 38 || !save.first_save_ms ||
            save.first_save_ms > round_ms)
        throw std::runtime_error("Invalid repeated condition");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name), kind,
                              save.dc, 60000, save.first_save_ms});
}

void apply_hold_person(EffectState &effects, EffectSource source,
                       std::string name, RepeatingSave save)
{
    if (!can_apply(effects) || !recorded(source, name) || save.dc < -2 || save.dc > 38 ||
            !save.first_save_ms || save.first_save_ms > round_ms)
        throw std::runtime_error("Invalid Hold Person application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::hold_person, save.dc, 60000, save.first_save_ms});
}

void apply_sanctuary(EffectState &effects, EffectSource source,
                     std::string name, int dc)
{
    if (!can_apply(effects) || !recorded(source, name) || dc < -2 || dc > 38)
        throw std::runtime_error("Invalid Sanctuary application");
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::sanctuary;
    });
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::sanctuary, dc, 60000, 0});
}

bool restrained(const EffectState &effects) noexcept
{
    return (has_effect(effects, EffectKind::ensnaring_strike) ||
            has_effect(effects, EffectKind::entangle) || has_effect(effects, EffectKind::webbed) ||
            has_effect(effects, EffectKind::tentacled) || has_effect(effects, EffectKind::engulfed) ||
            has_effect(effects, EffectKind::lifted) || has_effect(effects, EffectKind::stoning) ||
            has_effect(effects, EffectKind::indigo)) &&
           !has_effect(effects, EffectKind::free_moving);
}

void apply_entangle(EffectState &effects, EffectSource source,
                    std::string name, int dc, EffectKind kind)
{
    if ((kind != EffectKind::entangle && kind != EffectKind::webbed &&
            kind != EffectKind::tentacled) || !can_apply(effects) ||
            !recorded(source, name) || dc < -2 || dc > 38)
        throw std::runtime_error("Invalid Entangle application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name), kind,
                              dc, 60000, 0});
}

void apply_ensnaring_strike(EffectState &effects, EffectSource source,
                            std::string name, int dc)
{
    if (!can_apply(effects) || !recorded(source, name) || dc < -2 || dc > 38)
        throw std::runtime_error("Invalid Ensnaring Strike application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::ensnaring_strike, dc, 60000, 0});
}

bool blinded(const EffectState &effects) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [](const auto & e)
    {
        return e.kind == EffectKind::blindness || e.kind == EffectKind::dazzled ||
               e.kind == EffectKind::worded || e.kind == EffectKind::violet;
    });
}

bool can_apply(const EffectState &effects) noexcept
{
    return effects.active.size() < effect_limit &&
           effects.next_id < std::numeric_limits<std::uint64_t>::max();
}

void apply_blindness(EffectState &effects, EffectSource source,
                     std::string name, RepeatingSave save)
{
    if (!can_apply(effects) || !recorded(source, name) || save.dc < -2 || save.dc > 38 ||
            !save.first_save_ms || save.first_save_ms > round_ms)
        throw std::runtime_error("Invalid blindness application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::blindness, save.dc, 60000, save.first_save_ms});
}

void apply_searing_smite(EffectState &effects, EffectSource source,
                         std::string name, int dc)
{
    if (!can_apply(effects) || !recorded(source, name) || dc < -2 || dc > 38)
        throw std::runtime_error("Invalid Searing Smite application");
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::searing_smite, dc, 60000, 0});
}

// The largest value a benefit carries: a damage type for Resistance, a
// hidden creature's Stealth total, Aid's raise from a level-9 slot, else a
// bonus of at most 10.
int benefit_value_limit(EffectKind kind)
{
    return kind == EffectKind::resistance || kind == EffectKind::dragons_breath ||
           kind == EffectKind::energy_ward || kind == EffectKind::defending ||
           kind == EffectKind::minor_elementals || kind == EffectKind::spirit_breath ||
           kind == EffectKind::shared_resistance || kind == EffectKind::engulfed ||
           kind == EffectKind::elemental_spirit
           ? static_cast<int>(DamageType::count) - 1
           : kind == EffectKind::dominated || kind == EffectKind::violet ||
             kind == EffectKind::befuddled || kind == EffectKind::word_stunned ? 40
           : kind == EffectKind::indigo ? 4033
           : kind == EffectKind::overchanneled || kind == EffectKind::luck_used ? 20
           : kind == EffectKind::arcanum_used ? 7
           : kind == EffectKind::inspired ? 12
           : kind == EffectKind::resurged ? 1
           : kind == EffectKind::cursed ? 18
           : kind == EffectKind::contagion ? 335
           : kind == EffectKind::returned ? 4
           : kind == EffectKind::drained || kind == EffectKind::simulacrum ? 2000
           : kind == EffectKind::feasted ? 20
           : kind == EffectKind::animating ? 19
           : kind == EffectKind::worded ? 2
           : kind == EffectKind::knitting ? 9
           : kind == EffectKind::fire_shield ? 1
           : kind == EffectKind::haunted ? 13
           : kind == EffectKind::summoned ? 30
           : kind == EffectKind::illusory || kind == EffectKind::petrified ? 1
           : kind == EffectKind::stoning ? 32
           : kind == EffectKind::contingency ? static_cast<int>(spell_table.size()) * 4
           : kind == EffectKind::polymorphed || kind == EffectKind::animal_shaped
           ? static_cast<int>(beast_forms.size())
           : kind == EffectKind::metamagic ? 15
           : kind == EffectKind::hidden || kind == EffectKind::aid ? 40
           : kind == EffectKind::circle_ward ? static_cast<int>(ward_types.size()) - 1
           : kind == EffectKind::heightened ? static_cast<int>(spell_table.size()) - 1
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
    case EffectKind::wild_shape:
    case EffectKind::animal_shaped:
    case EffectKind::invisible:
    case EffectKind::see_invisibility:
    case EffectKind::hidden:
    case EffectKind::free_moving:
    case EffectKind::energy_ward:
    case EffectKind::stoneskin:
    case EffectKind::true_seeing:
    case EffectKind::worded:
    case EffectKind::regenerating:
        return 3600000; // 1 hour
    case EffectKind::sheltered:
        return 28800000; // 8 hours
    case EffectKind::polymorphed:
    case EffectKind::summoned:
    case EffectKind::illusory:
    case EffectKind::jarred:
    case EffectKind::spirit_breath:
    case EffectKind::shared_resistance:
    case EffectKind::dragon_wings: // an hour
        return 3600000; // Concentration, up to 1 hour
    case EffectKind::aid:
    case EffectKind::mage_armor:
    case EffectKind::death_ward:
        return 28800000; // 8 hours
    case EffectKind::contagion:
        return 604800000; // 7 days
    case EffectKind::deceased:
        return 864000000; // 10 days
    case EffectKind::returned:
        return 345600000; // four Long Rests' worth, ended by them
    case EffectKind::drained:
    case EffectKind::dust:
    case EffectKind::simulacrum: // never counted down
    case EffectKind::petrified: // a minute, or for good once its spell lasted it
        return 4000000000u; // until Greater Restoration ends it
    case EffectKind::contingency:
        return 864000000; // 10 days
    case EffectKind::prayer_of_healing:
    case EffectKind::feasted:
    case EffectKind::raised:
    case EffectKind::relentless_used:
    case EffectKind::protectors_used:
    case EffectKind::overchanneled:
    case EffectKind::wings_used:
    case EffectKind::luck_used:
    case EffectKind::hurl_used:
    case EffectKind::arcanum_used:
    case EffectKind::resurged:
    case EffectKind::natural_cast:
    case EffectKind::trait_uses:
    case EffectKind::heroic_inspiration:
        return 86400000; // until a Long Rest, at most a day
    case EffectKind::heroism:
    case EffectKind::divine_favor:
    case EffectKind::bless:
    case EffectKind::turned:
    case EffectKind::bane:
    case EffectKind::resistance:
    case EffectKind::guidance:
    case EffectKind::blur:
    case EffectKind::mirror_image:
    case EffectKind::enlarged:
    case EffectKind::reduced:
    case EffectKind::dragons_breath:
    case EffectKind::innate_sorcery:
    case EffectKind::outlined:
    case EffectKind::shining:
    case EffectKind::banished:
    case EffectKind::circling:
    case EffectKind::fallen:
    case EffectKind::abjured:
    case EffectKind::hopeful:
    case EffectKind::shillelagh:
    case EffectKind::heated:
    case EffectKind::ensnaring:
    case EffectKind::dominated:
    case EffectKind::cursed:
    case EffectKind::animating:
    case EffectKind::dispel_evil:
    case EffectKind::knitting:
    case EffectKind::hasted:
    case EffectKind::entranced:
    case EffectKind::terrified:
    case EffectKind::vampiric:
    case EffectKind::blinking:
    case EffectKind::greater_invisible:
    case EffectKind::haunted:
    case EffectKind::sphered:
    case EffectKind::eye_panic:
    case EffectKind::sickened:
    case EffectKind::eye_resisted:
    case EffectKind::eyebiting:
    case EffectKind::stoning:
    case EffectKind::dancing:
    case EffectKind::aloft:
    case EffectKind::rising:
    case EffectKind::indigo:
    case EffectKind::violet:
    case EffectKind::word_stunned:
        return 60000; // 1 minute
    case EffectKind::mazed:
        return 600000; // Concentration, up to 10 minutes
    case EffectKind::mind_blank:
        return 86400000; // 24 hours
    case EffectKind::befuddled:
        return 2592000000u; // 30 days
    case EffectKind::produce_flame:
    case EffectKind::flame_blade:
    case EffectKind::fire_shield:
    case EffectKind::minor_elementals:
    case EffectKind::engulfed:
    case EffectKind::elemental_spirit:
    case EffectKind::telekinetic:
    case EffectKind::aura_of_life:
    case EffectKind::heightened: // the longest spell it can mark, but Web
        return 600000; // 10 minutes
    case EffectKind::metamagic:
    case EffectKind::overchannel:
    case EffectKind::defended:
    case EffectKind::pack_moved:
    case EffectKind::defending:
    case EffectKind::vitality:
        return 6000; // the rest of the turn, cleared when it ends
    case EffectKind::extended:
        return 1200000; // a doubled 10-minute Concentration
    case EffectKind::circle_ward:
        return 25200000; // 7 hours: 1, and 1 more for each slot level above 3
    default:
        return 0;
    }
}

void apply_spell_benefit(EffectState &effects, EffectSource source,
                         std::string name, EffectKind kind, int value)
{
    const auto duration = benefit_duration_ms(kind);
    if (!duration || !can_apply(effects) || !recorded(source, name) || value < 0 ||
            value > benefit_value_limit(kind))
        throw std::runtime_error("Invalid spell benefit");
    effects.active.push_back(
    {effects.next_id++, source.scope, source.actor, std::move(name), kind, value, duration, 0});
}

void apply_command(EffectState &effects, EffectSource source,
                   std::string name, CommandOption option, unsigned duration_ms)
{
    const int stored = static_cast<int>(option);
    if (!can_apply(effects) || !recorded(source, name) ||
            stored < 1 || stored > 4 || !duration_ms || duration_ms > 2 * round_ms)
        throw std::runtime_error("Invalid Command application");
    // A newer Command replaces an earlier one.
    std::erase_if(effects.active, [](const auto & e)
    {
        return e.kind == EffectKind::command;
    });
    effects.active.push_back({effects.next_id++, source.scope, source.actor, std::move(name),
                              EffectKind::command, stored, duration_ms, 0});
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
    // Aid and Heroes' Feast each add their most; Harm's lowering subtracts.
    int aid = 0, feast = 0, drained = 0;
    for (const auto &e : effects.active)
        if (e.kind == EffectKind::aid)
            aid = std::max(aid, e.dc);
        else if (e.kind == EffectKind::feasted)
            feast = std::max(feast, e.dc);
        else if (e.kind == EffectKind::drained || e.kind == EffectKind::simulacrum)
            drained += e.dc;
    return aid + feast - drained;
}

bool has_effect(const EffectState &effects, EffectKind kind) noexcept
{
    return std::any_of(effects.active.begin(), effects.active.end(),
                       [&](const auto & e)
    {
        return e.kind == kind;
    });
}

RollModifiers mental_save_modifiers(Ability ability, const EffectSubject &subject)
{
    auto modifiers = saving_modifiers(ability, subject.str_dex_disadvantage, subject.dodge);
    modifiers.advantage |= subject.mental_advantage && mental(ability);
    modifiers.lucky = subject.lucky;
    return modifiers;
}

RollModifiers saving_modifiers(Ability ability, bool armor, bool dodge)
{
    return {dodge && ability == Ability::dexterity,
            armor && (ability == Ability::strength || ability == Ability::dexterity)};
}

RollModifiers attack_modifiers(const AttackConditions &conditions)
{
    return {conditions.target_blind,
            conditions.attacker_blind || (conditions.target_dodging && !conditions.target_blind) ||
            conditions.other_disadvantage};
}

int inspiration_die(const std::vector<Effect> &effects, const SaveResult &failed,
                    std::uint64_t &rng)
{
    const auto found = std::find_if(effects.begin(), effects.end(), [](const auto & e)
    {
        return e.kind == EffectKind::inspired && e.remaining_ms;
    });
    if (found == effects.end() || found->dc < failed.dc - failed.natural - failed.bonus)
        return 0;
    return roll_die(rng, found->dc);
}

void spend_inspiration(EffectState &effects)
{
    const auto found = std::find_if(effects.active.begin(), effects.active.end(),
                                    [](const auto & e)
    {
        return e.kind == EffectKind::inspired;
    });
    if (found != effects.active.end())
        effects.active.erase(found);
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
                step = std::min(step, static_cast<std::uint64_t>(e.remaining_ms));
                if (e.save_in_ms)
                    step = std::min(step, static_cast<std::uint64_t>(e.save_in_ms));
            }
        if (!any)
            return;
        milliseconds -= step;
        for (auto &subject : ordered)
        {
            auto &effects = subject.effects.get().active;
            // Bardic Inspiration is spent after the walk, which must not erase.
            bool inspiration_spent = false;
            for (auto &e : effects)
            {
                // A simulacrum lasts until it is destroyed.
                if (e.kind != EffectKind::simulacrum)
                    e.remaining_ms -= static_cast<unsigned>(step);
                if (e.save_in_ms)
                    e.save_in_ms -= static_cast<unsigned>(step);
                // The effect as this step found it, for an event built only
                // when an observer hears of a save or a removal.
                const auto kind = e.kind;
                const auto remaining = e.remaining_ms, save_in = e.save_in_ms;
                std::optional<SaveResult> save;
                int inspiration = 0;
                bool removed = !e.remaining_ms;
                if (!removed &&
                        (e.kind == EffectKind::blindness || e.kind == EffectKind::hold_person ||
                         e.kind == EffectKind::drowsy || e.kind == EffectKind::laughing ||
                         e.kind == EffectKind::enfeebled || e.kind == EffectKind::frightened ||
                         e.kind == EffectKind::knocked_out || e.kind == EffectKind::confused ||
                         e.kind == EffectKind::time_slowed) &&
                        !e.save_in_ms)
                {
                    // Blindness, Ray of Enfeeblement and Knock Out repeat a Constitution save,
                    // the others a Wisdom one, at the end of each of the target's turns.
                    const auto ability =
                        e.kind == EffectKind::blindness || e.kind == EffectKind::enfeebled ||
                        e.kind == EffectKind::knocked_out
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
                        auto modifiers = mental_save_modifiers(ability, subject);
                        // Beacon of Hope: Advantage on Wisdom saves.
                        modifiers.advantage |= ability == Ability::wisdom &&
                                               has_effect(subject.effects.get(), EffectKind::hopeful);
                        if (e.kind == EffectKind::frightened)
                            modifiers.advantage |= subject.brave;
                        else if (e.kind != EffectKind::knocked_out)
                            modifiers.disadvantage |=
                                heightened_against(subject.effects.get(), e.source_scope,
                                                   e.source_actor, repeated_save_spell(e.kind));
                        modifiers.disadvantage |= spend_staggering(subject.effects.get());
                        save = saving_throw(ability,
                                                  subject.saves[static_cast<unsigned>(ability)] + bless,
                                                  e.dc, modifiers, rng);
                        if (!save->success && !inspiration_spent)
                            inspiration = inspiration_die(effects, *save, rng);
                        if (inspiration)
                        {
                            inspiration_spent = true;
                            save->success = save->natural + save->bonus + inspiration >= save->dc;
                        }
                        removed = save->success;
                        // A drowsy sleeper that fails falls Unconscious, no more saves.
                        if (!save->success && e.kind == EffectKind::drowsy)
                        {
                            e.kind = EffectKind::asleep;
                            e.save_in_ms = 0;
                            subject.effects.get().prone = true;
                        }
                    }
                }
                if (removed)
                    e.remaining_ms = 0;
                if (observe && (save || removed))
                {
                    EffectEvent event{subject.id, e, save, inspiration, removed};
                    event.effect.kind = kind;
                    event.effect.remaining_ms = remaining;
                    event.effect.save_in_ms = save_in;
                    observe(event);
                }
            }
            std::erase_if(effects,
                          [](const auto & e)
            {
                return !e.remaining_ms;
            });
            if (inspiration_spent)
                spend_inspiration(subject.effects.get());
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
        out << ' ' << e.id << ' ' << static_cast<unsigned>(e.kind) << ' ' << e.source_scope << ' '
            << e.source_actor << ' ' << std::quoted(e.source_name) << ' ' << e.dc << ' '
            << e.remaining_ms << ' ' << e.save_in_ms;
    out << ' ' << effects.prone;
}

namespace
{
// A saved effect kind, checked before it becomes an EffectKind so the reader
// compares kinds rather than casting numbers (Effective C++ Item 27).
// EffectKind::last is the last kind.
EffectKind saved_effect_kind(unsigned value)
{
    if (value > static_cast<unsigned>(EffectKind::last))
        throw std::runtime_error("Invalid active effect");
    return static_cast<EffectKind>(value);
}
} // namespace

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
        unsigned saved_kind{};
        unsigned_field(in, e.id);
        unsigned_field(in, saved_kind);
        unsigned_field(in, e.source_scope);
        unsigned_field(in, e.source_actor);
        in >> std::quoted(e.source_name) >> e.dc;
        unsigned_field(in, e.remaining_ms);
        unsigned_field(in, e.save_in_ms);
        const auto kind = saved_effect_kind(saved_kind);
        const bool timed = kind == EffectKind::ray_of_frost || kind == EffectKind::frosts_chill ||
                           kind == EffectKind::hamstrung || kind == EffectKind::stunned ||
                           kind == EffectKind::staggered || kind == EffectKind::studied ||
                           kind == EffectKind::staggering || kind == EffectKind::sundered ||
                           kind == EffectKind::dazed || kind == EffectKind::smite_protected ||
                           kind == EffectKind::faith_tested || kind == EffectKind::pack_tested ||
                           kind == EffectKind::beings_tested || kind == EffectKind::veiled ||
                           kind == EffectKind::guarded || kind == EffectKind::swarmed ||
                           kind == EffectKind::bladed || kind == EffectKind::bathed ||
                           kind == EffectKind::lethargic || kind == EffectKind::retching ||
                           kind == EffectKind::ethereal || kind == EffectKind::sleeted ||
                           kind == EffectKind::groped || kind == EffectKind::vitriol ||
                           kind == EffectKind::wall_burned || kind == EffectKind::breathed ||
                           kind == EffectKind::lifted || kind == EffectKind::hand_ready ||
                           kind == EffectKind::hurled || kind == EffectKind::fey_fright ||
                           kind == EffectKind::thorned || kind == EffectKind::swum ||
                           kind == EffectKind::clouded || kind == EffectKind::rooted ||
                           kind == EffectKind::jigging ||
                           kind == EffectKind::shocking_grasp ||
                           kind == EffectKind::chill_touch ||
                           kind == EffectKind::sap || kind == EffectKind::vex ||
                           kind == EffectKind::slow ||
                           kind == EffectKind::guiding_bolt ||
                           kind == EffectKind::poisoned ||
                           kind == EffectKind::dazzled ||
                           kind == EffectKind::shield ||
                           kind == EffectKind::acid_arrow ||
                           kind == EffectKind::raging ||
                           kind == EffectKind::reckless ||
                           kind == EffectKind::addled ||
                           kind == EffectKind::lit ||
                           kind == EffectKind::moonlit ||
                           kind == EffectKind::scorched;
        // Searing Smite, Ensnaring Strike and Entangle act at the start of the
        // target's turn or on its escape, not on a timer.
        const bool turn_save = kind == EffectKind::searing_smite ||
                               kind == EffectKind::ensnaring_strike ||
                               kind == EffectKind::entangle ||
                               kind == EffectKind::webbed || kind == EffectKind::tentacled ||
                               kind == EffectKind::sanctuary ||
                               kind == EffectKind::asleep;
        // A spell benefit has no save; `dc` carries its value.
        const auto benefit = benefit_duration_ms(kind);
        if (kind == EffectKind::command && in)
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
                    e.dc < 0 || e.dc > benefit_value_limit(kind) ||
                    !e.remaining_ms || e.remaining_ms > benefit ||
                    e.save_in_ms)
                throw std::runtime_error("Invalid active effect");
            e.kind = kind;
            previous = e.id;
            result.active.push_back(std::move(e));
            continue;
        }
        if (!in || (!timed && !turn_save && kind != EffectKind::blindness &&
                    kind != EffectKind::hold_person &&
                    kind != EffectKind::drowsy && kind != EffectKind::laughing &&
                    kind != EffectKind::enfeebled && kind != EffectKind::frightened &&
                    kind != EffectKind::knocked_out && kind != EffectKind::confused &&
                    kind != EffectKind::time_slowed) ||
                e.id <= previous || e.id >= result.next_id || !e.source_scope || !e.source_actor ||
                e.source_name.empty() || e.source_name.size() > 160 ||
                (!timed && (e.dc < -2 || e.dc > 38 || !e.remaining_ms || e.remaining_ms > 60000 ||
                            (turn_save ? e.save_in_ms != 0
                             : (!e.save_in_ms || e.save_in_ms > round_ms)))) ||
                (timed && ((e.dc != 0 && !((kind == EffectKind::reckless ||
                                             kind == EffectKind::staggering) && e.dc == 1)) ||
                           e.save_in_ms != 0 || !e.remaining_ms ||
                           e.remaining_ms > ((kind == EffectKind::chill_touch ||
                                              kind == EffectKind::vex ||
                                              kind == EffectKind::studied ||
                                              kind == EffectKind::dazed ||
                                              kind == EffectKind::guiding_bolt ||
                                              kind == EffectKind::poisoned ||
                                              kind == EffectKind::dazzled ||
                                              kind == EffectKind::acid_arrow ||
                                              kind == EffectKind::raging ||
                                              kind == EffectKind::lit ||
                                              kind == EffectKind::veiled ||
                                              kind == EffectKind::lethargic ||
                                              kind == EffectKind::vitriol ||
                                              kind == EffectKind::lifted ||
                                              kind == EffectKind::rooted ||
                                              kind == EffectKind::jigging ||
                                              kind == EffectKind::hurled)
                                             ? 2 * round_ms
                                             : round_ms))))
            throw std::runtime_error("Invalid active effect");
        e.kind = kind;
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
