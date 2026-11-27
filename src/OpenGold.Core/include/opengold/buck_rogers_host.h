#ifndef OPENGOLD_BUCK_ROGERS_HOST_H
#define OPENGOLD_BUCK_ROGERS_HOST_H

// The Buck Rogers games' script host (buck_rogers_host.cpp,
// buck_rogers_solar_map.cpp; docs/games/countdown.md, "Scripts"): the
// commands the party answers, the commands their table changed or added, the
// current character's record as their scripts read and write it, and the
// solar map. It reaches the tour through TourHost (tour_host.h), lent for
// each call.

#include "opengold/buck_rogers_solar_map.h"
#include "opengold/campaign_encounter.h"
#include "opengold/campaign_party.h"
#include "opengold/ecl_machine.h"
#include "opengold/formats.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
class EncounterHost;
class TourHost;

// The team's ship as zone 0 keeps it from 4D00h (types.pas TShip): its
// name and its six systems, each as it is now and whole, in the order
// of ship_systems.
struct ShipSystem
{
    unsigned now{}, whole{};
};

enum class ShipSystemKind : std::uint8_t
{
    hull,
    sensors,
    controls,
    life_support,
    fuel,
    engines
};

struct TeamShip
{
    std::string name;
    std::array<ShipSystem, 6> systems;

    [[nodiscard]] const ShipSystem &system(ShipSystemKind kind) const
    {
        return systems.at(static_cast<std::size_t>(kind));
    }
};

class BuckRogersHost
{
  public:
    // The commands the party answers, for a new machine or one a save
    // restores.
    void enable_buck_rogers_party_commands(TourHost &tour);
    // The current character's record as their scripts read it from 7C00h
    // (eclmenu.pas SpecialReadZ1), and as they change it (SpecialWriteZ1).
    void add_buck_rogers_record(const TourHost &tour, PartySlot slot, EclHostReply &reply) const;
    void read_buck_rogers_character(TourHost &tour);
    // The commands their table changed or added; false for the commands the
    // other games' host answers. `encounters` stages and fights their
    // monsters.
    [[nodiscard]] bool handle_buck_rogers_host(TourHost &tour, EncounterHost &encounters,
                                               const EclRequest &request);
    // Matrix's PROGRAM 2 quits after its closing scene. Format 55 preserves
    // its validated terminal instruction position as a victory receipt.
    [[nodiscard]] bool matrix_ending_completed(const TourHost &tour) const;
    // A new game's state (gameinit.pas InitializeGame, InitShipAndOrbit):
    // the difficulty, the area kind, the message speed and the team's ship,
    // the Maelstrom Rider, in zone 0 from 4D00h.
    void configure_buck_rogers(TourHost &tour);
    // The level the scripts read (C_Diff: Novice 0 to Ace 4, Admiral 5).
    [[nodiscard]] static std::uint16_t buck_rogers_difficulty_level(const TourHost &tour) noexcept;
    // The Buck Rogers team's combat icons (CHARS), one for each member in
    // party order (an empty image where the file lacks it), as its ground
    // fights and the ship-combat screen draw it.
    [[nodiscard]] static std::vector<CombatArt> buck_rogers_team_art(const TourHost &tour);

    // The solar map while snapshot().solar_map (docs/games/countdown.md,
    // "The solar map"): the bodies' phase and the rocket as zone 0 keeps
    // them. The rocket flies by explore() with a step in one of the eight
    // directions: the script's step entry spends the fuel, the rocket steps
    // (original::step_rocket), the team rests three days, and the script's
    // search entry finds the body it reached.
    [[nodiscard]] original::SolarMapState solar_map_state(const TourHost &tour) const;
    // The body the scripts found the rocket on (the planet cell, 4BA5h):
    // its number in the scripts' order, or none en route.
    [[nodiscard]] std::optional<std::size_t> orbited_body(const TourHost &tour) const;
    [[nodiscard]] TeamShip team_ship(const TourHost &tour) const;
    // Whether the solar map is on screen, from the area kind the scripts set
    // and the PICTURE MODE they give, `picture_mode_given` when one is given
    // (TourSnapshot::solar_map).
    void update_solar_map(TourHost &tour,
                          std::optional<std::uint16_t> picture_mode_given = std::nullopt);
    // The rocket's step the explore() command asked for, unless a script
    // holds it (moveBlocked, 7EC9h, at FFh), and the team's three days.
    void solar_map_step(TourHost &tour);
    // The solar map as the original draws it: the backdrop, the Sun, each
    // body at its place and the rocket.
    [[nodiscard]] Image render_solar_map(const TourHost &tour) const;

  private:
    void buck_rogers_damage(TourHost &tour, const EclRequest &request);
    // Writes the solar map's state into zone 0, and each body's cell from
    // 4B85h as the original's PaintOrbitMap does, for the scripts to compare.
    void keep_solar_map(TourHost &tour, const original::SolarMapState &state);

    // The area kind update_solar_map last saw; none in a new game or a
    // loaded save.
    std::optional<std::uint16_t> solar_area_kind_seen_;
};
} // namespace opengold::por
#endif
