#ifndef OPENGOLD_SRD5_COMBAT_GRID_H
#define OPENGOLD_SRD5_COMBAT_GRID_H

#include "opengold/rules.h"

namespace opengold::srd5::detail
{

void validate_battlefield(const rules::Battlefield &board);

using rules::has_line_of_sight;

struct Occupant
{
    rules::Cell cell;
    bool hostile{}, incapacitated{};
    // At 0 Hit Points. As in the original game, its square can be stopped on.
    bool downed{};
    rules::Footprint footprint;
};

class ReachableCells
{
  public:
    // The origin and spaces held by a creature not downed are transit cells,
    // never destinations.
    [[nodiscard]] std::optional<int> cost_to(rules::Cell destination) const;
    // Excludes the origin, includes the destination. Empty means no legal move.
    [[nodiscard]] std::vector<rules::Cell> path_to(rules::Cell destination) const;

  private:
    friend class MovementGrid;
    int width_{}, height_{}, origin_{};
    std::vector<int> costs_, previous_;
    std::vector<bool> destinations_;
};

// A value-owned snapshot keeps geometry and occupancy fixed during a search.
// Include living/unconscious actors; exclude corpses and the moving actor.
// Cells are the mover's top-left square; every square of its footprint moves
// with it and must be open, as the original moves a large creature (FOOT-1).
class MovementGrid
{
  public:
    // A flier crosses Difficult Terrain underfoot at no extra cost; an
    // incorporeal mover passes creatures and obstacles as Difficult Terrain,
    // never stopping in them.
    MovementGrid(const rules::Battlefield &board, rules::Cell origin,
                 std::span<const Occupant> occupants, bool crawling = false, bool flying = false,
                 bool incorporeal = false, rules::Footprint footprint = {});
    [[nodiscard]] bool can_stop_at(rules::Cell destination) const;
    [[nodiscard]] std::optional<int> step_cost(rules::Cell from, rules::Cell to) const;
    [[nodiscard]] ReachableCells reachable(int budget) const;

  private:
    enum class Occupancy
    {
        empty,
        ally,
        incapacitated_enemy,
        enemy
    };
    rules::Battlefield board_;
    rules::Cell origin_;
    bool crawling_{}, flying_{}, incorporeal_{};
    rules::Footprint footprint_;
    std::vector<Occupancy> occupancy_;
    std::vector<bool> held_; // A creature not downed stands there.
    [[nodiscard]] int index(rules::Cell cell) const;
    // One square of the footprint stepping from `from` to `to`.
    [[nodiscard]] std::optional<int> square_step_cost(rules::Cell from, rules::Cell to) const;
};

} // namespace opengold::srd5::detail
#endif
