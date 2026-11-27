#ifndef OPENGOLD_TOWN_SERVICE_HOST_H
#define OPENGOLD_TOWN_SERVICE_HOST_H

// What a town service (hall_service.h and the others) reaches of the tour it
// serves. RolfTourSession implements it and lends itself to a service for one
// call at a time, so a service keeps no pointer into a tour that is moved or
// copied, and a test may serve a service from a host of its own.

#include "opengold/campaign_party.h"
#include "opengold/difficulty.h"
#include "opengold/coin_purse.h"
#include "opengold/ecl_machine.h"
#include "opengold/equipment.h"
#include "opengold/message.h"
#include "opengold/tour_snapshot.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace opengold::por
{
struct PhlanResources;

// The party of a tour without campaign rules (the host tests).
struct TownParty
{
    std::string name{"Fighter"};
    unsigned level{1}, hit_points{12}, max_hit_points{12};
    Purse wealth{0, 0, 0, 9999, 0, 0, 0};
    std::vector<Equipment> inventory;
};

class TownServiceHost
{
  public:
    // The game whose campaign this is, which its scripts belong to.
    [[nodiscard]] virtual GoldBoxGame game() const noexcept = 0;
    // The script machine whose requests the services answer.
    [[nodiscard]] virtual EclMachine &script_machine() noexcept = 0;
    [[nodiscard]] virtual const EclMachine &script_machine() const noexcept = 0;
    // The party; null for a tour without campaign rules (the host tests).
    [[nodiscard]] virtual CampaignParty *campaign() const noexcept = 0;
    // What the view shows, which a service changes as it serves.
    [[nodiscard]] virtual TourSnapshot &screen() noexcept = 0;
    // A ticket for the next choice the view offers.
    [[nodiscard]] virtual std::uint64_t new_ticket() noexcept = 0;
    // The member the script acts for, and a member's record as the script
    // reads it.
    [[nodiscard]] virtual PartySlot selected_character() const noexcept = 0;
    virtual void select_character(PartySlot slot) noexcept = 0;
    [[nodiscard]] virtual EclHostReply character_reply(PartySlot slot) const = 0;
    // Whether the party stands on a data/shops location: its map, by archive
    // and record, and one of its squares.
    [[nodiscard]] virtual bool
    standing_at(const std::string &archive, unsigned record,
                const std::vector<std::pair<unsigned, unsigned>> &markers) const = 0;
    // The difficulty the game is played at.
    [[nodiscard]] virtual Difficulty played_difficulty() const noexcept = 0;
    // Records a player's action in the game's log, with where the party stands.
    virtual void log_player(const std::string &action) const = 0;
    // The areas as the tour loaded them.
    [[nodiscard]] virtual const PhlanResources &town_resources() const = 0;
    // The party when there is no campaign().
    [[nodiscard]] virtual TownParty &town_party() noexcept = 0;
    // Each member's items, and auto-equip (EQUIP-1) for each weapon, armor,
    // shield or worn item a member gained since `before`.
    using HeldItems = std::vector<std::pair<opengold::MemberId, std::uint64_t>>;
    [[nodiscard]] virtual HeldItems held_items() const = 0;
    virtual void equip_gained(const HeldItems &before) = 0;
    // The leader deals with shopkeepers unless the event already asked who.
    virtual void choose_shopper() = 0;
    // Puts the party on the square it stepped into the shop from, outside the
    // door, facing away from the door (the user's request, 2026-11-09).
    virtual void step_out_of_shop() = 0;
    // The tour goes on, its script running.
    virtual void advance(double seconds) = 0;
    // The event cannot go on: it rolls back, or the tour stops.
    virtual void fail(std::string diagnostic) = 0;

    // A Pool of Radiance script cell where this campaign's game keeps it.
    [[nodiscard]] EclAddress game_cell(EclAddress pool_of_radiance_cell) const
    {
        return game_address(pool_of_radiance_cell, game());
    }

    // Shows our own menu, its dialogue and choices for the view to translate,
    // and waits for a choice.
    void show_host_menu(std::vector<rules::Message> dialogue, std::vector<rules::Message> choices);

  protected:
    TownServiceHost() = default;
    TownServiceHost(const TownServiceHost &) = default;
    TownServiceHost(TownServiceHost &&) = default;
    TownServiceHost &operator=(const TownServiceHost &) = default;
    TownServiceHost &operator=(TownServiceHost &&) = default;
    ~TownServiceHost() = default;
};
} // namespace opengold::por
#endif
