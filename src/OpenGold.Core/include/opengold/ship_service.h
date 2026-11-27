#ifndef OPENGOLD_SHIP_SERVICE_H
#define OPENGOLD_SHIP_SERVICE_H

#include "opengold/tour_snapshot.h"

#include <cstdint>
#include <memory>

namespace opengold::original
{
struct ShipCombatOptions;
class ShipCombat;
} // namespace opengold::original

namespace opengold::por
{
struct PhlanResources;
struct ShipFightState;
class TownServiceHost;

// A Buck Rogers script's SHIP COMBAT (buck_rogers_host.cpp): the script waits
// while the ship-combat screen plays the ship fight (buck_rogers_ship_fight.h),
// and goes on once the fight is over.
class ShipService
{
  public:
    // The script's SHIP COMBAT `call` waits on `request` for the ship fight.
    void begin(TownServiceHost &host, std::uint64_t request, ShipCombatCall call);
    // The ship fight that waits, opened with the player's `options` the first
    // time, the team's ship and the enemy's from the files of `town`.
    [[nodiscard]] original::ShipCombat &open(TownServiceHost &host, const PhlanResources *town,
                                             const original::ShipCombatOptions &options);
    // See RolfTourSession::finish_ship_combat.
    bool finish_ship_combat(TownServiceHost &host, std::uint64_t ticket);

    // The fight SHIP COMBAT opened, what followed it and a boarded ship;
    // null before the first SHIP COMBAT.
    [[nodiscard]] ShipFightState *state() noexcept
    {
        return ship_fight_.get();
    }

    [[nodiscard]] const ShipFightState *state() const noexcept
    {
        return ship_fight_.get();
    }

  private:
    // The SHIP COMBAT request the ship fight answers.
    std::uint64_t ship_combat_request_{};
    // Shared, as a copied session (a campaign's template) holds none.
    std::shared_ptr<ShipFightState> ship_fight_;
};
} // namespace opengold::por
#endif
