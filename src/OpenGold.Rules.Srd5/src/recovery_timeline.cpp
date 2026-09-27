#include "recovery_timeline.h"
#include <algorithm>
#include <stdexcept>

namespace opengold::srd5::detail
{
void elapse_recovery(std::span<RecoverySubject> subjects, std::uint64_t milliseconds,
                     std::uint64_t &rng, RecoveryMode mode, const EffectObserver &observe)
{
    std::vector<RecoverySubject> ordered(subjects.begin(), subjects.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const auto & a, const auto & b)
    {
        return a.effects.id < b.effects.id;
    });
    for (std::size_t i = 0; i < ordered.size(); ++i)
    {
        if (!ordered[i].effects.id || (i && ordered[i - 1].effects.id == ordered[i].effects.id))
            throw std::runtime_error("Invalid recovery participant identity");
        validate_recovery(ordered[i].life);
    }
    if (!milliseconds)
        return;
    for (auto &subject : ordered)
        start_stable_recovery(subject.life, rng);
    while (milliseconds)
    {
        auto step = milliseconds;
        bool any = false;
        for (const auto &subject : ordered)
        {
            const auto &life = subject.life.get();
            if (life.hp == 0 && !life.dead)
            {
                if (life.stable)
                {
                    any = true;
                    if (life.recovery.stable_recovery_due)
                    {
                        if (!healing_blocked(subject.effects.effects.get()))
                            step = 0;
                    }
                    else
                        step = std::min(step, std::uint64_t(life.recovery.stable_recovery_in_ms));
                }
                else if (mode == RecoveryMode::campaign)
                {
                    any = true;
                    step = std::min(step, std::uint64_t(life.recovery.death_save_in_ms));
                }
            }
            for (const auto &effect : subject.effects.effects.get().active)
            {
                any = true;
                step = std::min(step, std::uint64_t(effect.remaining_ms));
                if (effect.save_in_ms)
                    step = std::min(step, std::uint64_t(effect.save_in_ms));
            }
        }
        if (!any)
        {
            for (auto &subject : ordered)
                (void)advance_recovery_clock(subject.life, milliseconds);
            return;
        }
        milliseconds -= step;
        for (auto &subject : ordered)
        {
            auto &life = subject.life.get();
            const bool was_unconscious = life.hp == 0;
            (void)advance_recovery_clock(life, step,
                                         !healing_blocked(subject.effects.effects.get(), step));
            if (mode == RecoveryMode::campaign && life.hp == 0 && !life.dead && !life.stable &&
                    !life.recovery.death_save_in_ms)
                (void)death_save(life, rng, !healing_blocked(subject.effects.effects.get(), step));
            if (was_unconscious && life.hp > 0)
                subject.effects.effects.get().prone = true;
            // The chosen step reaches at most the first effect boundary, so
            // this shares the existing save/expiry rules without hiding an
            // earlier RNG event. Death at the same deadline suppresses its save.
            subject.effects.dead = life.dead;
            elapse_effects(std::span<EffectSubject>(&subject.effects, 1), step, rng, observe);
        }
    }
}
} // namespace opengold::srd5::detail
