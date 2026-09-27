#include "rest_activity.h"
#include <limits>
#include <stdexcept>

namespace opengold::srd5::rest
{
using namespace rules;

namespace
{
std::uint64_t add(std::uint64_t a, std::uint64_t b)
{
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        throw std::runtime_error("Rest clock overflow");
    return a + b;
}
} // namespace

RestPolicy policy(RestKind kind)
{
    switch (kind)
    {
    case RestKind::short_rest:
        return {60, 0};
    case RestKind::long_rest:
        return {480, 960, 360, 120, 60, 60};
    }
    throw std::runtime_error("Invalid rest kind");
}

RestProgress begin(RestKind kind)
{
    (void)policy(kind);
    RestProgress p;
    p.kind = kind;
    p.work = kind == RestKind::long_rest ? RestWork::sleep : RestWork::light_activity;
    return p;
}

std::uint64_t remaining(const RestProgress &p)
{
    const auto required =
        add(std::uint64_t(policy(p.kind).duration_minutes) * 60000, p.extension_milliseconds);
    if (p.elapsed_milliseconds > required)
        throw std::runtime_error("Invalid rest progress");
    return required - p.elapsed_milliseconds;
}

RestTransition interrupt(const RestProgress &before, RestInterruption cause)
{
    if (cause != RestInterruption::initiative && cause != RestInterruption::spell &&
            cause != RestInterruption::damage && cause != RestInterruption::exertion)
        throw std::runtime_error("Invalid rest interruption");
    if (before.interrupted)
        throw std::runtime_error("Rest is already interrupted");
    if (before.kind == RestKind::short_rest)
        return {};
    auto p = before;
    const auto timing = policy(p.kind);
    p.extension_milliseconds =
        add(p.extension_milliseconds, std::uint64_t(timing.interruption_extension_minutes) * 60000);
    (void)add(std::uint64_t(timing.duration_minutes) * 60000, p.extension_milliseconds);
    p.interrupted = true;
    p.interruption = cause;
    // Approved Q32 interpretation: previously credited time cannot earn benefits again.
    const auto benefit =
        p.segment_milliseconds >=
        std::uint64_t(policy(RestKind::short_rest).duration_minutes) * 60000
        ? RestBenefit::short_rest
        : RestBenefit::none;
    p.segment_milliseconds = 0;
    return {p, benefit};
}

RestTransition advance(const RestProgress &before, std::uint64_t milliseconds, RestWork work)
{
    if (!milliseconds || before.interrupted)
        throw std::runtime_error("Rest is not advancing");
    if (work != RestWork::sleep && work != RestWork::light_activity && work != RestWork::exertion)
        throw std::runtime_error("Invalid rest activity");
    auto p = before;
    const auto timing = policy(p.kind);
    p.work = work;
    if (work == RestWork::exertion)
    {
        if (p.kind == RestKind::short_rest)
            return interrupt(p, RestInterruption::exertion);
        const auto limit = std::uint64_t(timing.exertion_limit_minutes) * 60000;
        if (!limit || p.exertion_milliseconds >= limit ||
                milliseconds > limit - p.exertion_milliseconds)
            throw std::runtime_error("Advance only to the next rest interruption");
        p.exertion_milliseconds += milliseconds;
        if (p.exertion_milliseconds == limit)
            return interrupt(p, RestInterruption::exertion);
        return {p};
    }
    if (milliseconds > remaining(p))
        throw std::runtime_error("Advance only to rest completion");
    if (work == RestWork::light_activity)
    {
        p.light_milliseconds = add(p.light_milliseconds, milliseconds);
        if (p.kind == RestKind::long_rest &&
                p.light_milliseconds > std::uint64_t(timing.maximum_light_minutes) * 60000)
            throw std::runtime_error("Long Rest light activity limit exceeded");
    }
    else
        p.sleep_milliseconds = add(p.sleep_milliseconds, milliseconds);
    p.elapsed_milliseconds = add(p.elapsed_milliseconds, milliseconds);
    p.segment_milliseconds = add(p.segment_milliseconds, milliseconds);
    const auto required =
        add(std::uint64_t(timing.duration_minutes) * 60000, p.extension_milliseconds);
    if (p.elapsed_milliseconds < required)
        return {p};
    if (p.sleep_milliseconds < std::uint64_t(timing.minimum_sleep_minutes) * 60000)
        throw std::runtime_error("Long Rest requires more sleep");
    return {std::nullopt,
            p.kind == RestKind::short_rest ? RestBenefit::short_rest : RestBenefit::long_rest,
            required};
}

RestProgress resume(const RestProgress &before)
{
    if (!before.interrupted)
        throw std::runtime_error("Rest is not interrupted");
    auto p = before;
    p.interrupted = false;
    p.segment_milliseconds = 0;
    p.exertion_milliseconds = 0;
    p.work = RestWork::sleep;
    return p;
}

void validate(const RestProgress &p)
{
    const auto timing = policy(p.kind);
    const auto base = std::uint64_t(timing.duration_minutes) * 60000;
    if ((p.interrupted && (p.kind != RestKind::long_rest || p.segment_milliseconds)) || !base ||
            p.extension_milliseconds > std::numeric_limits<std::uint64_t>::max() - base ||
            p.elapsed_milliseconds >= base + p.extension_milliseconds ||
            (p.kind == RestKind::short_rest && (p.extension_milliseconds || p.exertion_milliseconds)) ||
            (p.kind == RestKind::long_rest &&
             (!timing.interruption_extension_minutes ||
              p.extension_milliseconds %
              (std::uint64_t(timing.interruption_extension_minutes) * 60000) ||
              (p.interrupted && !p.extension_milliseconds) ||
              (!p.extension_milliseconds && p.segment_milliseconds != p.elapsed_milliseconds) ||
              p.light_milliseconds > std::uint64_t(timing.maximum_light_minutes) * 60000 ||
              p.exertion_milliseconds > std::uint64_t(timing.exertion_limit_minutes) * 60000)))
        throw std::runtime_error("Rest activity disagrees with rules timing");
}
} // namespace opengold::srd5::rest
