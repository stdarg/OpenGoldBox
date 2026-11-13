#ifndef OPENGOLD_CORE_UA_CENSUS_H
#define OPENGOLD_CORE_UA_CENSUS_H

#include <filesystem>

#include "opengold/game_census.h"

namespace opengold
{

// Unlimited Adventures' census: the files of its folder and of DISK1 to DISK3
// and ART, named with their folder ("DISK2/PICA.TLB").
[[nodiscard]] GameCensus take_ua_census(const std::filesystem::path &folder);

} // namespace opengold

#endif
