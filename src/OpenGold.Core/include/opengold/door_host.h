#ifndef OPENGOLD_DOOR_HOST_H
#define OPENGOLD_DOOR_HOST_H

// The doors the party meets in a 3D area (phlan_session.cpp): a locked door's
// menu of Bash, Pick and Knock, OpenGoldBox's shops behind hidden entrances,
// and the Wand of Secrets (SECRET-1). It reaches the tour through TourHost
// (tour_host.h), lent for each call.

#include "opengold/campaign_party.h"
#include "opengold/shop_data.h"
#include "opengold/tour_records.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace opengold::por
{
class TourHost;

class DoorHost
{
  public:
    // The Wand of Secrets (SECRET-1): while the party explores, `member` spends
    // a charge of the wand `item`, and the hidden secret door or armed trap
    // nearest the party within 60 feet, six of the map's ten-foot squares, is
    // found: the door shows, and the trap's script sees it as Find Traps
    // would. It returns where it is, or nothing when none is that near.
    // Throws, spending nothing, when the party is busy or the wand is empty.
    [[nodiscard]] std::optional<WandSign>
    point_wand_of_secrets(TourHost &tour, opengold::MemberId member, std::uint64_t item);
    [[nodiscard]] bool on_wand_trap(const TourHost &tour) const;

    [[nodiscard]] bool locked_door_ahead(const TourHost &tour) const;
    void show_locked_door(TourHost &tour);

    [[nodiscard]] bool menu_open() const noexcept
    {
        return door_menu_;
    }

    // The locked door's menu closes, and the way chosen, if not Exit, is tried.
    void choose(TourHost &tour, std::size_t choice);

    // The party moved: it may Pick a lock again.
    void party_moved() noexcept
    {
        pick_tried_ = false;
    }

    // Changes the loaded map for the hidden entrances on it: the walls that
    // close each shop's square, the square's original event cleared, and the
    // secret door as a wall, a locked door once found, or an open one.
    void apply_entrances(TourHost &tour);
    // After a step or a Look: the rolls to find a hidden secret door the
    // party stands outside of, and OpenGoldBox's shop on its square opening.
    void search_for_secret_door(TourHost &tour);

    // What an event's checkpoint keeps (EventCheckpoint), and the event
    // rolled back to it: no door menu is open.
    [[nodiscard]] const std::map<std::string, EntranceState> &entrances() const noexcept
    {
        return entrances_;
    }

    [[nodiscard]] const std::set<unsigned> &wand_traps() const noexcept
    {
        return wand_traps_;
    }

    void roll_back(std::map<std::string, EntranceState> entrances, std::set<unsigned> wand_traps);

    // A new run of the tour: every entrance hidden, no trap pointed out.
    void reset();

    // A save is read: no door menu is open, and the party may Pick again.
    void forget_menu() noexcept
    {
        door_menu_ = pick_tried_ = false;
        door_choices_.clear();
    }

    // Reads `doors`' entrances and wand traps from a campaign save, or writes
    // a const one (campaign_save.cpp).
    template <class Codec, class Doors> static void save(Codec &codec, Doors &doors);

  private:
    // Whether the lock ahead is a wizard lock (door code 3).
    [[nodiscard]] bool wizard_locked_ahead(const TourHost &tour) const;
    void try_locked_door(TourHost &tour, opengold::DoorMethod method);
    // The shop whose secret door is on the edge ahead of the party, if any.
    [[nodiscard]] const opengold::ShopSite *entrance_ahead(TourHost &tour) const;

    // Doors forced open stay open until the district map is reloaded; the
    // event checkpoint keeps the map as the event found it.
    bool door_menu_{};
    // As in the original, one Pick per lock until the party moves again.
    bool pick_tried_{};
    std::vector<opengold::DoorMethod> door_choices_;
    // Each hidden entrance's state by its shop's data/shops id (saved).
    std::map<std::string, EntranceState> entrances_;
    // The traps a Wand of Secrets pointed out (saved), as map * 256 + y * 16
    // + x: on such a square a script asking for Find Traps finds it.
    std::set<unsigned> wand_traps_;
};
} // namespace opengold::por
#endif
