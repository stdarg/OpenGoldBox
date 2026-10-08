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
};

class ReachableCells
{
  public:
    // The origin and occupied spaces are transit cells, never destinations.
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
class MovementGrid
{
  public:
    MovementGrid(const rules::Battlefield &board, rules::Cell origin,
                 std::span<const Occupant> occupants, bool crawling = false);
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
    bool crawling_{};
    std::vector<Occupancy> occupancy_;
    [[nodiscard]] int index(rules::Cell cell) const;
};

} // namespace opengold::srd5::detail
#endif
