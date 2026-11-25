#include "archive_io.h"
#include "opengold/path_text.h"
#include "opengold/diagnostic_log.h"
#include "opengold/authored_items.h"
#include "opengold/combat_demo.h"
#include "combat_geometry.h"
#include "opengold/creature_art.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/character_pool.h"
#include "opengold/encounter_formation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <queue>
#include <set>
#include <span>
#include <stdexcept>

namespace opengold
{
using namespace rules;
using namespace por;
using namespace combat_geometry;

namespace
{
Battlefield arena()
{
    Battlefield b{12, 9, std::vector<Terrain>(108)};
    for (int y = 2; y <= 6; ++y)
    {
        if (y != 4)
        {
            b.terrain[y * 12 + 6] = Terrain::obstacle;
        }
    }
    b.terrain[4 * 12 + 5] = Terrain::difficult;
    b.terrain[4 * 12 + 6] = Terrain::difficult;
    return b;
}

std::vector<Participant> party()
{
    return {{1, "vanguard", "Vanguard", Side::party, {2, 2}},
            {2, "scout", "Scout", Side::party, {2, 4}},
            {3, "adept", "Adept", Side::party, {1, 3}},
            {4, "healer", "Healer", Side::party, {1, 5}}};
}

// The art of a creature of the Slums' bank (2), as any fight makes it.
std::optional<CombatArt> slums_bank_art(const std::filesystem::path &directory,
                                        const por::MonsterBundle *monsters, unsigned record,
                                        unsigned icon)
{
    for (const auto &file : std::filesystem::directory_iterator(directory))
    {
        auto name = por::detail::upper_ascii(path_text(file.path().filename()));
        if (name != "CPIC2.DAX")
        {
            continue;
        }
        const auto bytes = por::detail::read_archive_bytes<std::runtime_error>(
            file.path(), "Combat art archive exceeds limit", "Cannot read combat art",
            "Cannot read combat art");
        auto art = por::creature_art(bytes, icon,
                                     monsters ? monsters->creature(2, record) : nullptr, false);
        if (!art)
        {
            throw std::runtime_error("Cannot decode the script-selected combat icon");
        }
        return art;
    }
    return std::nullopt;
}

// The square of `free` nearest `target` whose footprint's squares are all
// free, if there is one.
std::optional<Cell> nearest_room(const std::vector<Cell> &free, Footprint footprint, Cell target)
{
    const auto distance = [&](Cell cell)
    {
        return std::abs(cell.x - target.x) + std::abs(cell.y - target.y);
    };
    const auto fits = [&](Cell start)
    {
        const auto squares = squares_of(start, footprint);
        return std::all_of(squares.begin(), squares.end(),
                           [&](Cell square)
                           {
                               return std::find(free.begin(), free.end(), square) != free.end();
                           });
    };
    std::optional<Cell> nearest;
    for (const auto cell : free)
    {
        if (fits(cell) && (!nearest || distance(cell) < distance(*nearest)))
        {
            nearest = cell;
        }
    }
    return nearest;
}

// Stands `participant` on `cell` and takes its footprint's squares from `free`.
void stand_on(std::vector<Cell> &free, Participant &participant, Cell cell)
{
    participant.cell = cell;
    for (const auto square : squares_of(cell, participant.footprint))
    {
        std::erase(free, square);
    }
}

// Stands `participant` on the square of `free` nearest `target` whose
// footprint's squares are all free, and takes them from `free`.
void stand_nearest(std::vector<Cell> &free, Participant &participant, Cell target)
{
    const auto nearest = nearest_room(free, participant.footprint, target);
    if (!nearest)
    {
        throw std::runtime_error("Original battlefield cannot fit the complete encounter");
    }
    stand_on(free, participant, *nearest);
}

// A monster it had no room for is left out of the fight, as there. A party
// member without room takes the free square of `reachable` nearest the side's
// first member (or `centre`); taken squares leave `reachable`.
void place_as_original(CampaignEncounter &encounter, std::vector<Participant> &party,
                       std::vector<Cell> &reachable, Cell centre)
{
    std::vector<Footprint> footprints;
    for (const auto &enemy : encounter.enemies)
    {
        footprints.push_back(enemy.footprint);
    }
    const auto formation =
        encounter.enemy_quarters.empty()
            ? encounter_formation(encounter.field, encounter.facing, encounter.distance,
                                  party.size(), footprints)
            : encounter_formation(encounter.field, encounter.facing, encounter.distance,
                                  party.size(), footprints, encounter.enemy_quarters);
    const auto claim =
        [&](std::span<Participant> side, const std::vector<std::optional<Cell>> &starts)
    {
        for (std::size_t n = 0; n < side.size(); ++n)
        {
            if (starts[n])
            {
                side[n].cell = *starts[n];
                for (const auto square : squares_of(*starts[n], side[n].footprint))
                {
                    std::erase(reachable, square);
                }
            }
        }
    };
    const auto fill_in =
        [&](std::span<Participant> side, const std::vector<std::optional<Cell>> &starts)
    {
        for (std::size_t n = 0; n < side.size(); ++n)
        {
            if (starts[n])
            {
                continue;
            }
            stand_nearest(reachable, side[n], starts.front().value_or(centre));
        }
    };
    // A monster dealt to a side of the party faces it from there
    // (combatst.pas, PlaceAllCreatures).
    for (std::size_t n = 0; n < encounter.enemy_quarters.size(); ++n)
    {
        encounter.enemies[n].facing =
            static_cast<int>(2 * index(reversed(encounter.enemy_quarters[n])));
    }
    // Every original square is claimed before any member falls back.
    claim(party, formation.party);
    claim(encounter.enemies, formation.enemies);
    fill_in(party, formation.party);
    // The original would start a fight with none of the band; the first monster
    // stays, standing like a party member without room.
    auto enemy_starts = formation.enemies;
    const bool no_monster_fits = std::none_of(enemy_starts.begin(), enemy_starts.end(),
                                              [](const auto &start)
                                              {
                                                  return start.has_value();
                                              });
    if (!enemy_starts.empty() && no_monster_fits)
    {
        fill_in(std::span(encounter.enemies).first(1), {std::nullopt});
        enemy_starts.front() = encounter.enemies.front().cell;
    }
    std::vector<Participant> fighting;
    for (std::size_t n = 0; n < encounter.enemies.size(); ++n)
    {
        if (enemy_starts[n])
        {
            fighting.push_back(std::move(encounter.enemies[n]));
        }
    }
    encounter.enemies = std::move(fighting);
}

std::string square(Cell cell)
{
    return std::to_string(cell.x) + "," + std::to_string(cell.y);
}

// "TROLL 2: melee at Bryn" or "Arden: move from 3,4 to 5,4": a command, and what
// its dice are for, in the log file.
std::string command_purpose(const Snapshot &state, const Command &command)
{
    const auto unit_of = [&](EntityId id)
    {
        const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                        [&](const auto &unit)
                                        {
                                            return unit.id == id;
                                        });
        return found == state.combatants.end() ? nullptr : &*found;
    };
    const auto *actor = unit_of(state.actor);
    auto purpose = (actor ? actor->name : std::string{}) + ": " + command.verb;
    if (const auto *target = unit_of(command.target))
    {
        purpose += " at " + target->name;
    }
    if (command.verb == "move" && actor)
    {
        purpose += " from " + square(actor->cell) + " to " + square(command.destination);
    }
    else if (!command.target && (command.destination.x || command.destination.y))
    {
        purpose += " at square " + square(command.destination);
    }
    return purpose;
}

// Who stands where as a fight begins, for the log file.
void log_fight_start(const Snapshot &state, const Identity &rules)
{
    log_diagnostic(LogPriority::info, "fight",
                   "rules " + rules.module + " " + rules.version + " (" + rules.content + ")");
    for (const auto &unit : state.combatants)
    {
        log_diagnostic(LogPriority::info, "fight",
                       unit.name + " (" + (unit.side == Side::party ? "party" : "opposition") +
                           ", " + unit.definition + ") at " + square(unit.cell) + ", HP " +
                           std::to_string(unit.hit_points) + "/" +
                           std::to_string(unit.max_hit_points) + ", AC " +
                           std::to_string(unit.armor_class));
    }
}

// A line of the fight's log as the file keeps it, the dice behind a damage total
// before the full stop.
std::string logged_line(const LogEntry &entry)
{
    auto line = entry.english;
    line.insert(line.ends_with('.') ? line.size() - 1 : line.size(), entry.message.detail);
    return line;
}

// Writes the lines `after` has beyond those `before` ended with. The fight keeps
// only its last 80 lines, so the oldest may have gone from its start.
void log_new_fight_lines(const std::vector<LogEntry> &before, const std::vector<LogEntry> &after)
{
    std::size_t kept = std::min(before.size(), after.size());
    for (; kept > 0; --kept)
    {
        if (std::equal(before.end() - static_cast<std::ptrdiff_t>(kept), before.end(),
                       after.begin(),
                       [](const LogEntry &old_line, const LogEntry &new_line)
                       {
                           return old_line.english == new_line.english;
                       }))
        {
            break;
        }
    }
    for (auto line = after.begin() + static_cast<std::ptrdiff_t>(kept); line != after.end(); ++line)
    {
        log_diagnostic(LogPriority::info, "fight", logged_line(*line));
    }
}
} // namespace

CombatDemo::CombatDemo(std::unique_ptr<RulesModule> module) : module_(std::move(module))
{
    if (!module_)
    {
        throw std::runtime_error("A combat rules module is required");
    }
}

CombatDemo::~CombatDemo() = default;

CombatDemo::CampaignCombat::CampaignCombat(std::shared_ptr<CampaignParty> party)
    : party_(std::move(party))
{
    party_->begin_combat();
}

CombatDemo::CampaignCombat::~CampaignCombat()
{
    if (party_)
    {
        party_->end_combat();
    }
}

void CombatDemo::campaign_party(std::shared_ptr<CampaignParty> party)
{
    if (combat_)
    {
        throw std::runtime_error("Attach party before starting combat");
    }
    campaign_ = std::move(party);
}

void CombatDemo::synchronize_party()
{
    if (!campaign_combat_)
    {
        return;
    }
    const auto state = combat_->snapshot();
    campaign_->apply_combat(state);
    finish_campaign_combat(state.outcome);
}

void CombatDemo::finish_campaign_combat(Outcome outcome)
{
    if (outcome == Outcome::ongoing)
    {
        return;
    }
    campaign_combat_.reset();
    if (!keep_quick_)
    {
        campaign_->take_control();
    }
    if (outcome == Outcome::victory && !reward_id_.empty())
    {
        campaign_->award_experience(300, reward_id_);
        reward_id_.clear();
    }
}

void CombatDemo::install_combat(std::unique_ptr<CombatSession> next, std::string reward_id)
{
    if (!next)
    {
        throw std::runtime_error("Rules module returned no combat session");
    }
    if (diagnostic_log_enabled())
    {
        log_fight_start(next->snapshot(), module_->identity());
        log_new_fight_lines({}, next->snapshot().log_entries);
    }
    if (!campaign_)
    {
        combat_ = std::move(next);
        return;
    }
    if (campaign_->identity() != module_->identity())
    {
        throw std::runtime_error("Party and combat rules differ");
    }
    // Failed snapshot validation releases the edit lock and leaves the previous
    // session intact. Only a successful handoff transfers the lock to this owner.
    CampaignCombat ownership(campaign_);
    const auto state = next->snapshot();
    campaign_->apply_combat(state);
    combat_ = std::move(next);
    campaign_combat_.emplace(std::move(ownership));
    reward_id_ = std::move(reward_id);
    finish_campaign_combat(state.outcome);
}

void CombatDemo::start_encounter(std::vector<Participant> enemies, std::string reward_id)
{
    auto participants = campaign_ ? campaign_->participants() : party();
    participants.insert(participants.end(), std::make_move_iterator(enemies.begin()),
                        std::make_move_iterator(enemies.end()));
    const RollPurpose initiative("initiative");
    auto next = module_->create(
        {arena(), std::move(participants), campaign_ ? campaign_->state().next_combat_scope : 1,
         100, campaign_ ? spendable_gold(campaign_->state().wealth, campaign_->coin_rates()) : 0},
        seed_);
    install_combat(std::move(next), std::move(reward_id));
}

// Each member's hit points, for the log: " Arden 8/20 Bryn 0/18".
static std::string party_hit_points(const CampaignParty &party)
{
    std::string text;
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            const auto &member = party.member(id);
            text += " " + member.character.sheet().name + " " +
                    std::to_string(member.vitals.hit_points) + "/" +
                    std::to_string(party.hit_point_maximum(id));
        }
    }
    return text;
}

void CombatDemo::encounter(CampaignEncounter encounter, std::uint64_t seed)
{
    if (!campaign_ || combat_)
    {
        throw std::runtime_error("A new campaign combat owner is required");
    }
    if (campaign_->identity() != module_->identity())
    {
        throw std::runtime_error("Invalid campaign encounter context");
    }
    const auto &board = encounter.field.geometry;
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64 ||
        board.terrain.size() != static_cast<std::size_t>(board.width * board.height) ||
        encounter.surprise > Surprise::both || encounter.distance > 2)
    {
        throw std::runtime_error("Invalid campaign battlefield");
    }
    // A party member the original formation has no room for, or the one
    // monster kept when none had room, stands inside one reachable component
    // of the original geometry. Never erase walls.
    std::vector<Cell> cells;
    for (int y = 0; y < board.height; ++y)
    {
        for (int x = 0; x < board.width; ++x)
        {
            if (board.at({x, y}) != Terrain::obstacle)
            {
                cells.push_back({x, y});
            }
        }
    }
    const auto distance = [](Cell a, Cell b)
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    };
    const Cell origin{25, 13};
    if (cells.empty())
    {
        throw std::runtime_error("Battlefield has no open cells");
    }
    std::stable_sort(cells.begin(), cells.end(),
                     [&](Cell a, Cell b)
                     {
                         return distance(a, origin) < distance(b, origin);
                     });
    std::vector<bool> seen(board.terrain.size(), false);
    std::queue<Cell> frontier;
    frontier.push(cells.front());
    seen[cells.front().y * board.width + cells.front().x] = true;
    cells.clear();
    while (!frontier.empty())
    {
        const auto p = frontier.front();
        frontier.pop();
        cells.push_back(p);
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                const Cell next{p.x + x, p.y + y};
                if ((!x && !y) || board.at(next) == Terrain::obstacle)
                {
                    continue;
                }
                if (x && y &&
                    (board.at({p.x + x, p.y}) == Terrain::obstacle ||
                     board.at({p.x, p.y + y}) == Terrain::obstacle))
                {
                    continue;
                }
                const auto index = next.y * board.width + next.x;
                if (seen[index])
                {
                    continue;
                }
                seen[index] = true;
                frontier.push(next);
            }
        }
    }
    auto participants = campaign_->participants();
    if (encounter.champion)
    {
        std::erase_if(participants,
                      [&](const auto &participant)
                      {
                          return participant.id != encounter.champion;
                      });
        if (participants.empty())
        {
            throw std::runtime_error("The duel's champion cannot fight");
        }
        // The arena's copy carries the duelist's gear and stands at its Hit
        // Points, as the original copies the record and its items; the
        // original names it ROLF.
        if (encounter.mirror)
        {
            auto copy = participants.front();
            copy.id = mirror_id;
            copy.side = Side::opposition;
            copy.name = "Rolf";
            encounter.enemies = {std::move(copy)};
        }
    }
    for (auto &participant : participants)
    {
        participant.resting = encounter.party_resting;
    }
    // Members fighting for the monsters stand with them, under the computer.
    for (const auto id : encounter.turncoats)
    {
        const auto turned = std::ranges::find(participants, id, &Participant::id);
        if (turned == participants.end())
        {
            throw std::runtime_error("A member who turned on the party is not in it");
        }
        turned->side = Side::opposition;
        encounter.enemies.insert(encounter.enemies.begin(), std::move(*turned));
        participants.erase(turned);
    }
    if (participants.empty())
    {
        throw std::runtime_error("No member is left to fight for the party");
    }
    // The reachable squares are not counted against the fighters: trees can
    // close in the open square nearest the centre, yet the original stands
    // everyone on its formation square whatever is reachable. Only one who
    // falls back to `cells` needs room there (stand_nearest).
    if (!encounter.positions.empty())
    {
        if (encounter.positions.size() != participants.size() + encounter.enemies.size())
        {
            throw std::runtime_error("Invalid authored combat formation");
        }
        std::set<Cell> occupied;
        for (std::size_t i = 0; i < encounter.positions.size(); ++i)
        {
            auto &placed = i < participants.size() ? participants[i]
                                                   : encounter.enemies[i - participants.size()];
            placed.cell = encounter.positions[i];
            for (const auto square : squares_of(placed.cell, placed.footprint))
            {
                if (board.at(square) == Terrain::obstacle || !occupied.insert(square).second)
                {
                    throw std::runtime_error("Invalid authored combat position");
                }
            }
        }
    }
    if (encounter.positions.empty())
    {
        place_as_original(encounter, participants, cells, origin);
    }
    // Allies take the free squares nearest the party's first member.
    for (auto &ally : encounter.allies)
    {
        const auto covered = [](const Participant &p, Cell cell)
        {
            return covers(p.cell, p.footprint, cell);
        };
        std::erase_if(cells,
                      [&](Cell cell)
                      {
                          return std::any_of(participants.begin(), participants.end(),
                                             [&](const auto &p)
                                             {
                                                 return covered(p, cell);
                                             }) ||
                                 std::any_of(encounter.enemies.begin(), encounter.enemies.end(),
                                             [&](const auto &e)
                                             {
                                                 return covered(e, cell);
                                             });
                      });
        // A creature the original has no room for is left out of the fight.
        const auto room = nearest_room(cells, ally.footprint, participants.front().cell);
        if (!room)
        {
            continue;
        }
        stand_on(cells, ally, *room);
        ally.side = Side::party;
        participants.push_back(std::move(ally));
    }
    // Mutual surprise cancels out: Surprise::both marks neither side. The
    // party's side takes the difficulty's to-hit (party_to_hit).
    for (auto &participant : participants)
    {
        participant.surprised = encounter.surprise == Surprise::party;
        participant.original_strength.to_hit += encounter.party_to_hit;
    }
    for (auto &enemy : encounter.enemies)
    {
        enemy.surprised = encounter.surprise == Surprise::monsters;
        participants.push_back(std::move(enemy));
    }
    // Every fight of a campaign rolls its own dice: the same seed alone would
    // give each one the same rolls. Both numbers are saved, so a reload repeats
    // them unless the difficulty gave it new dice (reseed_after_load).
    constexpr std::uint64_t golden_ratio = 0x9E3779B97F4A7C15ULL;
    seed += (campaign_->state().next_combat_scope - 1) * golden_ratio +
            (campaign_->state().random_state.value - PartyState{}.random_state.value);
    if (diagnostic_log_enabled())
    {
        log_diagnostic(LogPriority::info, "fight",
                       "begins with seed " + std::to_string(seed) + ", " +
                           std::to_string(participants.size()) + " combatants;" +
                           party_hit_points(*campaign_));
    }
    const RollPurpose initiative("initiative");
    auto next = module_->create({encounter.field.geometry,
                                 std::move(participants),
                                 campaign_->state().next_combat_scope,
                                 encounter.morale,
                                 spendable_gold(campaign_->state().wealth, campaign_->coin_rates()),
                                 encounter.daylight,
                                 encounter.spares_the_party,
                                 encounter.outdoors,
                                 encounter.escape_ends_fight,
                                 encounter.no_magic,
                                 static_cast<int>(index(encounter.facing)) * 2,
                                 encounter.party_move_modifier,
                                 encounter.crystals,
                                 encounter.frightening_surprise,
                                 encounter.romance,
                                 encounter.turning_tries,
                                 encounter.turning_penalty,
                                 campaign_->state().time_minutes,
                                 encounter.zero_gravity,
                                 encounter.underwater},
                                seed);
    install_combat(std::move(next), {});
    seed_ = seed;
    battlefield_tiles_ = std::move(encounter.field.tiles);
    terrain_art_ = std::move(encounter.terrain_art);
    art_ = std::move(encounter.art);
    // A campaign fight has no demo status: when it ends, the turn line already
    // says Victory or defeat.
    status_.clear();
    dialogue_.clear();
}

CombatDemoSetup make_combat_demo(std::unique_ptr<RulesModule> rules,
                                 const CharacterRules &characters,
                                 const std::filesystem::path &game_directory,
                                 const CombatDemoOptions &options)
{
    const auto &body_catalog_file = options.body_catalog_file;
    const std::span<const std::string> classes = options.classes, enemies = options.enemies,
                                       gear = options.gear;
    const unsigned level = options.level;
    if (!rules)
    {
        throw std::runtime_error("Combat demo requires combat rules");
    }
    auto art = CharacterArt::load(game_directory);
    auto pool = character_pool(characters, art);
    const auto body_catalog = body_catalog_file.empty()
                                  ? std::optional<CombatBodyCatalog>{}
                                  : std::optional<CombatBodyCatalog>{CombatBodyCatalog::load(
                                        body_catalog_file, body_catalog_file.parent_path() /
                                                               "combat-weapon-options.tsv")};
    auto party = std::make_shared<CampaignParty>(std::move(rules));
    CombatDemoSetup result{party, {}};
    result.encounter.field.geometry = {12, 12, std::vector<Terrain>(144)};
    result.encounter.field.tiles = std::vector<std::uint8_t>(144, 0);
    const std::array<std::string_view, 6> showcase{"fighter", "paladin", "cleric",
                                                   "ranger",  "rogue",   "bard"};
    const std::array<unsigned, 6> weapons{36, 36, 23, 36, 8, 33};
    const std::array<Cell, 6> positions{{{5, 5}, {6, 5}, {7, 5}, {5, 6}, {6, 6}, {7, 6}}};
    if (classes.size() > positions.size())
    {
        throw std::runtime_error("A play-test party has at most six members");
    }
    const bool custom = !classes.empty();
    for (std::size_t i = 0; i < (custom ? classes.size() : showcase.size()); ++i)
    {
        const std::string_view klass = custom ? std::string_view(classes[i]) : showcase[i];
        // A repeated class takes the class's next pool variant.
        const auto earlier =
            custom
                ? static_cast<unsigned>(std::count(classes.begin(),
                                                   classes.begin() + static_cast<std::ptrdiff_t>(i),
                                                   std::string(klass)))
                : 0u;
        unsigned seen = 0;
        const auto found =
            std::find_if(pool.begin(), pool.end(),
                         [&](const Character &candidate)
                         {
                             if (candidate.creation_data().character_class != klass)
                             {
                                 return false;
                             }
                             if (custom)
                             {
                                 return seen++ == earlier % 4;
                             }
                             return i != 0 || candidate.creation_data().race == "goliath";
                         });
        if (found == pool.end())
        {
            throw std::runtime_error("Missing showcase hero in character pool");
        }
        const auto id = party->add_pc(*found);
        party->add_coins(Coin::gold, 10);
        if (custom)
        {
            outfit_pool_member(*party, id);
        }
        else
        {
            for (const unsigned type : {weapons[i], i < 2 ? 55u : 50u})
            {
                Equipment item;
                item.stored.type = static_cast<std::uint8_t>(type);
                item.stored.stack_size = 1;
                item.stored.value = 1;
                party->purchase(id, item);
                party->equip(id, party->member(id).character.inventory().items().back().id);
            }
        }

        for (const auto &key : gear)
        {
            Equipment flask;
            flask.stored.type = key == "oil"    ? original_item::flask_of_oil
                                : key == "acid" ? authored_item::acid
                                : key == "alchemists_fire"
                                    ? authored_item::alchemists_fire
                                    : throw std::runtime_error("Unknown play-test gear: " + key);
            flask.stored.stack_size = 1;
            party->purchase(id, flask);
        }
        auto icons =
            party_combat_icons(party->member(id), art, body_catalog ? &*body_catalog : nullptr);
        result.encounter.art.push_back({id, std::move(icons.ready), std::move(icons.action),
                                        std::move(icons.missing_combination)});
    }
    // Every member gains the experience for `level` and takes default choices.
    if (level > 1)
    {
        party->award_experience(level >= 4 ? 2700 : level == 3 ? 900 : 300, "playtest:level");
        for (const auto id : party->state().slots)
        {
            while (id && party->can_advance(id))
            {
                party->advance(id, party->default_advancement(id));
            }
        }
    }
    result.encounter.positions.assign(
        positions.begin(),
        positions.begin() + static_cast<std::ptrdiff_t>(party->state().roster.size()));
    if (!enemies.empty())
    {
        // In a column east of the party, with their Slums art and the squares
        // those fill (FOOT-1).
        const auto creatures = CreatureCatalog::load(game_directory);
        int row = 4;
        for (std::size_t n = 0; n < enemies.size(); ++n)
        {
            const auto &definition = enemies[n];
            const unsigned icon = definition == "troll" ? 31 : definition == "ogre" ? 8 : 0;
            auto picture = slums_bank_art(game_directory, options.monsters.get(), icon, icon);
            const auto creature = creatures.find({2, static_cast<std::uint8_t>(icon)});
            if (!picture || !creature)
            {
                throw std::runtime_error("Missing original combat icon for " + definition);
            }
            const auto id = static_cast<EntityId>(1000 + n);
            const Cell cell{10, row};
            result.encounter.enemies.push_back(por::staged_fighter(
                id, definition, definition + " " + std::to_string(n + 1), Side::opposition,
                creature->get(), options.monsters ? options.monsters->creature(2, icon) : nullptr,
                options.difficulty));
            result.encounter.enemies.back().cell = cell;
            row += result.encounter.enemies.back().footprint.height;
            result.encounter.positions.push_back(cell);
            picture->entity = id;
            result.encounter.art.push_back(std::move(*picture));
        }
        apply_difficulty(result.encounter, options.difficulty, level);
        return result;
    }
    const auto kobold = slums_bank_art(game_directory, options.monsters.get(), 0, 0);
    const auto leader = slums_bank_art(game_directory, options.monsters.get(), 1, 1);
    if (!kobold || !leader)
    {
        throw std::runtime_error("Missing original Kobold combat icon");
    }
    const auto creatures = CreatureCatalog::load(game_directory);
    static constexpr std::array<Cell, 6> removed{{{6, 4}, {9, 4}, {9, 7}, {7, 8}, {4, 8}, {4, 5}}};
    unsigned number = 0;
    for (int y = 4; y <= 8; ++y)
    {
        for (int x = 4; x <= 9; ++x)
        {
            if (x > 4 && x < 9 && y > 4 && y < 8)
            {
                continue;
            }
            const Cell cell{x, y};
            const bool is_leader = cell == Cell{6, 4};
            if (!is_leader && std::find(removed.begin(), removed.end(), cell) != removed.end())
            {
                continue;
            }
            const auto id = static_cast<EntityId>(1000 + result.encounter.enemies.size());
            const std::uint8_t record = is_leader ? 1 : 0;
            const auto creature = creatures.find({2, record});
            if (!creature)
            {
                throw std::runtime_error("Missing original Kobold record");
            }
            result.encounter.enemies.push_back(por::staged_fighter(
                id, is_leader ? "slums-kobold-leader" : "slums-kobold",
                is_leader ? "Kobold Leader" : "Kobold " + std::to_string(++number),
                Side::opposition, creature->get(),
                options.monsters ? options.monsters->creature(2, record) : nullptr,
                options.difficulty));
            result.encounter.enemies.back().cell = cell;
            result.encounter.positions.push_back({x, y});
            result.encounter.art.push_back(is_leader ? *leader : *kobold);
            result.encounter.art.back().entity = id;
        }
    }
    apply_difficulty(result.encounter, options.difficulty, level);
    return result;
}

const CombatSession &CombatDemo::combat() const
{
    if (!combat_)
    {
        throw std::runtime_error("No active combat");
    }
    return *combat_;
}

void CombatDemo::training(std::uint64_t seed, bool conditions)
{
    if (campaign_)
    {
        throw std::runtime_error("A campaign fight needs an encounter");
    }
    const RollPurpose initiative("initiative");
    auto next = module_->create(
        {arena(),
         {{1,
           conditions ? "blindness-adept" : "vanguard",
           conditions ? "Adept" : "Vanguard",
           Side::party,
           {2, 4}},
          {10, "bandit", "Bandit", Side::opposition, conditions ? Cell{3, 4} : Cell{9, 4}}}},
        seed);
    combat_ = std::move(next);
    vm_.reset();
    creatures_.reset();
    art_.clear();
    enemies_.clear();
    menu_ticket_ = combat_ticket_ = 0;
    encounters_ = 0;
    seed_ = seed;
    dialogue_ = "Training encounter: one martial test profile and one SRD Bandit.";
    status_ = conditions ? "Blindness training" : "Training arena";
}

void CombatDemo::slums(const std::filesystem::path &directory, std::uint64_t seed,
                       std::shared_ptr<const por::MonsterBundle> monsters, Difficulty difficulty)
{
    if (campaign_combat_)
    {
        throw std::runtime_error("Finish combat before changing the party");
    }
    difficulty_ = difficulty;
    const auto catalog = EclCatalog::load(directory);
    const auto program = catalog.find({"ECL2.DAX", 20});
    if (!program)
    {
        throw std::runtime_error("Slums profile requires ECL2.DAX:20");
    }
    auto creatures = CreatureCatalog::load(directory);
    for (auto record : {4, 13})
    {
        if (!creatures.find({2, static_cast<std::uint8_t>(record)}))
        {
            throw std::runtime_error("Missing Slums creature record");
        }
    }
    EclMachine vm(program);
    for (auto [first, last] : std::array<std::array<unsigned, 2>, 3>{
             {{0x4900, 0x4cff}, {0x6b00, 0x6eff}, {0x9700, 0x98ff}}})
    {
        for (unsigned a = first; a <= last; ++a)
        {
            vm.bind_variable(EclAddress(static_cast<std::uint16_t>(a)), 0);
        }
    }
    vm.bind_variable(ecl_cell_event, 1);
    for (std::uint8_t opcode : std::initializer_list<std::uint8_t>{11, 12, 13, 14, 28, 36})
    {
        vm.enable_host(opcode);
    }
    if (!vm.start(1))
    {
        throw std::runtime_error("Cannot enter Slums event 1");
    }
    vm_ = std::move(vm);
    creatures_ = std::move(creatures);
    combat_.reset();
    art_.clear();
    enemies_.clear();
    menu_ticket_ = combat_ticket_ = 0;
    seed_ = seed;
    encounters_ = 0;
    game_directory_ = directory;
    monsters_ = std::move(monsters);
    dialogue_.clear();
    status_ = "Slums event 1";
    pump();
}

void CombatDemo::pump()
{
    if (!vm_ || menu_ticket_ || combat_ticket_)
    {
        return;
    }
    for (unsigned step = 0; step < 64; ++step)
    {
        const auto result = vm_->run(1000);
        if (result.state == EclState::faulted)
        {
            throw std::runtime_error(result.diagnostic);
        }
        if (result.state == EclState::completed)
        {
            status_ = "Script complete; event flag " +
                      std::to_string(vm_->variable(ecl_slums_orc_victory)) + ", fight count " +
                      std::to_string(vm_->variable(EclAddress{0x4ABB}));
            return;
        }
        if (!result.request)
        {
            continue;
        }
        const auto &request = *result.request;
        if (request.kind == EclRequestKind::text)
        {
            if (request.clear)
            {
                dialogue_.clear();
            }
            dialogue_ += request.text;
            if (dialogue_.size() > 8192)
            {
                throw std::runtime_error("Slums dialogue exceeds limit");
            }
            if (!vm_->resume(request.id))
            {
                throw std::runtime_error("Text acknowledgement rejected");
            }
        }
        else if (request.kind == EclRequestKind::menu)
        {
            if (request.choices.size() != 1)
            {
                throw std::runtime_error("Unsupported Slums menu");
            }
            menu_ticket_ = request.id;
            return;
        }
        else if (request.kind == EclRequestKind::host)
        {
            const auto opcode = request.instruction->opcode;
            if (opcode == 11)
            {
                const auto &a = request.arguments;
                const bool first = enemies_.empty();
                const unsigned record = first ? 13 : 4, count = first ? 1 : 3;
                if (a.size() != 3 || a[0].number() != record || a[1].number() != count ||
                    a[2].number() != 4 || enemies_.size() > 1)
                {
                    throw std::runtime_error("Unrecognized Slums creature/count/icon profile");
                }
                const auto &creature =
                    creatures_->find({2, static_cast<std::uint8_t>(record)})->get();
                const auto art =
                    slums_bank_art(game_directory_, monsters_.get(), record, a[2].number());
                for (unsigned i = 0; i < count; ++i)
                {
                    const auto id = static_cast<EntityId>(1000 + enemies_.size());
                    enemies_.push_back(por::staged_fighter(
                        id, "slums-orc",
                        creature.stored.name + " " + std::to_string(enemies_.size() + 1),
                        Side::opposition, creature,
                        monsters_ ? monsters_->creature(2, record) : nullptr, difficulty_));
                    enemies_.back().cell = {9, 2 + static_cast<int>(enemies_.size()) - 1};
                    if (art)
                    {
                        art_.push_back(*art);
                        art_.back().entity = id;
                    }
                }
            }
            else if (opcode == 36)
            {
                if (enemies_.size() != 4 || encounters_ != 0 ||
                    vm_->variable(ecl_encounter_morale) != 99 ||
                    vm_->variable(ecl_encounter_surprise) != 0)
                {
                    throw std::runtime_error("Unsupported Slums combat context");
                }
                CampaignEncounter scaled;
                scaled.enemies = enemies_;
                apply_difficulty(scaled, difficulty_, 1);
                start_encounter(std::move(scaled.enemies), "por:ECL2:20:search1:orcs:v1");
                combat_ticket_ = request.id;
                ++encounters_;
                status_ = "Slums combat: original four-orc group, authored 5e conversion and arena";
                return;
            }
            else if (opcode == 28)
            {
                enemies_.clear();
                art_.clear(); // CLEAR MONSTERS resets the staged encounter.
            }
            else if (opcode != 12 && opcode != 13 && opcode != 14)
            {
                throw std::runtime_error("Unsupported Slums presentation service");
            }
            if (!vm_->resume_host(request.id, {}))
            {
                throw std::runtime_error("Slums host acknowledgement rejected");
            }
        }
        else
        {
            throw std::runtime_error("Unsupported Slums input");
        }
    }
    throw std::runtime_error("Slums script exceeded request budget");
}

void CombatDemo::continue_script()
{
    if (!vm_ || !menu_ticket_)
    {
        return;
    }
    if (!vm_->resume(menu_ticket_, 0))
    {
        throw std::runtime_error("Slums menu acknowledgement rejected");
    }
    menu_ticket_ = 0;
    pump();
}

void CombatDemo::finish_combat()
{
    if (!vm_ || !combat_ticket_ || !combat_)
    {
        return;
    }
    const auto state = combat_->snapshot();
    if (state.outcome == Outcome::ongoing)
    {
        return;
    }
    unsigned defeated = 0;
    for (const auto &unit : state.combatants)
    {
        if (unit.side == Side::opposition && (unit.hit_points <= 0 || unit.surrendered))
        {
            ++defeated;
        }
    }
    EclHostReply reply;
    reply.writes = {
        // The original's combat results, as its engine wrote them (coab
        // ovr006): 0 won, 128 (0x80) the party fell or lost a duel, 129 (0x81)
        // it fled. Scripts test "above 128" for a flight.
        {ecl_combat_result, static_cast<std::uint16_t>(state.outcome == Outcome::victory ? 0
                                                       : state.outcome == Outcome::fled  ? 129
                                                                                         : 128)},
        {ecl_monsters_defeated, static_cast<std::uint16_t>(defeated)},
        {ecl_encounter_surprise, 0},
        {EclAddress{0x6DE3}, 0},
        {EclAddress{0x6E70}, 0},
        {EclAddress{0x6E71}, 0},
        {EclAddress{0x6E72}, 0},
        {ecl_allies_attacked, static_cast<std::uint16_t>(state.allies_attacked)}};
    if (!vm_->resume_host(combat_ticket_, reply))
    {
        throw std::runtime_error("Combat outcome rejected by ECL");
    }
    combat_ticket_ = 0;
    pump();
}

bool CombatDemo::submit(const Command &command)
{
    if (!combat_)
    {
        return false;
    }
    // The log file names what each die is for and keeps every line of the
    // fight's log, with the dice behind each damage total.
    const bool logging = diagnostic_log_enabled();
    const auto before = logging ? combat_->snapshot().log_entries : std::vector<LogEntry>{};
    std::optional<RollPurpose> purpose;
    if (logging)
    {
        purpose.emplace(command_purpose(combat_->snapshot(), command));
        log_diagnostic(LogPriority::info, "command", command_purpose(combat_->snapshot(), command));
    }
    if (!combat_->submit(command))
    {
        return false;
    }
    purpose.reset();
    if (logging)
    {
        log_new_fight_lines(before, combat_->snapshot().log_entries);
    }
    synchronize_party();
    finish_combat();
    return true;
}

void CombatDemo::attack_allies(rules::EntityId attacker)
{
    if (!combat_)
    {
        return;
    }
    std::vector<rules::EntityId> turning;
    for (const auto &unit : combat_->snapshot().combatants)
    {
        if (unit.id == attacker || unit.side != Side::party)
        {
            continue;
        }
        const auto &slots = campaign_ ? campaign_->state().slots : std::array<MemberId, 8>{};
        const bool companion = std::ranges::find(slots, unit.id) != slots.end() &&
                               !campaign_->member(unit.id).npc_source.empty();
        if (companion || is_ally(unit.id))
        {
            turning.push_back(unit.id);
        }
    }
    combat_->turn_against_party(turning);
}

void CombatDemo::revisit()
{
    if (!script_complete() || !combat_ || combat_->snapshot().outcome != Outcome::victory)
    {
        throw std::runtime_error("Revisit requires completed victory");
    }
    if (!vm_->start(1))
    {
        throw std::runtime_error("Slums revisit rejected");
    }
    dialogue_.clear();
    pump();
}

std::string CombatDemo::save_combat() const
{
    if (vm_ || campaign_)
    {
        throw std::runtime_error(
            "Campaign checkpoints are pending; save/load currently supports training combat only");
    }
    return combat().save();
}

void CombatDemo::restore_combat(std::string_view checkpoint)
{
    if (vm_ || campaign_)
    {
        throw std::runtime_error("Cannot replace a campaign combat with a training checkpoint");
    }
    auto restored = module_->restore(checkpoint);
    combat_ = std::move(restored);
}

unsigned CombatDemo::script_variable(EclAddress address) const
{
    if (!vm_)
    {
        throw std::runtime_error("No active campaign script");
    }
    return vm_->variable(address);
}
} // namespace opengold
