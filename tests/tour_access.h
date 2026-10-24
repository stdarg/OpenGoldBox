#ifndef OPENGOLD_TESTS_TOUR_ACCESS_H
#define OPENGOLD_TESTS_TOUR_ACCESS_H

#include "opengold/rolf_tour.h"
#include <cstdint>
#include <string_view>

namespace opengold::test
{
// Test-only access to a town session: sets a script variable, such as a
// council commission's flag, that the test route cannot reach through the
// game, or reseeds the script's dice. The party must be idle between events.
struct TourAccess
{
    static void set_variable(por::RolfTourSession &town, por::EclAddress address,
                             std::uint16_t value)
    {
        town.machine_.bind_variable(address, value);
    }

    // Sets a script string, such as the training hall's class word.
    static void set_string(por::RolfTourSession &town, por::EclAddress address,
                           std::string_view value)
    {
        town.machine_.bind_string(address, value);
    }

    // A copy of the script machine, to try dice on without touching the session.
    static por::EclMachine machine(const por::RolfTourSession &town)
    {
        return town.machine_;
    }

    // Reseeds the script's dice, so a test can try an event on several rolls.
    static void seed_dice(por::RolfTourSession &town, std::uint32_t seed)
    {
        town.machine_.seed_random(seed);
    }
};
} // namespace opengold::test
#endif
