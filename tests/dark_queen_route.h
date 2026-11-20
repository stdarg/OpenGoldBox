#ifndef OPENGOLD_TEST_DARK_QUEEN_ROUTE_H
#define OPENGOLD_TEST_DARK_QUEEN_ROUTE_H

#include "gold_box_route.h"

namespace opengold::test
{
enum class DarkQueenProgress
{
    none,
    opening,
    caergoth,
    caves,
    sea_elves,
    naulidis,
    safe_hall,
    hinge,
    audience,
    captain,
    air_chamber,
    air_guardians,
    eggs,
    eggs_returned,
    taladas,
    ezra,
    hizden,
    luminari,
    luminari_deep,
    fastillion,
    kristophan,
    tombs,
    oracle,
    ending
};

[[nodiscard]] bool dark_queen_scripts_share_map(unsigned area, unsigned from, unsigned to);
std::vector<RouteBeat> dark_queen_earned_route(DarkQueenProgress progress);
} // namespace opengold::test

#endif
