#ifndef OPENGOLD_COMBAT_GEOMETRY_H
#define OPENGOLD_COMBAT_GEOMETRY_H
#include "opengold/rules.h"
#include <vector>

// Square arithmetic over a fight's public state, shared by the combat adapter
// (combat_demo.cpp) and the computer player (combat_player.cpp). Nothing here
// rolls dice or changes a fight.
namespace opengold::combat_geometry
{
// The squares a footprint fills from its top-left square.
[[nodiscard]] std::vector<rules::Cell> squares_of(rules::Cell start, rules::Footprint footprint);
[[nodiscard]] std::vector<rules::Cell> squares_of(const rules::CombatantView &a);

// Squares from `cell` to the nearest square `a` fills: 1 when beside it.
[[nodiscard]] int squares_from(rules::Cell cell, const rules::CombatantView &a);

// Squares between the nearest squares two combatants fill: 1 when beside.
[[nodiscard]] int squares_apart(const rules::CombatantView &a, const rules::CombatantView &b);

// Whether any square `a` fills is one of `cells`.
[[nodiscard]] bool in_cells(const std::vector<rules::Cell> &cells, const rules::CombatantView &a);

// Whether any of `viewer`'s squares has a line of sight to any of `seen`'s.
[[nodiscard]] bool sees(const rules::Battlefield &board, const rules::CombatantView &viewer,
                        const rules::CombatantView &seen);
} // namespace opengold::combat_geometry
#endif
