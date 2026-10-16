#ifndef OPENGOLD_ENCOUNTER_FORMATION_H
#define OPENGOLD_ENCOUNTER_FORMATION_H
#include "opengold/dungeon_battlefield.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace opengold::por
{
// Starting squares in campaign order: party members, then enemies. Empty where
// the original engine found no room for that combatant.
struct EncounterFormation
{
    std::vector<std::optional<rules::Cell>> party, enemies;
};

// Where the original engine stood each side when a scripted fight began
// (PLACE-1 in docs/SRD-DECISIONS.md): two groups facing each other, the
// enemies `distance` exploration squares (0 to 2) ahead of a party facing
// `facing`. A group that does not fit its square spills behind it, then
// beside it, never around the other side.
[[nodiscard]] EncounterFormation encounter_formation(const DungeonBattlefield &field,
                                                     MapDirection facing, unsigned distance,
                                                     std::size_t party_size,
                                                     std::size_t enemy_count);
} // namespace opengold::por
#endif
