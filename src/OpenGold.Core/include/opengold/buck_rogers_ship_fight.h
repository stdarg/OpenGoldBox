#ifndef OPENGOLD_BUCK_ROGERS_SHIP_FIGHT_H
#define OPENGOLD_BUCK_ROGERS_SHIP_FIGHT_H

#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_ship_combat.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/formats.h"
#include "opengold/rolf_tour.h"

#include <array>
#include <memory>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// What the Buck Rogers games' ship fight needs from the game's files and the
// campaign beyond its rules (buck_rogers_ship_combat.h): the ships and the
// pictures of SHIPS.DAX, the enemy's name, the start of the fight SHIP
// COMBAT asks for, and what follows its end (docs/games/countdown.md, "Ship
// combat"). The campaign session and the ship-combat screen's check both
// start a fight this one way.
namespace opengold::por
{

inline constexpr unsigned enemy_ship_types = 5;

// The ships and pictures of SHIPS.DAX a ship fight uses.
struct ShipFightFiles
{
    // Records 40 to 44, the enemy ships of types 1 to 5.
    std::array<buck_rogers::Ship, enemy_ship_types> enemy_ships;
    // The enemy ship of type n is drawn as picture n - 1 (saveload.pas
    // LoadShipData), over its stars.
    std::array<Image, enemy_ship_types> enemy_pictures;
    // The systems' icons, hull to engines: the team ship's (pictures 27 to
    // 32) and the enemy's smaller ones (33 to 38), as charhlp.pas
    // DrawShipSystems shows them.
    std::array<Image, buck_rogers::Ship::systems> party_system_icons, enemy_system_icons;
};

// Reads SHIPS.DAX's ships and pictures; throws std::runtime_error when one is
// missing or does not decode.
[[nodiscard]] ShipFightFiles read_ship_fight_files(const DaxDecodeResult &ships);

// The name a new enemy ship of `type` takes when it is the `count`th of its
// type the game has met, counting from 0 (saveload.pas LoadShipData): five
// names a type, round again with the count after them.
[[nodiscard]] std::string enemy_ship_name(unsigned type, unsigned count);

// The cell that counts the enemy ships of `type` the game has met (zone 0's
// shipNameCounts, 4D91h + type - 1).
[[nodiscard]] std::uint16_t enemy_ship_count_cell(unsigned type);

// The start of the ship fight `call` asks for: the team's ship, the enemy of
// its type named `enemy_name`, the team in party order and the player's
// options. Throws std::invalid_argument for a type out of range.
[[nodiscard]] original::ShipCombatStart
ship_combat_start(GoldBoxGame game, buck_rogers::Ship party_ship, const ShipFightFiles &files,
                  const ShipCombatCall &call, std::string enemy_name,
                  std::vector<original::TeamMember> crew, original::ShipCombatOptions options);

// What follows a ship fight's end (postcbt.pas PostCombat): the team
// destroyed when no member is left alive, else the wounded recover; unless
// the team boarded, the medics treat them and the crew repairs the ship
// (repair_ship), sharing the experience.
struct ShipFightAftermath
{
    original::ShipCombatEnd end;
    bool team_destroyed{};
    std::optional<original::TreatmentReport> treatment;
    std::optional<original::ShipRepairReport> repairs;
};

// Plays what follows `end` on its crew and ship, with the fight's options and
// dice. `enemy_ship` is the enemy as the fight left it, whose fuel the
// repairs take.
[[nodiscard]] ShipFightAftermath follow_ship_fight(GoldBoxGame game, original::ShipCombatEnd end,
                                                   const original::ShipCombatOptions &options,
                                                   RandomState &random);

// The script memory a ship fight's end writes (ship_combat_memory), as the
// script machine keeps it: the team's ship's numbers of two bytes and the
// saved enemy systems as words, the rest as bytes.
struct ShipMemoryCell
{
    std::uint16_t address{}, value{};
};

[[nodiscard]] std::vector<ShipMemoryCell>
ship_memory_cells(std::span<const original::ShipMemoryWrite> writes);

// What the session keeps of a ship fight: the fight SHIP COMBAT opened, with
// its options and dice, kept past its end (which the screen still shows)
// until the next SHIP COMBAT; what followed it; and after a boarding the
// captured ship and the experience still to share, which the repairs take
// once the fight on its decks sets post-combat mode 2 (postcbt.pas
// PostCombat, RepairShip).
struct ShipFightState
{
    struct Open
    {
        original::ShipCombatOptions options;
        std::shared_ptr<RandomState> dice;
        original::ShipCombat combat;
    };

    struct Boarded
    {
        original::ShipCombatOptions options;
        buck_rogers::Ship ship;
        int experience{};
    };

    std::optional<Open> open;
    std::optional<ShipFightAftermath> aftermath;
    std::optional<Boarded> boarded;
};

// The ship fight SHIP COMBAT waits on, while the session's phase is
// ship_combat (docs/games/countdown.md, "The ship combat screen"): the
// team's ship as zone 0 keeps it at 4D00h, the enemy of the call's type
// named as the next ship of its type, the team in party order. The first
// call opens it with the player's options; the ship-combat screen plays it
// by the orders it gives, then gives its end to the session
// (RolfTourSession::finish_ship_combat). Throws EclError when no ship fight
// waits.
original::ShipCombat &ship_fight(RolfTourSession &session,
                                 const original::ShipCombatOptions &options);
// SHIPS.DAX's ships and pictures, which the ship-combat screen draws; throws
// EclError for a game without them.
[[nodiscard]] const ShipFightFiles &ship_fight_files(const RolfTourSession &session);
// What followed the last ship fight, for the screen to report; null before
// its end.
[[nodiscard]] const ShipFightAftermath *ship_fight_aftermath(const RolfTourSession &session);

} // namespace opengold::por

#endif
