#ifndef OPENGOLD_BUCK_ROGERS_SHIP_COMBAT_H
#define OPENGOLD_BUCK_ROGERS_SHIP_COMBAT_H

#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/gold_box_game.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Ship combat in the two Buck Rogers games, Countdown to Doomsday and Matrix
// Cubed (docs/games/countdown.md, "Ship combat"): the fight SHIP COMBAT (1Fh)
// starts (shipcbt.pas ShipCombat) and the repairs that follow it
// (postcbt.pas RepairShip). Every function takes the game and refuses any
// other. The two games play it alike once the bugs listed in the game docs
// are fixed.
namespace opengold::original
{

// One die of `sides` faces, 1 to `sides`: the fight's random numbers, which
// AID rolls with too.
using ShipDice = DieRoll;

// Dexterity's adjustment to a ranged attack, -6 at 1 to +4 at 21 and 22
// (gdconsts.pas dexMissileAdjustments); a gunner adds it to its combat value.
[[nodiscard]] int buck_rogers_missile_adjustment(int dexterity);

// A ship's six systems, in the record's order, and its weapon mounts, which a
// hit or a gunner's aim may also strike.
enum class ShipSystem : std::uint8_t
{
    hull,
    sensors,
    controls,
    life_support,
    fuel,
    engines,
    weapons
};

// What every mount of one weapon kind (1 to 8) shares (gdconsts.pas
// shipWeapons). Kinds 1 and 2 are lasers; the others fire rounds.
struct ShipWeaponFacts
{
    int maximum_range{}, reload_rounds{}, damage{}, to_hit_bonus{};
    std::string_view name;
};

// A mount's kind byte: the kind in the low seven bits, and this bit on a
// mount the crew fires rather than the pilot.
inline constexpr std::uint8_t ship_crew_mount = 0x80;

[[nodiscard]] ShipWeaponFacts ship_weapon_facts(GoldBoxGame game, int kind);
// The percent chance a hit not aimed at a system strikes `system`; the seven
// chances add up to 100 (gdconsts.pas systemHitChances).
[[nodiscard]] int ship_system_hit_chance(GoldBoxGame game, ShipSystem system);
// The experience beating enemy ship 1 to 5 is worth (shipcbt.pas ShipCombat).
[[nodiscard]] int ship_combat_experience(GoldBoxGame game, int enemy_ship);
// Whether controls, fuel and engines all work (charhlp.pas CanShipMove).
[[nodiscard]] bool ship_can_move(const buck_rogers::Ship &ship);
// The ship's speed with `boosts` successful engine boosts, slowed by a hull
// below half and engines below half or a quarter; 0 for a ship that cannot
// move, else at least 1 (charhlp.pas ShipSpeed).
[[nodiscard]] int ship_speed(const buck_rogers::Ship &ship, int boosts);

// How the fight plays where the user chose between the original and the
// Rule Book (docs/CONFIGURATION.md, the ship-combat options). The defaults are
// the Rule Book's, or OpenGoldBox's where the original has no good answer;
// each option set plays the original's way.
struct ShipCombatOptions
{
    // COMMAND is offered once a round even while the pilot is well, as in the
    // original; by default only while the pilot is dead or incapacitated, as
    // the Rule Book says.
    bool command_every_round{};
    // With no one at the helm the ship dodges a ram by its own pilot value
    // (100 for the Maelstrom Rider), as in the original; by default an empty
    // helm dodges with 0.
    bool empty_helm_dodges{};
    // After a boarding the repairs take no fuel from the captured ship, as in
    // the original; by default its fuel is transferred, as the Rule Book says.
    bool no_boarding_fuel{};
};

struct ShipCombatStart
{
    GoldBoxGame game{};
    // The team's ship, as kept at 4D00h.
    buck_rogers::Ship party_ship;
    // SHIPS.DAX record 39 + enemy_ship_number, as read.
    buck_rogers::Ship enemy_ship;
    // SHIP COMBAT's operands: the enemy ship, 1 to 5; the enemy pilot's
    // skill; the enemy's to-hit number, 0 to 60, lower being better.
    int enemy_ship_number{}, enemy_pilot_skill{}, enemy_to_hit{};
    // The team in party order, at most eight: what the fight reads of each
    // is its record (skills, Hit Points, status, combat value, Dexterity,
    // saving throws, career, level and the pilot mark) and its stun. The
    // first enabled member marked as pilot is at the helm.
    std::vector<TeamMember> crew;
    ShipCombatOptions options;
};

enum class ShipSide : std::uint8_t
{
    party,
    enemy
};

// What a crew member may do on its turn. The pilot may FIRE, TARGET, CLOSE,
// WITHDRAW, RAM and BOARD; the rest of the crew FIRE and TARGET with the
// crew's lasers, LOAD, JURY RIG, take COMMAND, AID, BOOST ENGINES and use
// SENSORS. Either may QUIT.
enum class ShipAction : std::uint8_t
{
    fire,
    target,
    close,
    withdraw,
    ram,
    board,
    load,
    jury_rig,
    command,
    aid,
    boost_engines,
    sensors,
    quit
};

struct ShipOrder
{
    ShipAction action{};
    // The system TARGET aims at, or JURY RIG mends (weapons with a mount).
    ShipSystem system{ShipSystem::hull};
    // The mount LOAD reloads, or JURY RIG mends; -1 for none.
    int mount{-1};

    friend bool operator==(const ShipOrder &, const ShipOrder &) = default;
};

enum class ShipEventKind : std::uint8_t
{
    // The opposed pilot roll as the fight opens; `success` when the team won,
    // which lets the enemy move first each round.
    pilot_contest,
    // A weapon fired: `success` on a hit, for `amount` damage.
    shot,
    // A system lost `amount` points; `system` is where it landed.
    system_damaged,
    system_destroyed,
    // A mount lost one of its working weapons.
    weapon_destroyed,
    // A ship lost a crew member to a hit (the ship's own crew count).
    ship_crew_lost,
    // A team member took `amount` damage; `status` is how it ended.
    member_hurt,
    member_leaps_clear,
    // The team's ship exploded around the member.
    member_killed,
    member_comatose,
    boost,
    jury_rig,
    reload,
    close,
    withdraw,
    ram_attempt,
    ram_dodged,
    rammed,
    board,
    command,
    // AID: a dying member bandaged, or a stunned one treated (`success`).
    bandage,
    treat_stun,
    sensors,
    // The range passed 8: the enemy ship is lost from the scanners.
    lost_from_scanners,
    // No team member could act and the enemy could neither finish the ship
    // nor get away: it breaks off, and the fight ends as if it were lost.
    breaks_off
};

struct ShipEvent
{
    ShipEventKind kind{};
    ShipSide side{};
    // The team member acting or affected, when one is.
    std::optional<std::size_t> member;
    int weapon_kind{}, mount{-1}, amount{};
    ShipSystem system{ShipSystem::hull};
    bool success{};
    BuckRogersStatus status{};
};

// How a ship combat ended, for the script and the steps after it.
struct ShipCombatEnd
{
    // 4BBAh: 0 the enemy ship destroyed or boarded, 1 the team's ship
    // destroyed or the enemy lost from the scanners or broken off.
    int outcome{};
    // The team boarded the enemy: post-combat mode 1 at 4D7Ch. The script
    // runs the fight on the enemy's decks; nothing after the fight runs here.
    bool boarded{};
    // The ship-combat experience still to share (repair_ship shares it); 0
    // once the enemy is lost from the scanners or broken off.
    int experience{};
    // Treatment and repair_ship follow every fight but a boarding.
    bool repair_pending{}, treatment_pending{};
    // The team's ship with the fight's jury-rigging taken away, and the
    // enemy's as the fight left it.
    buck_rogers::Ship party_ship, enemy_ship;
    std::vector<TeamMember> crew;
};

// One byte the end of a ship combat writes into script memory.
struct ShipMemoryWrite
{
    std::uint16_t address{};
    std::uint8_t value{};
};

// The script memory a ship combat's end writes: the outcome (4BBAh), the
// treatment flag (4B80h), the team's ship (4D00h), the enemy's systems, type,
// crew, gunnery and working weapons for the script's salvage (4D6Ch to 4D82h,
// cleared when the enemy got away) and the post-combat mode (4D7Ch).
[[nodiscard]] std::vector<ShipMemoryWrite> ship_combat_memory(const ShipCombatEnd &end);

// A ship combat, from SHIP COMBAT to its end. The fight opens at range 8 with
// the opposed pilot roll, then plays rounds: in each the team's members act
// one by one in party order, and the enemy plays its whole turn, the team
// first unless it won the pilot roll. The fight waits for a team member's
// order, for the answer to whether to dodge an enemy ram, or, when no member
// can act, for play_round. A helpless round in which the enemy did nothing
// that could change the fight ends it: the enemy breaks off.
class ShipCombat
{
  public:
    // Throws std::invalid_argument for another game or operands out of range.
    ShipCombat(ShipCombatStart start, ShipDice dice);

    [[nodiscard]] bool over() const noexcept;
    // The team member whose order the fight waits for.
    [[nodiscard]] std::optional<std::size_t> acting_member() const noexcept;
    // The enemy tries to ram a ship that can move: answer_dodge says whether
    // the pilot tries to dodge.
    [[nodiscard]] bool awaits_dodge() const noexcept;
    // No team member can act this round: play_round plays the enemy's turn.
    [[nodiscard]] bool awaits_round() const noexcept;

    // Every order the acting member may give, each TARGET, LOAD and JURY RIG
    // choice listed apart; empty while no member acts.
    [[nodiscard]] std::vector<ShipOrder> legal_orders() const;
    // Carries out one of legal_orders; throws std::invalid_argument for any
    // other. CLOSE, WITHDRAW and COMMAND leave the member acting; every other
    // order ends its turn.
    void submit(const ShipOrder &order);
    void answer_dodge(bool dodges);
    void play_round();

    // The range the round's moves, shots and orders use.
    [[nodiscard]] int range() const noexcept;
    [[nodiscard]] const buck_rogers::Ship &ship(ShipSide side) const noexcept;
    [[nodiscard]] int boosts(ShipSide side) const noexcept;
    [[nodiscard]] std::span<const TeamMember> crew() const noexcept;
    [[nodiscard]] std::optional<std::size_t> pilot() const noexcept;
    // The enemy moves first each round (the team won the pilot roll).
    [[nodiscard]] bool enemy_moves_first() const noexcept;
    // What happened since the last call.
    [[nodiscard]] std::vector<ShipEvent> take_events();
    // Throws std::logic_error before the fight is over.
    [[nodiscard]] ShipCombatEnd end() const;

  private:
    enum class Phase : std::uint8_t
    {
        member_turn,
        dodge,
        helpless,
        over
    };
    enum class Stage : std::uint8_t
    {
        first_turn,
        second_turn,
        round_end
    };
    enum class Strategy : std::uint8_t
    {
        flee,
        hold,
        attack
    };

    struct ShipState
    {
        buck_rogers::Ship ship;
        std::array<int, buck_rogers::Ship::systems> system_rigs{};
        std::array<int, 8> weapon_rigs{}, shots{};
        int boosts{};
    };

    enum class WeaponReadiness : std::uint8_t
    {
        ready,
        pilot_only,
        crew_only,
        destroyed,
        out_of_rounds,
        fired_out,
        out_of_range,
        empty
    };

    void run();
    void start_party_turn();
    [[nodiscard]] bool find_acting_member();
    void end_member_turn();
    void start_enemy_turn();
    void continue_enemy_turn();
    void end_round();
    void finish();

    [[nodiscard]] int die(int sides);
    [[nodiscard]] int dice(int count, int sides);
    [[nodiscard]] bool percent(int chance);
    [[nodiscard]] int member_skill(std::size_t member, Skill skill) const;
    [[nodiscard]] bool pilot_down() const;
    [[nodiscard]] int party_dodge_skill() const;

    [[nodiscard]] ShipState &state(ShipSide side) noexcept;
    [[nodiscard]] const ShipState &state(ShipSide side) const noexcept;
    [[nodiscard]] int speed(ShipSide side) const;
    [[nodiscard]] WeaponReadiness readiness(ShipSide side, bool crew_fires,
                                            std::size_t mount) const;
    [[nodiscard]] std::optional<std::size_t> best_weapon(ShipSide side, bool crew_fires) const;
    [[nodiscard]] int target_number(ShipSide attacker, std::size_t mount,
                                    std::optional<std::size_t> gunner) const;
    [[nodiscard]] int combat_value(ShipSide side) const;
    [[nodiscard]] bool can_reload(ShipSide side, std::size_t mount) const;
    [[nodiscard]] std::vector<ShipOrder> jury_rig_choices(std::size_t member) const;
    [[nodiscard]] bool enemy_fire_worthwhile() const;

    void fire(ShipSide side, std::size_t mount, std::optional<std::size_t> gunner,
              std::optional<ShipSystem> aim);
    void fire_party(std::size_t member, std::optional<ShipSystem> aim);
    void strike(ShipSide side, int kind, std::optional<ShipSystem> aim);
    void damage_ship(ShipSide side, ShipSystem system, int value);
    void report_enemy_burst();
    void hurt_crew(int chance);
    void explode_crew();
    void inflict(std::size_t member, int damage);
    void ram(ShipSide rammer, std::optional<bool> party_dodges);
    void move(ShipSide side, ShipEventKind kind, int distance);
    void boost(ShipSide side, std::optional<std::size_t> engineer);
    void jury_rig(ShipSide side, std::optional<std::size_t> engineer, const ShipOrder &choice);
    [[nodiscard]] bool enemy_jury_rig();
    void reload(ShipSide side, std::size_t mount, std::optional<std::size_t> loader);
    void aid(std::size_t aider);
    void take_command(std::size_t member);
    void note_enemy_progress() noexcept;
    void break_off();
    void remove_jury_rigs();
    void emit(ShipEvent event);

    GoldBoxGame game_;
    int enemy_ship_number_;
    ShipDice dice_;
    std::array<ShipState, 2> ships_;
    ShipCombatOptions options_;
    std::vector<TeamMember> crew_;
    // Rounds each member has spent dying in this fight; at the tenth it falls
    // into a coma (charhlp.pas ProcessDyingCharacters).
    std::vector<int> dying_rounds_;
    std::optional<std::size_t> pilot_;

    int range_{}, next_range_{};
    bool enemy_first_{};
    bool enemy_can_maneuver_{};
    bool life_support_failed_{};
    bool command_taken_{};
    bool party_can_maneuver_{};

    // A hull destroyed, the enemy boarded or lost: the round still ends.
    bool combat_over_{};
    Phase phase_{Phase::member_turn};
    Stage stage_{Stage::first_turn};
    bool turn_started_{};
    std::size_t member_{};
    Strategy strategy_{Strategy::hold};

    // The enemy's fire since its mount last changed: its damage tests the
    // crew (shipcbt.pas ReportEnemyDamage).
    bool burst_pending_{};
    int burst_damage_{};

    // The round play_round plays, with no member able to act, and whether
    // the enemy did anything in it that could change the fight.
    bool helpless_round_{};
    bool enemy_progress_{};

    int outcome_{};
    bool boarded_{};
    int experience_{};
    std::vector<ShipEvent> events_;
};

struct ShipRepair
{
    std::size_t member{};
    ShipSystem system{};
    // The mount fixed, for weapons; -1 otherwise.
    int mount{-1};
    // The points a system regained, or 1 for a weapon fixed.
    int amount{};
};

struct ShipRepairReport
{
    // The experience each enabled member gains.
    int experience_share{};
    std::vector<ShipRepair> repairs;
    int fuel_taken{};
};

// After a fight that did not end in boarding, and after the boarding's fight
// when the script asks (post-combat mode 2): shares `experience` among the
// enabled members, lets the okay members' repair skills mend each damaged
// system but fuel and each weapon, and, when experience was shared, takes
// fuel from the enemy ship and empties its tanks (postcbt.pas RepairShip).
// After a boarding `enemy_ship` is the captured ship as the fight's end saved
// it (ShipCombatEnd::enemy_ship, at 4D6Ch); its fuel is transferred unless
// `options` keep the original's none. `enemy_ship` is null when there is
// none.
[[nodiscard]] ShipRepairReport repair_ship(GoldBoxGame game, buck_rogers::Ship &party_ship,
                                           std::span<const TeamMember> crew, int experience,
                                           buck_rogers::Ship *enemy_ship, bool after_boarding,
                                           const ShipCombatOptions &options, const ShipDice &dice);

} // namespace opengold::original

#endif
