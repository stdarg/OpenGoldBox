#ifndef OPENGOLD_DEV_TOOLS_SEEDED_BATTLEFIELD_H
#define OPENGOLD_DEV_TOOLS_SEEDED_BATTLEFIELD_H
#include "opengold/encounter_formation.h"
#include <cstdint>
#include <optional>
#include <span>

// A fight's battlefield on a dungeon map, for the developer tools that stage
// fights (the Combat tester and the Party tester).
struct SeededBattlefield
{
    opengold::por::DungeonBattlefield field;
    opengold::por::MapDirection facing{};
    // Where the game's own placement stands each side on it.
    opengold::por::EncounterFormation formation;
};

// The first square and facing of `map`, in an order `seed` shuffles, where the
// party sees `distance` open squares ahead, as the game requires of a fight,
// the party and every enemy have a starting square that far apart, and every
// enemy, its whole size, can walk to the party. The same seed gives the same
// battlefield. None when no square fits.
[[nodiscard]] std::optional<SeededBattlefield>
seeded_battlefield(const opengold::por::GeoMap &map, std::uint64_t seed, unsigned distance,
                   std::size_t party_size, std::span<const opengold::rules::Footprint> enemies);

#endif
