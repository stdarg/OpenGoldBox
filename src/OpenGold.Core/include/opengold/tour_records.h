#ifndef OPENGOLD_TOUR_RECORDS_H
#define OPENGOLD_TOUR_RECORDS_H

// The records a town session (rolf_tour.h) keeps of its own: where a Wand of
// Secrets points, the hidden entrances, the stage and trigger of an event,
// Word of Recall's sanctuary, a Teleport's arrival, and the checkpoint a
// failed event rolls back to.

#include "opengold/campaign_party.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/tour_snapshot.h"
#include "opengold/town_service_host.h"
#include "opengold/treasure_service.h"
#include "opengold/vault_service.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace opengold::por
{
// Where a Wand of Secrets points (SECRET-1): a trap on square (x, y), or a
// secret door in its `side` wall, the square on the door's side nearer the
// party.
struct WandSign
{
    unsigned x{}, y{};
    MapDirection side{};
    bool trap{};
};

// OpenGoldBox's shops with a hidden entrance (SECRET-1): each entrance's
// state by its shop's data/shops id (saved).
enum class EntranceState : std::uint8_t
{
    hidden,
    found, // a locked door
    open
};

// A Treasures picture drawn in a color record's colors (pic_frames): the
// picture, the color record and the frames, kept while both stay the same.
struct RecolouredPicture
{
    unsigned picture{}, colors{};
    std::vector<AnimationFrame> frames;
};

// Which script of an exploration event is running, so finish_event knows
// what follows it.
enum class EventStage : std::uint8_t
{
    none, // No event: the tour itself.
    step, // Before the party steps forward.
    search,
    area_entry,
    pre_camp,
    camp_interrupted
};

// What the player did to start an event. Each value is the original script
// entry slot that runs it.
enum class EventTrigger : std::uint8_t
{
    step = 0,
    look = 1,
    camp = 2
};

// Word of Recall: a square where the party used a temple of New Phlan, and
// the sanctuary it prepared there (saved).
struct Sanctuary
{
    unsigned script{};
    PartyPose pose;
    auto operator<=>(const Sanctuary &) const = default;
};

// Teleport's map and spot, waiting for the map's entry script to finish.
struct TeleportArrival
{
    unsigned map{};
    PartyPose pose;
};

// Everything an event can change, saved when it begins so a failed event
// rolls back as a whole rather than member by hand-paired member
// (Effective C++ Item 29).
struct EventCheckpoint
{
    EclMachine machine;
    TownParty party;
    std::optional<opengold::PartyState> campaign;
    unsigned script{}, area{};
    std::map<unsigned, MapKnowledge> visited_areas, seen_areas;
    std::map<std::string, EntranceState> entrances;
    std::set<unsigned> wand_traps;
    unsigned rest_checks{};
    unsigned travel_mode{};
    unsigned dark_queen_combat_set{};
    VaultService dark_queen_vault;
    GeoMap map;
    std::vector<PendingLoot> pending_loot;
    TourSnapshot snapshot;
    PartySlot selected_character{};
};
} // namespace opengold::por
#endif
