#include "life_cycle.h"
#include "dice.h"
#include <algorithm>
#include <stdexcept>

namespace opengold::srd5::detail
{
void validate_temporary_hp(const rules::TemporaryHitPoints &pool)
{
    if (pool.amount < 0 || (pool.amount == 0) != pool.source_id.empty() ||
            pool.source_id.size() > 128 ||
            std::any_of(pool.source_id.begin(), pool.source_id.end(),
                        [](unsigned char c)
{
    return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == ':' || c == '/' || c == '_' ||
             c == '-' || c == '.');
    }))
    throw std::runtime_error("Invalid Temporary Hit Points");
}

void grant_temporary_hp(LifeState &state, const rules::TemporaryHitPoints &offered,
                        rules::TemporaryHpChoice choice)
{
    validate_temporary_hp(state.temporary_hp);
    validate_temporary_hp(offered);
    if (state.dead || !offered.amount)
        throw std::runtime_error(
            "Temporary Hit Points require a living recipient and a positive grant");
    if (choice == rules::TemporaryHpChoice::use_new)
        state.temporary_hp = offered;
    else if (choice != rules::TemporaryHpChoice::keep_current || !state.temporary_hp.amount)
        throw std::runtime_error("Invalid Temporary Hit Point choice");
}

unsigned encode_stable_recovery(const RecoveryClock &clock)
{
    return clock.stable_recovery_due ? 4 * recovery_hour_ms + 1 : clock.stable_recovery_in_ms;
}

void decode_stable_recovery(RecoveryClock &clock)
{
    clock.stable_recovery_due = clock.stable_recovery_in_ms == 4 * recovery_hour_ms + 1;
    if (clock.stable_recovery_due)
        clock.stable_recovery_in_ms = 0;
}

void validate_recovery(const LifeState &state)
{
    const auto &clock = state.recovery;
    if (clock.death_save_in_ms > death_turn_ms ||
            clock.stable_recovery_in_ms > 4 * recovery_hour_ms ||
            ((state.hp > 0 || state.dead) &&
             (clock.death_save_in_ms || clock.stable_recovery_in_ms || clock.stable_recovery_due)) ||
            (state.stable && clock.death_save_in_ms) ||
            (!state.stable && (clock.stable_recovery_in_ms || clock.stable_recovery_due)) ||
            (clock.stable_recovery_due && clock.stable_recovery_in_ms))
        throw std::runtime_error("Invalid recovery clock");
}

void initialize_legacy_recovery(LifeState &state)
{
    state.recovery = {};
    if (state.hp == 0 && !state.dead && !state.stable)
        state.recovery.death_save_in_ms = death_turn_ms;
}

void start_stable_recovery(LifeState &state, std::uint64_t &rng)
{
    if (state.hp == 0 && !state.dead && state.stable && !state.recovery.stable_recovery_in_ms &&
            !state.recovery.stable_recovery_due)
        state.recovery.stable_recovery_in_ms = unsigned(roll_die(rng, 4)) * recovery_hour_ms;
}

void stabilize(LifeState &state, std::uint64_t &rng)
{
    if (state.hp != 0 || state.dead)
        throw std::runtime_error("Stabilization requires a living creature at zero HP");
    state.stable = true;
    state.successes = state.failures = 0;
    state.recovery.death_save_in_ms = 0;
    start_stable_recovery(state, rng);
}

int death_save(LifeState &state, std::uint64_t &rng, bool can_heal)
{
    if (state.hp != 0 || state.dead || state.stable)
        throw std::runtime_error("Death save requires an unstable living creature at zero HP");
    const int natural = roll_die(rng, 20);
    state.recovery.death_save_in_ms = death_turn_ms;
    if (natural == 20 && can_heal)
    {
        state.hp = 1;
        state.successes = state.failures = 0;
        state.recovery = {};
    }
    else if (natural >= 10)
        ++state.successes;
    else
        state.failures += natural == 1 ? 2 : 1;
    if (state.failures >= 3)
    {
        state.dead = true;
        state.recovery = {};
    }
    else if (state.successes >= 3)
        stabilize(state, rng);
    return natural;
}

namespace
{
void apply_damage(LifeState &state, int damage, int hp_loss, int maximum_hp, bool critical,
                  bool dies_at_zero)
{
    const bool was_zero = state.hp == 0;
    // A buffer prevents HP loss, not taking damage. At zero HP the original
    // resolved damage still ends Stable, causes failures and may kill outright.
    const int remaining = was_zero ? damage : hp_loss - state.hp;
    state.hp = std::max(0, state.hp - hp_loss);
    if (state.hp)
        return;
    state.stable = false;
    state.recovery = {death_turn_ms, 0};
    if (was_zero)
        state.failures += critical ? 2 : 1;
    else
        state.successes = state.failures = 0;
    if (dies_at_zero || remaining >= maximum_hp || state.failures >= 3)
    {
        state.dead = true;
        state.recovery = {};
    }
}
} // namespace

void damage_life(LifeState &state, int amount, int maximum_hp, bool critical, bool dies_at_zero)
{
    if (amount < 0 || maximum_hp < 1 || state.hp < 0 || state.hp > maximum_hp)
        throw std::runtime_error("Invalid damage");
    validate_temporary_hp(state.temporary_hp);
    if (!amount || state.dead)
        return;
    const int absorbed = std::min(amount, state.temporary_hp.amount);
    state.temporary_hp.amount -= absorbed;
    if (!state.temporary_hp.amount)
        state.temporary_hp.source_id.clear();
    apply_damage(state, amount, amount - absorbed, maximum_hp, critical, dies_at_zero);
}

void set_life_hit_points(LifeState &state, int hp, int maximum_hp)
{
    if (hp < 0 || hp > maximum_hp || maximum_hp < 1 || state.hp < 0 || state.hp > maximum_hp ||
            (state.dead && hp))
        throw std::runtime_error("Invalid HP assignment");
    if (hp == state.hp)
        return;
    if (hp > state.hp)
        (void)heal_life(state, hp - state.hp, maximum_hp);
    else
        apply_damage(state, state.hp - hp, state.hp - hp, maximum_hp, false, false);
}

int heal_life(LifeState &state, int amount, int maximum_hp, bool can_heal)
{
    if (amount < 0 || maximum_hp < 1 || state.hp < 0 || state.hp > maximum_hp || state.dead)
        throw std::runtime_error("Invalid healing");
    const int healed = can_heal ? std::min(amount, maximum_hp - state.hp) : 0;
    if (healed)
    {
        state.hp += healed;
        state.successes = state.failures = 0;
        state.stable = false;
        state.recovery = {};
    }
    return healed;
}

bool advance_recovery_clock(LifeState &state, std::uint64_t milliseconds, bool can_heal)
{
    auto &clock = state.recovery;
    clock.death_save_in_ms -=
        static_cast<unsigned>(std::min<std::uint64_t>(milliseconds, clock.death_save_in_ms));
    if (clock.stable_recovery_in_ms)
    {
        if (milliseconds >= clock.stable_recovery_in_ms)
        {
            clock.stable_recovery_in_ms = 0;
            clock.stable_recovery_due = true;
        }
        else
            clock.stable_recovery_in_ms -= static_cast<unsigned>(milliseconds);
    }
    if (clock.stable_recovery_due && can_heal)
    {
        state.hp = 1;
        state.stable = false;
        state.successes = state.failures = 0;
        clock = {};
        return true;
    }
    return false;
}
} // namespace opengold::srd5::detail
