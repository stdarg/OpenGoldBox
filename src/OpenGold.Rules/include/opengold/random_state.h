#ifndef OPENGOLD_RANDOM_STATE_H
#define OPENGOLD_RANDOM_STATE_H

#include <cstdint>

namespace opengold
{
// The seeded dice's state, carried by the campaign and handed to the rules for
// every roll. A type of its own, so the campaign clock or another counter
// cannot be passed where the dice state belongs (Effective C++ Item 18).
struct RandomState
{
    std::uint64_t value{};
    bool operator==(const RandomState &) const = default;
};
} // namespace opengold

#endif
