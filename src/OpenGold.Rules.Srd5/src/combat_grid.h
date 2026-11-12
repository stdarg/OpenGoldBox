#ifndef OPENGOLD_SRD5_COMBAT_GRID_H
#define OPENGOLD_SRD5_COMBAT_GRID_H

#include "opengold/movement_grid.h"

// The SRD rules move on the shared grid (opengold/movement_grid.h) with its
// default 5-foot step costs.
namespace opengold::srd5::detail
{
using rules::has_line_of_sight;
using rules::MovementGrid;
using rules::Occupant;
using rules::ReachableCells;
using rules::validate_battlefield;
} // namespace opengold::srd5::detail
#endif
