#include "opengold/buck_rogers_ship_combat.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace opengold::original
{
namespace
{
using buck_rogers::Ship;

constexpr std::size_t system_count = Ship::systems;
constexpr std::size_t mount_count = 8;
constexpr std::uint8_t kind_mask = 0x7F;
// Kinds above it fire rounds; 1 and 2 are lasers.
constexpr int last_laser = 2;
// The range a fight opens at; an enemy beyond it is lost.
constexpr int scanner_range = 8;
// The team may BOOST ENGINES until two boosts have worked.
constexpr int most_party_boosts = 2;
// The base a gunner's total and the target's Armor Class come off for the
// d20 a shot needs, which is never above 20.
constexpr int to_hit_base = 60;
constexpr int hardest_to_hit = 20;
// The team's gunners' edge over the enemy's.
constexpr int party_gunnery_bonus = 3;
// A ship's crew count passes this many to each repair pass: one pass per ten
// crew or part of ten, the rest fire.
constexpr int crew_per_repair_pass = 10;
// The dying fall into a coma once they have been dying for more rounds.
constexpr int most_dying_rounds = 9;

constexpr std::array<ShipWeaponFacts, 8> weapon_table{{
    {6, 0, 10, 1, "Beam laser"},
    {4, 0, 20, 0, "Pumped laser"},
    {4, 50, 10, 1, "Gyrocannon"},
    {6, 4, 40, 0, "Missile mount"},
    {6, 4, 60, -1, "Heavy missile mount"},
    {4, 6, 100, -3, "K-cannon"},
    {2, 15, 20, -1, "Light acceleration gun"},
    {2, 10, 30, -2, "Heavy acceleration gun"},
}};

constexpr std::array<int, 7> hit_chances{28, 9, 9, 13, 10, 18, 13};
constexpr std::array<int, 5> experience_by_ship{300, 3000, 12600, 2500, 4600};
// The d10s a successful repair rolls for each system; fuel is never repaired.
constexpr std::array<int, system_count> repair_dice{20, 4, 4, 8, 0, 12};
constexpr std::array<int, 22> missile_adjustments{-6, -4, -3, -2, -1, 0, 0, 0, 0, 0, 0,
                                                  0,  0,  0,  0,  1,  2, 2, 3, 3, 4, 4};

void require_buck_rogers(GoldBoxGame game)
{
    if (game != GoldBoxGame::countdown_to_doomsday && game != GoldBoxGame::matrix_cubed)
    {
        throw std::invalid_argument("Ship combat belongs to the Buck Rogers games");
    }
}

[[nodiscard]] std::size_t index(ShipSystem system) noexcept
{
    return static_cast<std::size_t>(system);
}

[[nodiscard]] ShipSystem system_at(std::size_t index) noexcept
{
    return static_cast<ShipSystem>(index);
}

[[nodiscard]] int kind_of(const Ship::WeaponMount &mount) noexcept
{
    return mount.kind & kind_mask;
}

[[nodiscard]] const ShipWeaponFacts &facts(int kind)
{
    return weapon_table.at(static_cast<std::size_t>(kind - 1));
}

[[nodiscard]] bool fires_rounds(int kind) noexcept
{
    return kind > last_laser;
}

[[nodiscard]] int working_weapons(const Ship &ship)
{
    return std::accumulate(ship.weapons.begin(), ship.weapons.end(), 0,
                           [](int total, const Ship::WeaponMount &mount)
                           {
                               return total + mount.status;
                           });
}

[[nodiscard]] ShipSide other(ShipSide side) noexcept
{
    return side == ShipSide::party ? ShipSide::enemy : ShipSide::party;
}

[[nodiscard]] bool okay(const ShipCrewMember &member) noexcept
{
    return member.status == CrewStatus::okay;
}

// The repair skill each system takes (gdconsts.pas repairSkills); fuel has
// none.
[[nodiscard]] int repair_skill(const ShipCrewMember &member, ShipSystem system)
{
    switch (system)
    {
    case ShipSystem::hull:
        return member.repair_hull;
    case ShipSystem::sensors:
        return member.repair_electrical;
    case ShipSystem::controls:
        return member.repair_mechanical;
    case ShipSystem::life_support:
        return member.repair_life_support;
    case ShipSystem::engines:
        return member.repair_nuclear_engine;
    case ShipSystem::weapons:
        return member.repair_weapon;
    case ShipSystem::fuel:
        break;
    }
    return 0;
}

[[nodiscard]] int checked_die(const ShipDice &dice, int sides)
{
    const int result = dice(sides);
    if (result < 1 || result > sides)
    {
        throw std::logic_error("A die rolled off its faces");
    }
    return result;
}

} // namespace

int buck_rogers_missile_adjustment(int dexterity)
{
    const int clamped = std::clamp(dexterity, 1, static_cast<int>(missile_adjustments.size()));
    return missile_adjustments[static_cast<std::size_t>(clamped - 1)];
}

ShipWeaponFacts ship_weapon_facts(GoldBoxGame game, int kind)
{
    require_buck_rogers(game);
    if (kind < 1 || kind > static_cast<int>(weapon_table.size()))
    {
        throw std::invalid_argument("Ship weapon kinds run from 1 to 8");
    }
    return facts(kind);
}

int ship_system_hit_chance(GoldBoxGame game, ShipSystem system)
{
    require_buck_rogers(game);
    return hit_chances.at(index(system));
}

int ship_combat_experience(GoldBoxGame game, int enemy_ship)
{
    require_buck_rogers(game);
    if (enemy_ship < 1 || enemy_ship > static_cast<int>(experience_by_ship.size()))
    {
        throw std::invalid_argument("SHIP COMBAT's enemy ships run from 1 to 5");
    }
    return experience_by_ship[static_cast<std::size_t>(enemy_ship - 1)];
}

bool ship_can_move(const Ship &ship)
{
    return ship.system_status[index(ShipSystem::controls)] > 0 &&
           ship.system_status[index(ShipSystem::fuel)] > 0 &&
           ship.system_status[index(ShipSystem::engines)] > 0;
}

int ship_speed(const Ship &ship, int boosts)
{
    if (!ship_can_move(ship))
    {
        return 0;
    }
    int speed = ship.speed + boosts;
    const auto hull = index(ShipSystem::hull);
    const auto engines = index(ShipSystem::engines);
    if (ship.system_maximum[hull] / 2 > ship.system_status[hull])
    {
        speed /= 2;
    }
    if (ship.system_maximum[engines] / 4 > ship.system_status[engines])
    {
        speed -= 3;
    }
    else if (ship.system_maximum[engines] / 2 > ship.system_status[engines])
    {
        speed -= 1;
    }
    return std::max(speed, 1);
}

ShipCombat::ShipCombat(ShipCombatStart start, ShipDice dice)
    : game_(start.game), enemy_ship_number_(start.enemy_ship_number), dice_(std::move(dice)),
      crew_(std::move(start.crew))
{
    require_buck_rogers(game_);
    experience_ = ship_combat_experience(game_, enemy_ship_number_);
    if (start.enemy_pilot_skill < 0 || start.enemy_pilot_skill > 255 || start.enemy_to_hit < 0 ||
        start.enemy_to_hit > to_hit_base)
    {
        throw std::invalid_argument("SHIP COMBAT's pilot skill and to-hit number are out of range");
    }
    if (crew_.size() > 8)
    {
        throw std::invalid_argument("A team holds at most eight members");
    }
    if (!dice_)
    {
        throw std::invalid_argument("A ship combat needs dice");
    }

    state(ShipSide::party).ship = std::move(start.party_ship);
    auto &enemy = state(ShipSide::enemy).ship;
    enemy = std::move(start.enemy_ship);
    enemy.pilot_skill = static_cast<std::uint8_t>(start.enemy_pilot_skill);
    enemy.gunnery = static_cast<std::uint8_t>(to_hit_base - start.enemy_to_hit);

    // The first enabled member at the helm pilots; any later one marked so
    // stands down.
    for (std::size_t member = 0; member < crew_.size(); ++member)
    {
        crew_[member].dying_rounds = 0;
        if (!crew_[member].pilot)
        {
            continue;
        }
        if (pilot_)
        {
            crew_[member].pilot = false;
        }
        else if (crew_[member].enabled)
        {
            pilot_ = member;
        }
    }

    range_ = scanner_range;
    next_range_ = scanner_range;
    // An opposed d10 each, and the ships' reactions: the winning team lets
    // the enemy move first each round.
    const int party_roll = die(10);
    const int enemy_roll = die(10);
    const int contest = party_roll - enemy_roll + state(ShipSide::party).ship.reaction -
                        state(ShipSide::enemy).ship.reaction;
    enemy_first_ = contest > 0;
    emit({.kind = ShipEventKind::pilot_contest, .side = ShipSide::party, .success = enemy_first_});
    run();
}

bool ShipCombat::over() const noexcept
{
    return phase_ == Phase::over;
}

std::optional<std::size_t> ShipCombat::acting_member() const noexcept
{
    if (phase_ != Phase::member_turn)
    {
        return std::nullopt;
    }
    return member_;
}

bool ShipCombat::awaits_dodge() const noexcept
{
    return phase_ == Phase::dodge;
}

bool ShipCombat::awaits_round() const noexcept
{
    return phase_ == Phase::helpless;
}

int ShipCombat::range() const noexcept
{
    return range_;
}

const Ship &ShipCombat::ship(ShipSide side) const noexcept
{
    return state(side).ship;
}

int ShipCombat::boosts(ShipSide side) const noexcept
{
    return state(side).boosts;
}

std::span<const ShipCrewMember> ShipCombat::crew() const noexcept
{
    return crew_;
}

std::optional<std::size_t> ShipCombat::pilot() const noexcept
{
    return pilot_;
}

bool ShipCombat::enemy_moves_first() const noexcept
{
    return enemy_first_;
}

std::vector<ShipEvent> ShipCombat::take_events()
{
    return std::exchange(events_, {});
}

ShipCombatEnd ShipCombat::end() const
{
    if (!over())
    {
        throw std::logic_error("The ship combat is not over");
    }
    return {.outcome = outcome_,
            .boarded = boarded_,
            .experience = experience_,
            .repair_pending = !boarded_,
            .treatment_pending = !boarded_,
            .party_ship = state(ShipSide::party).ship,
            .enemy_ship = state(ShipSide::enemy).ship,
            .crew = crew_};
}

// The rounds: each the first side's turn, the second's, then the round's end,
// until the fight waits for the player or is over.
void ShipCombat::run()
{
    while (true)
    {
        if (stage_ == Stage::round_end)
        {
            end_round();
            if (combat_over_)
            {
                finish();
                return;
            }
            for (auto &ship_state : ships_)
            {
                ship_state.shots.fill(0);
            }
            stage_ = Stage::first_turn;
            turn_started_ = false;
            continue;
        }

        const bool party_turn = (stage_ == Stage::first_turn) != enemy_first_;
        if (!turn_started_)
        {
            turn_started_ = true;
            if (party_turn)
            {
                start_party_turn();
                if (phase_ == Phase::helpless)
                {
                    return;
                }
            }
            else if (!combat_over_)
            {
                start_enemy_turn();
                if (phase_ == Phase::dodge)
                {
                    return;
                }
            }
        }
        if (party_turn && !combat_over_ && find_acting_member())
        {
            phase_ = Phase::member_turn;
            return;
        }
        stage_ = stage_ == Stage::first_turn ? Stage::second_turn : Stage::round_end;
        turn_started_ = false;
    }
}

void ShipCombat::play_round()
{
    if (phase_ != Phase::helpless)
    {
        throw std::logic_error("The fight is not waiting for a round");
    }
    phase_ = Phase::member_turn;
    member_ = crew_.size();
    run();
}

void ShipCombat::start_party_turn()
{
    command_taken_ = false;
    party_can_maneuver_ = ship_can_move(state(ShipSide::party).ship);
    member_ = 0;
    if (!combat_over_ && std::none_of(crew_.begin(), crew_.end(), okay))
    {
        phase_ = Phase::helpless;
    }
}

// Members act in party order; one who fell earlier in the round is passed.
bool ShipCombat::find_acting_member()
{
    while (member_ < crew_.size() && !okay(crew_[member_]))
    {
        ++member_;
    }
    return member_ < crew_.size();
}

void ShipCombat::end_member_turn()
{
    ++member_;
}

std::vector<ShipOrder> ShipCombat::legal_orders() const
{
    if (phase_ != Phase::member_turn)
    {
        return {};
    }
    const std::size_t member = member_;
    const bool is_pilot = pilot_ == member;
    std::vector<ShipOrder> orders;
    if (best_weapon(ShipSide::party, !is_pilot))
    {
        orders.push_back({.action = ShipAction::fire});
        for (std::size_t system = 0; system <= index(ShipSystem::weapons); ++system)
        {
            orders.push_back({.action = ShipAction::target, .system = system_at(system)});
        }
    }
    if (!is_pilot)
    {
        for (std::size_t mount = 0; mount < mount_count; ++mount)
        {
            if (can_reload(ShipSide::party, mount))
            {
                orders.push_back({.action = ShipAction::load, .mount = static_cast<int>(mount)});
            }
        }
        const auto rigs = jury_rig_choices(member);
        orders.insert(orders.end(), rigs.begin(), rigs.end());
    }
    orders.push_back({.action = ShipAction::quit});

    const auto &party = state(ShipSide::party);
    if (is_pilot)
    {
        if (party_can_maneuver_)
        {
            if (range_ > 1)
            {
                orders.push_back({.action = ShipAction::close});
            }
            orders.push_back({.action = ShipAction::withdraw});
            if (range_ <= 1)
            {
                orders.push_back({.action = ShipAction::ram});
                if (speed(ShipSide::enemy) == 0)
                {
                    orders.push_back({.action = ShipAction::board});
                }
            }
        }
        return orders;
    }
    const auto &crew_member = crew_[member];
    if (!command_taken_)
    {
        orders.push_back({.action = ShipAction::command});
    }
    if (!patients(member).empty())
    {
        orders.push_back({.action = ShipAction::aid});
    }
    if (member_skill(member, crew_member.repair_nuclear_engine) > 0 &&
        party.ship.system_status[index(ShipSystem::engines)] > 0 &&
        party.boosts < most_party_boosts)
    {
        orders.push_back({.action = ShipAction::boost_engines});
    }
    if (party.ship.system_status[index(ShipSystem::sensors)] > 0 &&
        member_skill(member, crew_member.sensor_operation) > 0)
    {
        orders.push_back({.action = ShipAction::sensors});
    }
    return orders;
}

void ShipCombat::submit(const ShipOrder &order)
{
    const auto legal = legal_orders();
    if (std::find(legal.begin(), legal.end(), order) == legal.end())
    {
        throw std::invalid_argument("That order is not open to the acting member");
    }
    const std::size_t member = member_;
    bool ends_turn = true;
    switch (order.action)
    {
    case ShipAction::fire:
        fire_party(member, std::nullopt);
        break;
    case ShipAction::target:
        fire_party(member, order.system);
        break;
    case ShipAction::close:
        move(ShipSide::party, ShipEventKind::close, speed(ShipSide::party));
        party_can_maneuver_ = false;
        ends_turn = false;
        break;
    case ShipAction::withdraw:
        move(ShipSide::party, ShipEventKind::withdraw, speed(ShipSide::party));
        party_can_maneuver_ = false;
        ends_turn = false;
        break;
    case ShipAction::ram:
        ram(ShipSide::party, std::nullopt);
        break;
    case ShipAction::board:
        emit({.kind = ShipEventKind::board, .side = ShipSide::party, .member = member});
        boarded_ = true;
        outcome_ = 0;
        combat_over_ = true;
        break;
    case ShipAction::load:
        reload(ShipSide::party, static_cast<std::size_t>(order.mount), member);
        break;
    case ShipAction::jury_rig:
        jury_rig(ShipSide::party, member, order);
        break;
    case ShipAction::command:
        take_command(member);
        ends_turn = false;
        break;
    case ShipAction::aid:
        aid(member);
        break;
    case ShipAction::boost_engines:
        boost(ShipSide::party, member);
        break;
    case ShipAction::sensors:
    {
        // Sensor Operation at twice the skill (charhlp.pas SelectSkillCharacter,
        // difficulty 1).
        const bool success = percent(2 * member_skill(member, crew_[member].sensor_operation));
        emit({.kind = ShipEventKind::sensors,
              .side = ShipSide::party,
              .member = member,
              .success = success});
        break;
    }
    case ShipAction::quit:
        break;
    }
    if (ends_turn)
    {
        end_member_turn();
    }
    run();
}

void ShipCombat::answer_dodge(bool dodges)
{
    if (phase_ != Phase::dodge)
    {
        throw std::logic_error("No ram is coming");
    }
    phase_ = Phase::member_turn;
    ram(ShipSide::enemy, dodges);
    continue_enemy_turn();
    run();
}

// The enemy weighs the two ships, then rams, retreats or closes (shipcbt.pas
// EnemyTurnBody).
void ShipCombat::start_enemy_turn()
{
    const int party_value = combat_value(ShipSide::party);
    const int enemy_value = combat_value(ShipSide::enemy);
    if (enemy_value * 2 < party_value)
    {
        strategy_ = Strategy::flee;
    }
    else if (enemy_value > party_value)
    {
        strategy_ = Strategy::attack;
    }
    else
    {
        strategy_ = Strategy::hold;
    }

    const auto &enemy = state(ShipSide::enemy).ship;
    const auto &party = state(ShipSide::party).ship;
    if (ship_can_move(enemy))
    {
        const int enemy_hull = enemy.system_status[index(ShipSystem::hull)];
        const int enemy_speed = speed(ShipSide::enemy);
        if (range_ <= 1 && !enemy_fire_worthwhile() &&
            enemy_hull > party.system_status[index(ShipSystem::hull)] && enemy_hull > party.tonnage)
        {
            emit({.kind = ShipEventKind::ram_attempt, .side = ShipSide::enemy});
            if (speed(ShipSide::party) > 0)
            {
                phase_ = Phase::dodge;
                return;
            }
            ram(ShipSide::enemy, std::nullopt);
        }
        else if (strategy_ == Strategy::flee && enemy_can_maneuver_)
        {
            move(ShipSide::enemy, ShipEventKind::withdraw, enemy_speed);
        }
        else if (strategy_ == Strategy::hold && range_ <= 2)
        {
            move(ShipSide::enemy, ShipEventKind::withdraw, std::min(enemy_speed, 3));
        }
        else if (strategy_ == Strategy::attack || range_ >= 5)
        {
            move(ShipSide::enemy, ShipEventKind::close, enemy_speed);
        }
    }
    continue_enemy_turn();
}

// The enemy's pilot fires every pilot's weapon it can, its crew reload or
// fire one shot each, and a tenth of them boost the engines or jury-rig.
void ShipCombat::continue_enemy_turn()
{
    // A ship destroyed by its own ram fires no more; the original lets the
    // wreck fire once.
    if (combat_over_)
    {
        return;
    }
    const auto &enemy = state(ShipSide::enemy).ship;
    bool boost_pending = strategy_ == Strategy::flee && ship_can_move(enemy) &&
                         speed(ShipSide::enemy) < speed(ShipSide::party) && enemy_can_maneuver_ &&
                         percent(enemy.system_skill);

    burst_pending_ = false;
    burst_damage_ = 0;
    std::optional<std::size_t> last_mount;
    while (true)
    {
        const auto firing = best_weapon(ShipSide::enemy, false);
        if (last_mount && last_mount != firing)
        {
            report_enemy_burst();
        }
        if (firing)
        {
            fire(ShipSide::enemy, *firing, std::nullopt, std::nullopt);
        }
        last_mount = firing;
        if (!firing || combat_over_)
        {
            break;
        }
    }

    const int crew = std::max(enemy.crew, 0);
    int repair_passes = (crew + crew_per_repair_pass - 1) / crew_per_repair_pass;
    int fire_passes = crew - repair_passes;
    while (fire_passes > 0 && !combat_over_)
    {
        --fire_passes;
        std::optional<std::size_t> reloadable;
        for (std::size_t mount = 0; mount < mount_count; ++mount)
        {
            if (can_reload(ShipSide::enemy, mount))
            {
                reloadable = mount;
            }
        }
        if (reloadable)
        {
            reload(ShipSide::enemy, *reloadable, std::nullopt);
            continue;
        }
        const auto firing = best_weapon(ShipSide::enemy, true);
        if (last_mount && last_mount != firing)
        {
            report_enemy_burst();
        }
        if (firing)
        {
            fire(ShipSide::enemy, *firing, std::nullopt, std::nullopt);
        }
        else
        {
            repair_passes += fire_passes;
            fire_passes = 0;
        }
        last_mount = firing;
    }
    if (last_mount)
    {
        report_enemy_burst();
    }

    while (repair_passes > 0 && !combat_over_)
    {
        --repair_passes;
        if (boost_pending)
        {
            boost(ShipSide::enemy, std::nullopt);
            boost_pending = false;
        }
        else if (!enemy_jury_rig())
        {
            repair_passes = 0;
        }
    }
}

// The round's moves take effect at its end, and the dying count a round.
void ShipCombat::end_round()
{
    if (!combat_over_)
    {
        if (next_range_ < 0)
        {
            next_range_ = 0;
        }
        else if (next_range_ < 4)
        {
            enemy_can_maneuver_ = true;
        }
        else if (next_range_ > scanner_range)
        {
            emit({.kind = ShipEventKind::lost_from_scanners, .side = ShipSide::enemy});
            combat_over_ = true;
            experience_ = 0;
            outcome_ = 1;
        }
    }
    for (std::size_t member = 0; member < crew_.size(); ++member)
    {
        auto &crew_member = crew_[member];
        if (crew_member.status != CrewStatus::dying)
        {
            continue;
        }
        if (++crew_member.dying_rounds > most_dying_rounds)
        {
            crew_member.status = CrewStatus::comatose;
            emit({.kind = ShipEventKind::member_comatose,
                  .side = ShipSide::party,
                  .member = member,
                  .status = CrewStatus::comatose});
        }
    }
    range_ = next_range_;
}

void ShipCombat::finish()
{
    remove_jury_rigs();
    phase_ = Phase::over;
}

int ShipCombat::die(int sides)
{
    return checked_die(dice_, sides);
}

int ShipCombat::dice(int count, int sides)
{
    int total = 0;
    for (int roll = 0; roll < count; ++roll)
    {
        total += die(sides);
    }
    return total;
}

// A percentile check: a d100 at or under the chance. A chance below 1 never
// succeeds and one of 100 or more always does; the original keeps the
// chance in a byte, which wraps.
bool ShipCombat::percent(int chance)
{
    return die(100) <= chance;
}

int ShipCombat::member_skill(std::size_t member, int value) const
{
    return crew_[member].enabled ? value : 0;
}

ShipCombat::ShipState &ShipCombat::state(ShipSide side) noexcept
{
    return ships_[side == ShipSide::party ? 0 : 1];
}

const ShipCombat::ShipState &ShipCombat::state(ShipSide side) const noexcept
{
    return ships_[side == ShipSide::party ? 0 : 1];
}

int ShipCombat::speed(ShipSide side) const
{
    return ship_speed(state(side).ship, state(side).boosts);
}

ShipCombat::WeaponReadiness ShipCombat::readiness(ShipSide side, bool crew_fires,
                                                  std::size_t mount) const
{
    const auto &ship_state = state(side);
    const auto &weapon = ship_state.ship.weapons[mount];
    const int kind = kind_of(weapon);
    const bool crew_mount = (weapon.kind & ship_crew_mount) != 0;
    if (kind == 0)
    {
        return WeaponReadiness::empty;
    }
    if (crew_fires && !crew_mount)
    {
        return WeaponReadiness::pilot_only;
    }
    if (!crew_fires && crew_mount)
    {
        return WeaponReadiness::crew_only;
    }
    if (weapon.status == 0)
    {
        return WeaponReadiness::destroyed;
    }
    if (fires_rounds(kind) && weapon.ammunition == 0)
    {
        return WeaponReadiness::out_of_rounds;
    }
    if (ship_state.shots[mount] >= weapon.status)
    {
        return WeaponReadiness::fired_out;
    }
    if (range_ > facts(kind).maximum_range)
    {
        return WeaponReadiness::out_of_range;
    }
    return WeaponReadiness::ready;
}

// The ready mount that does the most damage, the first of equals.
std::optional<std::size_t> ShipCombat::best_weapon(ShipSide side, bool crew_fires) const
{
    std::optional<std::size_t> best;
    int best_damage = 0;
    for (std::size_t mount = 0; mount < mount_count; ++mount)
    {
        if (readiness(side, crew_fires, mount) != WeaponReadiness::ready)
        {
            continue;
        }
        const int damage = facts(kind_of(state(side).ship.weapons[mount])).damage;
        if (damage > best_damage)
        {
            best = mount;
            best_damage = damage;
        }
    }
    return best;
}

// The d20 a shot needs: 60 less the target's Armor Class and the gunner's
// total, at most 20 (shipcbt.pas TargetValue). The total is the gunner's
// combat value, Dexterity and a Rocketjock's level, or the enemy's gunnery;
// the team's gunners add 3. The weapon, the range, the controls and the
// target's maneuver change it.
int ShipCombat::target_number(ShipSide attacker, std::size_t mount,
                              std::optional<std::size_t> gunner) const
{
    const auto &shooter = state(attacker).ship;
    const auto &target = state(other(attacker)).ship;
    int total = shooter.gunnery;
    if (gunner)
    {
        const auto &member = crew_[*gunner];
        total = member.combat_value + buck_rogers_missile_adjustment(member.dexterity) +
                (member.rocketjock ? member.level : 0);
    }
    if (attacker == ShipSide::party)
    {
        total += party_gunnery_bonus;
    }
    total += facts(kind_of(shooter.weapons[mount])).to_hit_bonus;
    if (range_ > 4)
    {
        total -= 5;
    }
    else if (range_ > 2)
    {
        total -= 2;
    }
    if (shooter.system_status[index(ShipSystem::controls)] == 0)
    {
        total -= 5;
    }
    if (target.system_status[index(ShipSystem::controls)] == 0)
    {
        total += 3;
    }
    total += target.maneuver;
    return std::min(to_hit_base - target.armor_class - total, hardest_to_hit);
}

// A ship's fighting strength, which the enemy weighs: hull, mobility and
// weapons; 0 for an unarmed ship (shipcbt.pas ShipCombatValue).
int ShipCombat::combat_value(ShipSide side) const
{
    const auto &ship = state(side).ship;
    int base = ship.system_status[index(ShipSystem::hull)];
    if (ship_can_move(ship))
    {
        base += (ship.system_status[index(ShipSystem::controls)] +
                 ship.system_status[index(ShipSystem::fuel)] +
                 ship.system_status[index(ShipSystem::engines)]) /
                3;
    }
    int weapons = 0;
    for (const auto &mount : ship.weapons)
    {
        if (kind_of(mount) != 0)
        {
            weapons += facts(kind_of(mount)).damage * mount.status;
        }
    }
    return weapons == 0 ? 0 : base + weapons;
}

bool ShipCombat::can_reload(ShipSide side, std::size_t mount) const
{
    const auto &weapon = state(side).ship.weapons[mount];
    return fires_rounds(kind_of(weapon)) && weapon.reloads > 0 && weapon.ammunition == 0;
}

std::vector<ShipOrder> ShipCombat::jury_rig_choices(std::size_t member) const
{
    if (member_skill(member, crew_[member].jury_rig) <= 0)
    {
        return {};
    }
    const auto &party = state(ShipSide::party);
    std::vector<ShipOrder> choices;
    for (std::size_t mount = 0; mount < mount_count; ++mount)
    {
        const auto &weapon = party.ship.weapons[mount];
        if (weapon.status < weapon.maximum && party.weapon_rigs[mount] < weapon.maximum)
        {
            choices.push_back({.action = ShipAction::jury_rig,
                               .system = ShipSystem::weapons,
                               .mount = static_cast<int>(mount)});
        }
    }
    for (std::size_t system = 0; system < system_count; ++system)
    {
        if (system_at(system) != ShipSystem::fuel &&
            party.ship.system_status[system] < party.ship.system_maximum[system] &&
            party.system_rigs[system] == 0)
        {
            choices.push_back({.action = ShipAction::jury_rig, .system = system_at(system)});
        }
    }
    return choices;
}

// The members AID would treat: the dying, for a member with First Aid or
// Treat Light Wounds, and the stunned, for one with Treat Stun (viewchar.pas
// FindPatients).
std::vector<std::size_t> ShipCombat::patients(std::size_t aider) const
{
    const auto &medic = crew_[aider];
    const bool bandages = member_skill(aider, medic.first_aid) > 0 ||
                          member_skill(aider, medic.treat_light_wounds) > 0;
    const bool treats_stun = member_skill(aider, medic.treat_stun) > 0;
    std::vector<std::size_t> found;
    for (std::size_t member = 0; member < crew_.size(); ++member)
    {
        const auto &patient = crew_[member];
        if ((patient.status == CrewStatus::dying && bandages) ||
            (patient.enabled && patient.stunned && treats_stun))
        {
            found.push_back(member);
        }
    }
    return found;
}

// At range 1 or less the enemy fires rather than rams when its weapons'
// chance outweighs the team pilot's chance to dodge (shipcbt.pas
// EnemyFireWorthwhile).
bool ShipCombat::enemy_fire_worthwhile() const
{
    const auto chosen = best_weapon(ShipSide::enemy, false);
    if (!chosen)
    {
        return false;
    }
    // With no one at the helm there is no pilot's skill; the original reads
    // one from no one.
    const int pilot_value = pilot_ ? member_skill(*pilot_, crew_[*pilot_].pilot_rocket) : 0;
    const int dodge_score = pilot_value + (speed(ShipSide::party) - speed(ShipSide::enemy)) * 10;
    if (dodge_score >= 90)
    {
        return true;
    }
    const auto &enemy = state(ShipSide::enemy).ship;
    // The ready weapons' damage, which the original adds up in a byte and
    // reads as a signed one, so a pilot's weapons worth more than 127 count
    // as less than nothing.
    int weapon_power = 0;
    for (std::size_t mount = 0; mount < mount_count; ++mount)
    {
        if (readiness(ShipSide::enemy, false, mount) == WeaponReadiness::ready)
        {
            weapon_power +=
                enemy.weapons[mount].status * facts(kind_of(enemy.weapons[mount])).damage;
        }
    }
    const long long ram_weight = static_cast<long long>(100 - dodge_score) *
                                 (static_cast<std::uint16_t>(enemy.tonnage) / 10);
    const long long fire_weight =
        static_cast<long long>(21 - target_number(ShipSide::enemy, *chosen, std::nullopt)) * 5 *
        (weapon_power / 10);
    return ram_weight < fire_weight;
}

void ShipCombat::fire(ShipSide side, std::size_t mount, std::optional<std::size_t> gunner,
                      std::optional<ShipSystem> aim)
{
    auto &ship_state = state(side);
    const int kind = kind_of(ship_state.ship.weapons[mount]);
    // A shot aimed at a system first needs an even chance.
    const bool hit = (!aim || percent(50)) && die(20) >= target_number(side, mount, gunner);
    const int damage = hit ? facts(kind).damage : 0;
    emit({.kind = ShipEventKind::shot,
          .side = side,
          .member = gunner,
          .weapon_kind = kind,
          .mount = static_cast<int>(mount),
          .amount = damage,
          .system = aim.value_or(ShipSystem::hull),
          .success = hit});
    if (side == ShipSide::enemy)
    {
        burst_pending_ = true;
        burst_damage_ += damage;
    }
    if (hit)
    {
        strike(other(side), kind, aim);
    }
    if (fires_rounds(kind))
    {
        --ship_state.ship.weapons[mount].ammunition;
    }
    ++ship_state.shots[mount];
}

// The pilot fires every pilot's weapon it can; another member one shot of
// the crew's best.
void ShipCombat::fire_party(std::size_t member, std::optional<ShipSystem> aim)
{
    const bool crew_fires = pilot_ != member;
    while (true)
    {
        const auto mount = best_weapon(ShipSide::party, crew_fires);
        if (!mount)
        {
            return;
        }
        fire(ShipSide::party, *mount, member, aim);
        if (combat_over_ || pilot_ != member)
        {
            return;
        }
    }
}

// A hit lands on the aimed system, or on one systemHitChances picks
// (shipcbt.pas DispatchWeaponHit).
void ShipCombat::strike(ShipSide side, int kind, std::optional<ShipSystem> aim)
{
    const int damage = facts(kind).damage;
    if (aim)
    {
        damage_ship(side, *aim, damage);
        return;
    }
    int roll = die(100);
    for (std::size_t system = 0; system < hit_chances.size(); ++system)
    {
        roll -= hit_chances[system];
        if (roll <= 0)
        {
            damage_ship(side, system_at(system), damage);
            return;
        }
    }
}

// Takes `value` off a system, or one working weapon off a mount picked by
// its working weapons; a destroyed system's hits fall on the hull. A hull at
// 0 destroys the ship, and the team's crew with it; the team's life support
// at 0 hurts the crew once. Half the value is the chance the ship loses a
// crew member (shipcbt.pas DamageShip).
void ShipCombat::damage_ship(ShipSide side, ShipSystem system, int value)
{
    auto &ship = state(side).ship;
    bool exploding = false;
    if (system == ShipSystem::weapons)
    {
        const int total = working_weapons(ship);
        if (total <= 0)
        {
            return;
        }
        int pick = die(total);
        for (std::size_t mount = 0; mount < mount_count && pick > 0; ++mount)
        {
            pick -= ship.weapons[mount].status;
            if (pick < 1)
            {
                if (side == ShipSide::party)
                {
                    report_enemy_burst();
                }
                emit({.kind = ShipEventKind::weapon_destroyed,
                      .side = side,
                      .weapon_kind = kind_of(ship.weapons[mount]),
                      .mount = static_cast<int>(mount),
                      .system = ShipSystem::weapons});
                --ship.weapons[mount].status;
            }
        }
    }
    else
    {
        std::size_t hit = index(system);
        if (ship.system_status[hit] == 0)
        {
            hit = index(ShipSystem::hull);
        }
        value = std::min(value, ship.system_status[hit]);
        if (value <= 0)
        {
            return;
        }
        ship.system_status[hit] -= value;
        emit({.kind = ShipEventKind::system_damaged,
              .side = side,
              .amount = value,
              .system = system_at(hit)});
        if (ship.system_status[hit] == 0)
        {
            if (side == ShipSide::party)
            {
                report_enemy_burst();
            }
            emit({.kind = ShipEventKind::system_destroyed, .side = side, .system = system_at(hit)});
            if (system_at(hit) == ShipSystem::hull)
            {
                combat_over_ = true;
                outcome_ = side == ShipSide::enemy ? 0 : 1;
                exploding = side == ShipSide::party;
            }
            else if (system_at(hit) == ShipSystem::life_support && side == ShipSide::party &&
                     !life_support_failed_)
            {
                hurt_crew(100);
                value = 0;
                life_support_failed_ = true;
            }
        }
    }
    if (exploding)
    {
        explode_crew();
    }
    // The original passes half the value as a byte, so above 511 the chance
    // wraps round; here it stops at certain.
    if (percent(std::min(value / 2, 100)) && ship.crew > 0)
    {
        --ship.crew;
        emit({.kind = ShipEventKind::ship_crew_lost, .side = side});
    }
}

// The enemy's fire since its last change of mount has struck home: each
// okay member is hurt a fifth of its damage percent of the time
// (shipcbt.pas ReportEnemyDamage).
void ShipCombat::report_enemy_burst()
{
    if (!burst_pending_)
    {
        return;
    }
    burst_pending_ = false;
    const int chance = std::min(burst_damage_ / 5, 100);
    burst_damage_ = 0;
    hurt_crew(chance);
}

// Each okay member, `chance` percent of the time, takes 2 to 10 damage, or,
// when the chance is short of certain, leaps clear on its saving throw
// (shipcbt.pas DamageCrew).
void ShipCombat::hurt_crew(int chance)
{
    for (std::size_t member = 0; member < crew_.size(); ++member)
    {
        if (!okay(crew_[member]) || !percent(chance))
        {
            continue;
        }
        if (chance < 100)
        {
            const int save = die(20);
            if (save == 20 || (save != 1 && save >= crew_[member].blast_save))
            {
                emit({.kind = ShipEventKind::member_leaps_clear,
                      .side = ShipSide::party,
                      .member = member});
                continue;
            }
        }
        inflict(member, dice(1, 9) + 1);
    }
}

// The team's ship explodes: every member aboard dies. The original spares
// those already down, and Countdown's blast may leave a member alive in a
// coma.
void ShipCombat::explode_crew()
{
    for (std::size_t member = 0; member < crew_.size(); ++member)
    {
        auto &crew_member = crew_[member];
        if (crew_member.status == CrewStatus::dead)
        {
            continue;
        }
        crew_member.status = CrewStatus::dead;
        crew_member.hit_points = 0;
        crew_member.enabled = false;
        crew_member.pilot = false;
        emit({.kind = ShipEventKind::member_killed,
              .side = ShipSide::party,
              .member = member,
              .status = CrewStatus::dead});
    }
    pilot_.reset();
}

// Damage past 0 Hit Points: 1 to 9 dying, 10 to 29 comatose, 30 or more dead
// (charhlp.pas InflictDamage). A pilot who falls leaves the helm.
void ShipCombat::inflict(std::size_t member, int damage)
{
    auto &crew_member = crew_[member];
    const int overkill = std::max(damage - crew_member.hit_points, 0);
    crew_member.hit_points = std::max(crew_member.hit_points - damage, 0);
    if (overkill > 29)
    {
        crew_member.status = CrewStatus::dead;
    }
    else if (overkill > 9)
    {
        crew_member.status = CrewStatus::comatose;
    }
    else if (overkill > 0)
    {
        crew_member.status = CrewStatus::dying;
    }
    else if (crew_member.hit_points == 0)
    {
        crew_member.status = CrewStatus::unconscious;
    }
    if (crew_member.status != CrewStatus::okay)
    {
        crew_member.enabled = false;
        if (pilot_ == member)
        {
            crew_member.pilot = false;
            pilot_.reset();
            command_taken_ = false;
        }
    }
    emit({.kind = ShipEventKind::member_hurt,
          .side = ShipSide::party,
          .member = member,
          .amount = damage,
          .status = crew_member.status});
}

// Each hull takes the other ship's tonnage, unless the rammed ship, moving,
// dodges with its pilot's skill and ten for each point of speed it has over
// the rammer. The enemy tries when the rammer's hull is the stronger; the
// team's pilot as the player answers (shipcbt.pas RamAttempt). With no one
// at the helm the team's ship dodges by its own pilot value.
void ShipCombat::ram(ShipSide rammer, std::optional<bool> party_dodges)
{
    const ShipSide rammed = other(rammer);
    if (rammer == ShipSide::party)
    {
        emit({.kind = ShipEventKind::ram_attempt, .side = rammer});
    }
    bool dodged = false;
    if (speed(rammed) > 0)
    {
        const auto &rammed_ship = state(rammed).ship;
        const bool tries = rammed == ShipSide::enemy
                               ? rammed_ship.system_status[index(ShipSystem::hull)] <
                                     state(rammer).ship.system_status[index(ShipSystem::hull)]
                               : party_dodges.value_or(false);
        if (tries)
        {
            int skill = rammed_ship.pilot_skill;
            if (rammed == ShipSide::party && pilot_)
            {
                skill = member_skill(*pilot_, crew_[*pilot_].pilot_rocket);
            }
            dodged = percent(skill + (speed(rammed) - speed(rammer)) * 10);
        }
    }
    if (dodged)
    {
        emit({.kind = ShipEventKind::ram_dodged, .side = rammed});
        return;
    }
    emit({.kind = ShipEventKind::rammed, .side = rammer});
    damage_ship(rammer, ShipSystem::hull, state(rammed).ship.tonnage);
    damage_ship(rammed, ShipSystem::hull, state(rammer).ship.tonnage);
}

// CLOSE and WITHDRAW change the range at the round's end.
void ShipCombat::move(ShipSide side, ShipEventKind kind, int distance)
{
    emit({.kind = kind, .side = side, .amount = distance});
    next_range_ += kind == ShipEventKind::close ? -distance : distance;
}

// Repair Nuclear Engine at half the skill, or at half the enemy ship's own
// value: a success adds a point of speed, a failure costs the engines 1 to
// 100 (shipcbt.pas AttemptSpeedBoost).
void ShipCombat::boost(ShipSide side, std::optional<std::size_t> engineer)
{
    auto &ship_state = state(side);
    const int skill = engineer ? member_skill(*engineer, crew_[*engineer].repair_nuclear_engine)
                               : ship_state.ship.system_skill;
    const bool success = percent(skill / 2);
    const int damage = success ? 0 : die(100);
    if (success)
    {
        ++ship_state.boosts;
    }
    emit({.kind = ShipEventKind::boost, .side = side, .member = engineer, .success = success});
    if (damage > 0)
    {
        damage_ship(side, ShipSystem::engines, damage);
    }
}

// Jury Rig at the skill, or at the enemy ship's own value: a mount gains one
// working weapon, a system 2 to 20 points up to its whole strength, all
// taken away from the team's ship as the fight ends (shipcbt.pas
// AttemptJuryRig).
void ShipCombat::jury_rig(ShipSide side, std::optional<std::size_t> engineer,
                          const ShipOrder &choice)
{
    auto &ship_state = state(side);
    const int skill = engineer ? member_skill(*engineer, crew_[*engineer].jury_rig)
                               : ship_state.ship.system_skill;
    const bool success = percent(skill);
    int amount = 0;
    if (success && choice.system == ShipSystem::weapons)
    {
        const auto mount = static_cast<std::size_t>(choice.mount);
        ++ship_state.weapon_rigs[mount];
        ++ship_state.ship.weapons[mount].status;
        amount = 1;
    }
    else if (success)
    {
        const auto system = index(choice.system);
        amount = std::min(dice(2, 10), ship_state.ship.system_maximum[system] -
                                           ship_state.ship.system_status[system]);
        ship_state.ship.system_status[system] += amount;
        ship_state.system_rigs[system] += amount;
    }
    emit({.kind = ShipEventKind::jury_rig,
          .side = side,
          .member = engineer,
          .mount = choice.mount,
          .amount = amount,
          .system = choice.system,
          .success = success});
}

// The enemy mends its most damaging broken mount first, else the system
// worst off that it has not mended yet; false when nothing needs it.
bool ShipCombat::enemy_jury_rig()
{
    const auto &enemy = state(ShipSide::enemy);
    const auto &weapons = enemy.ship.weapons;
    const bool mount_open = std::any_of(
        weapons.begin(), weapons.end(),
        [&enemy, &weapons](const Ship::WeaponMount &weapon)
        {
            const auto mount = static_cast<std::size_t>(&weapon - weapons.data());
            return weapon.status < weapon.maximum && enemy.weapon_rigs[mount] < weapon.maximum;
        });
    std::optional<ShipOrder> choice;
    if (mount_open)
    {
        int best_damage = 0;
        for (std::size_t mount = 0; mount < mount_count; ++mount)
        {
            if (kind_of(weapons[mount]) != 0 && weapons[mount].status < weapons[mount].maximum &&
                facts(kind_of(weapons[mount])).damage > best_damage)
            {
                best_damage = facts(kind_of(weapons[mount])).damage;
                choice = ShipOrder{.action = ShipAction::jury_rig,
                                   .system = ShipSystem::weapons,
                                   .mount = static_cast<int>(mount)};
            }
        }
    }
    if (!choice)
    {
        int best_score = 0;
        for (std::size_t system = 0; system < system_count; ++system)
        {
            const int status = enemy.ship.system_status[system];
            const int maximum = enemy.ship.system_maximum[system];
            if (maximum <= 0 || status == maximum || enemy.system_rigs[system] != 0)
            {
                continue;
            }
            const int score = (10 - 10 * status / maximum + (status == 0 ? 5 : 0)) * 2;
            if (score > best_score)
            {
                best_score = score;
                choice = ShipOrder{.action = ShipAction::jury_rig, .system = system_at(system)};
            }
        }
    }
    if (!choice)
    {
        return false;
    }
    jury_rig(ShipSide::enemy, std::nullopt, *choice);
    return true;
}

void ShipCombat::reload(ShipSide side, std::size_t mount, std::optional<std::size_t> loader)
{
    auto &weapon = state(side).ship.weapons[mount];
    --weapon.reloads;
    weapon.ammunition =
        static_cast<std::uint8_t>(weapon.ammunition + facts(kind_of(weapon)).reload_rounds);
    emit({.kind = ShipEventKind::reload,
          .side = side,
          .member = loader,
          .weapon_kind = kind_of(weapon),
          .mount = static_cast<int>(mount)});
}

// AID bandages each dying patient, who falls unconscious, and treats each
// stunned one with Treat Stun (viewchar.pas TreatPatients). The original
// looks the patients up among the last ground fight's combatants, which a
// ship combat never sets; here they are the crew.
void ShipCombat::aid(std::size_t aider)
{
    const auto &medic = crew_[aider];
    const bool bandages = member_skill(aider, medic.first_aid) > 0 ||
                          member_skill(aider, medic.treat_light_wounds) > 0;
    for (const std::size_t member : patients(aider))
    {
        auto &patient = crew_[member];
        if (patient.status == CrewStatus::dying && bandages)
        {
            patient.status = CrewStatus::unconscious;
            patient.dying_rounds = 0;
            emit({.kind = ShipEventKind::bandage,
                  .side = ShipSide::party,
                  .member = member,
                  .success = true,
                  .status = CrewStatus::unconscious});
        }
        else if (patient.stunned)
        {
            const bool success = percent(member_skill(aider, medic.treat_stun));
            patient.stunned = !success;
            emit({.kind = ShipEventKind::treat_stun,
                  .side = ShipSide::party,
                  .member = member,
                  .success = success});
        }
    }
}

// COMMAND: the member takes the helm, once a round, and acts on as the
// pilot. The pilot it replaces stands down; the original leaves both marked,
// and the first of them in party order pilots in the next fight.
void ShipCombat::take_command(std::size_t member)
{
    if (pilot_)
    {
        crew_[*pilot_].pilot = false;
    }
    pilot_ = member;
    crew_[member].pilot = true;
    command_taken_ = true;
    emit({.kind = ShipEventKind::command, .side = ShipSide::party, .member = member});
}

// The team's jury-rigging lasts the fight only. The original takes a
// system's rigged points away only when they are fewer than its points, so
// a system rigged back from 0 stays mended; here every system loses them,
// down to 0, as the mounts do.
void ShipCombat::remove_jury_rigs()
{
    auto &party = state(ShipSide::party);
    for (std::size_t system = 0; system < system_count; ++system)
    {
        party.ship.system_status[system] =
            std::max(party.ship.system_status[system] - party.system_rigs[system], 0);
    }
    for (std::size_t mount = 0; mount < mount_count; ++mount)
    {
        auto &weapon = party.ship.weapons[mount];
        weapon.status = static_cast<std::uint8_t>(
            std::max(static_cast<int>(weapon.status) - party.weapon_rigs[mount], 0));
    }
}

void ShipCombat::emit(ShipEvent event)
{
    events_.push_back(event);
}

std::vector<ShipMemoryWrite> ship_combat_memory(const ShipCombatEnd &end)
{
    constexpr std::uint16_t treatment_skip_address = 0x4B80;
    constexpr std::uint16_t outcome_address = 0x4BBA;
    constexpr std::uint16_t party_ship_address = 0x4D00;
    constexpr std::uint16_t saved_systems_address = 0x4D6C;
    constexpr std::uint16_t saved_type_address = 0x4D78;
    constexpr std::uint16_t saved_crew_address = 0x4D79;
    constexpr std::uint16_t saved_gunnery_address = 0x4D7B;
    constexpr std::uint16_t post_combat_mode_address = 0x4D7C;
    constexpr std::uint16_t saved_weapons_address = 0x4D7D;
    constexpr std::uint16_t saved_weapon_count_address = 0x4D81;
    constexpr std::uint16_t saved_flag_address = 0x4D82;

    const auto party_ship = buck_rogers::encode_ship(end.party_ship);
    if (!party_ship)
    {
        throw std::logic_error("The team's ship no longer fits its record");
    }
    std::vector<ShipMemoryWrite> writes;
    const auto put = [&writes](std::size_t address, int value)
    {
        writes.push_back(
            {static_cast<std::uint16_t>(address), static_cast<std::uint8_t>(value & 0xFF)});
    };
    for (std::size_t offset = 0; offset < party_ship->size(); ++offset)
    {
        put(party_ship_address + offset, (*party_ship)[offset]);
    }
    put(treatment_skip_address, 0);
    put(outcome_address, end.outcome);
    put(post_combat_mode_address, end.boarded ? 1 : 0);
    if (end.experience == 0)
    {
        for (std::size_t offset = 0; offset < 2 * system_count; ++offset)
        {
            put(saved_systems_address + offset, 0);
        }
        for (std::size_t address = saved_weapons_address; address <= saved_flag_address; ++address)
        {
            put(address, 0);
        }
        return writes;
    }
    for (std::size_t system = 0; system < system_count; ++system)
    {
        const auto word = static_cast<std::uint16_t>(end.enemy_ship.system_status[system]);
        put(saved_systems_address + 2 * system, word & 0xFF);
        put(saved_systems_address + 2 * system + 1, word >> 8);
    }
    const auto crew = static_cast<std::uint16_t>(end.enemy_ship.crew);
    put(saved_type_address, end.enemy_ship.type);
    put(saved_crew_address, crew & 0xFF);
    put(saved_crew_address + 1, crew >> 8);
    put(saved_gunnery_address, end.enemy_ship.gunnery);
    put(saved_weapon_count_address, working_weapons(end.enemy_ship));
    put(saved_flag_address, 0);
    return writes;
}

ShipRepairReport repair_ship(GoldBoxGame game, Ship &party_ship,
                             std::span<const ShipCrewMember> crew, int experience, Ship *enemy_ship,
                             const ShipDice &dice)
{
    require_buck_rogers(game);
    // A repair skill at its own value (charhlp.pas SelectSkillCharacter,
    // difficulty 2).
    const auto skill_succeeds = [&dice](const ShipCrewMember &member, int value)
    {
        return checked_die(dice, 100) <= (member.enabled ? value : 0);
    };
    ShipRepairReport report;
    const auto enabled = std::count_if(crew.begin(), crew.end(),
                                       [](const ShipCrewMember &member)
                                       {
                                           return member.enabled;
                                       });
    if (enabled > 0)
    {
        report.experience_share = experience / static_cast<int>(enabled);
    }

    // Each damaged system but fuel: the first okay member whose repair skill
    // succeeds mends it by the system's d10s.
    for (std::size_t system = 0; system < system_count; ++system)
    {
        const ShipSystem repaired = system_at(system);
        if (repaired == ShipSystem::fuel ||
            party_ship.system_status[system] >= party_ship.system_maximum[system])
        {
            continue;
        }
        for (std::size_t member = 0; member < crew.size(); ++member)
        {
            if (!okay(crew[member]) ||
                !skill_succeeds(crew[member], repair_skill(crew[member], repaired)))
            {
                continue;
            }
            int amount = 0;
            for (int roll = 0; roll < repair_dice[system]; ++roll)
            {
                amount += checked_die(dice, 10);
            }
            amount = std::min(amount,
                              party_ship.system_maximum[system] - party_ship.system_status[system]);
            party_ship.system_status[system] += amount;
            report.repairs.push_back({.member = member, .system = repaired, .amount = amount});
            break;
        }
    }

    // Each broken weapon: the first okay member whose Repair Weapon succeeds
    // fixes it.
    for (std::size_t mount = 0; mount < mount_count; ++mount)
    {
        auto &weapon = party_ship.weapons[mount];
        for (int broken = weapon.maximum - weapon.status; broken > 0; --broken)
        {
            for (std::size_t member = 0; member < crew.size(); ++member)
            {
                if (!okay(crew[member]) ||
                    !skill_succeeds(crew[member], crew[member].repair_weapon))
                {
                    continue;
                }
                ++weapon.status;
                report.repairs.push_back({.member = member,
                                          .system = ShipSystem::weapons,
                                          .mount = static_cast<int>(mount),
                                          .amount = 1});
                break;
            }
        }
    }

    // Matrix Cubed reads the enemy's fuel when there is no enemy ship, as
    // after a boarding; Countdown takes none then, and so does OpenGoldBox.
    if (!enemy_ship)
    {
        return report;
    }
    const auto fuel = index(ShipSystem::fuel);
    if (report.experience_share != 0)
    {
        // Nine to fifteen sixteenths of the enemy's fuel, and 1 to 16 more.
        const int sixteenths = checked_die(dice, 4) + checked_die(dice, 4) + 7;
        int taken = (sixteenths * std::max(enemy_ship->system_status[fuel], 0)) >> 4;
        if (taken > 0)
        {
            taken += checked_die(dice, 16);
        }
        taken = std::min(
            taken, std::max(party_ship.system_maximum[fuel] - party_ship.system_status[fuel], 0));
        party_ship.system_status[fuel] += taken;
        report.fuel_taken = taken;
    }
    enemy_ship->system_status[fuel] = 0;
    return report;
}

} // namespace opengold::original
