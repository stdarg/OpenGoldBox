#ifndef OPENGOLD_MOVEMENT_GRID_H
#define OPENGOLD_MOVEMENT_GRID_H

#include "opengold/rules.h"

// Movement on a battlefield, shared by every set of rules: which squares a
// creature can reach and by which path, for its rules' step costs.
namespace opengold::rules
{

void validate_battlefield(const rules::Battlefield &board);

// What a step costs. A step costs its base (straight or diagonal) times the
// ground's factor, plus the crawling extra while crawling. The defaults are
// the SRD rules' 5-foot squares: 5, 10 in Difficult Terrain, 20 overgrown.
struct StepCosts
{
    int straight{5}, diagonal{5};
    int difficult_factor{2}, overgrown_factor{4};
    int crawling_extra{5};
};

// Whose way of stepping a grid follows. The original games' (cbtmap.pas
// ProbeDirection, cbtacts.pas CombatMoveCreature) charge each square's own
// movement cost (Battlefield::move_cost), let a diagonal step pass a wall's
// corner, and let a creature larger than one square take a step its dearest
// square can pay for while charging only its anchor (top-left) square's cost.
// The standard way is the SRD rules', and the original rules' with a game's
// Original movement option off (docs/ORIGINAL-RULES.md).
enum class StepRules : std::uint8_t
{
    standard,
    original
};

// What a step asks of the mover: `need` must be left to take it and `charge`
// is spent. They differ only for a large creature under the original rules.
struct StepPrice
{
    int need{}, charge{};
    friend bool operator==(const StepPrice &, const StepPrice &) = default;
};

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
                 bool incorporeal = false, rules::Footprint footprint = {}, StepCosts costs = {},
                 StepRules step_rules = StepRules::standard);
    [[nodiscard]] bool can_stop_at(rules::Cell destination) const;
    // What the step charges.
    [[nodiscard]] std::optional<int> step_cost(rules::Cell from, rules::Cell to) const;
    [[nodiscard]] std::optional<StepPrice> step_price(rules::Cell from, rules::Cell to) const;
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
    StepCosts costs_;
    StepRules step_rules_;
    std::vector<Occupancy> occupancy_;
    std::vector<bool> held_; // A creature not downed stands there.
    [[nodiscard]] int index(rules::Cell cell) const;
    // One square of the footprint stepping from `from` to `to`.
    [[nodiscard]] std::optional<int> square_step_cost(rules::Cell from, rules::Cell to) const;
};

} // namespace opengold::rules
#endif
