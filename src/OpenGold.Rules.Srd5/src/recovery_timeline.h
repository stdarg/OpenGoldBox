#ifndef OPENGOLD_SRD5_RECOVERY_TIMELINE_H
#define OPENGOLD_SRD5_RECOVERY_TIMELINE_H
#include "life_cycle.h"
#include "status_effects.h"

namespace opengold::srd5::detail
{
// Borrowed views are valid for this synchronous elapsed-time operation only.
struct RecoverySubject
{
    EffectSubject effects;
    std::reference_wrapper<LifeState> life;
};
// Positive elapsed time rolls any Stable delay not yet rolled, once. Events
// are ordered by deadline, entity ID, mortality, then effect application ID.
// Combat continues to roll death saves at initiative entry instead.
enum class RecoveryMode
{
    campaign,
    combat
};
void elapse_recovery(std::span<RecoverySubject> subjects, std::uint64_t milliseconds,
                     std::uint64_t &rng, RecoveryMode mode = RecoveryMode::campaign,
                     const EffectObserver &observe = {});
} // namespace opengold::srd5::detail
#endif
