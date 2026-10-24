#include "archive_io.h"
#include "opengold/authored_items.h"
#include "opengold/combat_demo.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/character_pool.h"
#include "opengold/encounter_formation.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <queue>
#include <set>
#include <span>
#include <stdexcept>

namespace opengold
{
using namespace rules;
using namespace por;

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

std::optional<Image> original_icon(const std::filesystem::path &directory, unsigned record)
{
    for (const auto &file : std::filesystem::directory_iterator(directory))
    {
        auto name = por::detail::upper_ascii(file.path().filename().string());
        if (name != "CPIC2.DAX")
        {
            continue;
        }
        auto bytes = por::detail::read_archive_bytes<std::runtime_error>(
            file.path(), "Combat art archive exceeds limit", "Cannot read combat art",
            "Cannot read combat art");
        auto decoded = decode_ega_combat_icon(bytes, static_cast<std::uint8_t>(record), 0);
        if (!decoded)
        {
            throw std::runtime_error("Cannot decode the script-selected combat icon");
        }
        return std::move(decoded.image);
    }
    return std::nullopt;
}

// Stands the party and the enemies where the original engine did (PLACE-1).
// A monster it had no room for is left out of the fight, as there. A party
// member without room takes the free square of `reachable` nearest the side's
// first member (or `centre`); taken squares leave `reachable`.
void place_as_original(CampaignEncounter &encounter, std::vector<Participant> &party,
                       std::vector<Cell> &reachable, Cell centre)
{
    const auto formation =
        encounter_formation(encounter.field, encounter.facing, encounter.distance, party.size(),
                            encounter.enemies.size());
    const auto claim =
        [&](std::span<Participant> side, const std::vector<std::optional<Cell>> &starts)
    {
        for (std::size_t n = 0; n < side.size(); ++n)
        {
            if (starts[n])
            {
                side[n].cell = *starts[n];
                std::erase(reachable, *starts[n]);
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
            if (reachable.empty())
            {
                throw std::runtime_error("Original battlefield cannot fit the complete encounter");
            }
            const auto target = starts.front().value_or(centre);
            const auto nearest =
                std::min_element(reachable.begin(), reachable.end(),
                                 [&](Cell a, Cell b)
                                 {
                                     return std::abs(a.x - target.x) + std::abs(a.y - target.y) <
                                            std::abs(b.x - target.x) + std::abs(b.y - target.y);
                                 });
            side[n].cell = *nearest;
            reachable.erase(nearest);
        }
    };
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
    auto next = module_->create({arena(), std::move(participants),
                                 campaign_ ? campaign_->state().next_combat_scope : 1, 100,
                                 campaign_ ? coins(campaign_->state().wealth, Coin::gold) : 0},
                                seed_);
    install_combat(std::move(next), std::move(reward_id));
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
    }
    for (auto &participant : participants)
    {
        participant.resting = encounter.party_resting;
    }
    if (cells.size() < participants.size() + 1)
    {
        throw std::runtime_error("Original battlefield cannot fit the complete encounter");
    }
    if (!encounter.positions.empty())
    {
        if (encounter.positions.size() != participants.size() + encounter.enemies.size())
        {
            throw std::runtime_error("Invalid authored combat formation");
        }
        std::set<Cell> occupied;
        for (std::size_t i = 0; i < encounter.positions.size(); ++i)
        {
            const auto cell = encounter.positions[i];
            if (board.at(cell) == Terrain::obstacle || !occupied.insert(cell).second)
            {
                throw std::runtime_error("Invalid authored combat position");
            }
            if (i < participants.size())
            {
                participants[i].cell = cell;
            }
            else
            {
                encounter.enemies[i - participants.size()].cell = cell;
            }
        }
    }
    if (encounter.positions.empty())
    {
        place_as_original(encounter, participants, cells, origin);
    }
    // Mutual surprise cancels out: Surprise::both marks neither side.
    for (auto &participant : participants)
    {
        participant.surprised = encounter.surprise == Surprise::party;
    }
    for (auto &enemy : encounter.enemies)
    {
        enemy.surprised = encounter.surprise == Surprise::monsters;
        participants.push_back(std::move(enemy));
    }
    auto next = module_->create({encounter.field.geometry, std::move(participants),
                                 campaign_->state().next_combat_scope, encounter.morale,
                                 coins(campaign_->state().wealth, Coin::gold), encounter.daylight},
                                seed);
    install_combat(std::move(next), {});
    seed_ = seed;
    battlefield_tiles_ = std::move(encounter.field.tiles);
    terrain_art_ = std::move(encounter.terrain_art);
    art_ = std::move(encounter.art);
    // A campaign fight has no demo status: when it ends, the turn line already
    // says Victory or defeat.
    status_.clear();
    dialogue_ = "The original script has requested combat.";
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
                item.stored.type = type;
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
        auto appearance = found->appearance();
        std::string missing;
        if (body_catalog)
        {
            const auto resolved = resolve_combat_appearance(party->member(id), *body_catalog);
            if (!resolved.selection.matched)
            {
                missing = resolved.selection.label;
            }
            result.encounter.art.push_back({id, resolved.icon(art, IconPose::ready),
                                            resolved.icon(art, IconPose::action), missing});
        }
        else
        {
            result.encounter.art.push_back({id, art.icon(appearance, IconPose::ready),
                                            art.icon(appearance, IconPose::action), missing});
        }
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
        // In a row east of the party, with their original Slums icons.
        for (std::size_t n = 0; n < enemies.size(); ++n)
        {
            const auto &definition = enemies[n];
            const unsigned icon = definition == "troll" ? 31 : definition == "ogre" ? 8 : 0;
            const auto picture = original_icon(game_directory, icon);
            if (!picture)
            {
                throw std::runtime_error("Missing original combat icon for " + definition);
            }
            const auto id = static_cast<EntityId>(1000 + n);
            const Cell cell{10, 4 + static_cast<int>(n)};
            result.encounter.enemies.push_back(
                {id, definition, definition + " " + std::to_string(n + 1), Side::opposition, cell});
            result.encounter.positions.push_back(cell);
            result.encounter.art.push_back(
                {id, *picture, original_icon(game_directory, icon + 128)});
        }
        return result;
    }
    const auto kobold = original_icon(game_directory, 0);
    const auto kobold_action = original_icon(game_directory, 128);
    const auto leader = original_icon(game_directory, 1);
    const auto leader_action = original_icon(game_directory, 129);
    if (!kobold || !leader)
    {
        throw std::runtime_error("Missing original Kobold combat icon");
    }
    constexpr std::array<Cell, 6> removed{{{6, 4}, {9, 4}, {9, 7}, {7, 8}, {4, 8}, {4, 5}}};
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
            result.encounter.enemies.push_back(
                {id, is_leader ? "slums-kobold-leader" : "slums-kobold",
                 is_leader ? "Kobold Leader" : "Kobold " + std::to_string(++number),
                 Side::opposition, cell});
            result.encounter.positions.push_back({x, y});
            result.encounter.art.push_back(
                {id, is_leader ? *leader : *kobold, is_leader ? leader_action : kobold_action});
        }
    }
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
        seed_ = seed;
        start_encounter({{1000, "bandit", "Bandit", Side::opposition, {9, 4}}},
                        "preview:bandit:v1");
        vm_.reset();
        art_.clear();
        dialogue_ = "Party combat preview. HP and spent resources carry back to the party.";
        status_ = "Party training";
        return;
    }
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

void CombatDemo::slums(const std::filesystem::path &directory, std::uint64_t seed)
{
    if (campaign_combat_)
    {
        throw std::runtime_error("Finish combat before changing the party");
    }
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
    for (std::uint8_t opcode : {11, 12, 13, 14, 28, 36})
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
                const auto icon = original_icon(game_directory_, a[2].number());
                const auto action = original_icon(game_directory_, a[2].number() + 128);
                for (unsigned i = 0; i < count; ++i)
                {
                    const auto id = static_cast<EntityId>(1000 + enemies_.size());
                    enemies_.push_back(
                        {id,
                         "slums-orc",
                         creature.stored.name + " " + std::to_string(enemies_.size() + 1),
                         Side::opposition,
                         {9, 2 + static_cast<int>(enemies_.size())}});
                    if (icon)
                    {
                        art_.push_back({id, *icon, action});
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
                start_encounter(enemies_, "por:ECL2:20:search1:orcs:v1");
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
        if (unit.side == Side::opposition && unit.hit_points == 0)
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
        {EclAddress{0x6E72}, 0}};
    if (!vm_->resume_host(combat_ticket_, reply))
    {
        throw std::runtime_error("Combat outcome rejected by ECL");
    }
    combat_ticket_ = 0;
    pump();
}

bool CombatDemo::submit(const Command &command)
{
    if (!combat_ || !combat_->submit(command))
    {
        return false;
    }
    synchronize_party();
    finish_combat();
    return true;
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

namespace
{
std::optional<Command> run_for_edge(const rules::Snapshot &state,
                                    const std::vector<Command> &offered,
                                    const rules::CombatantView &active);
// The demonstration AI's choice from what it reads of a fight: its state and
// the commands open to it. It submits nothing, so it takes no session
// (Effective C++ Item 32).
Command choose_from(const rules::Snapshot &state, const std::vector<Command> &offered,
                    Searching searching);
Command choose_in_sight(const rules::Snapshot &state, const std::vector<Command> &offered,
                        Searching searching);
std::optional<Command> choose_item(const rules::Snapshot &state, const CombatantView &active,
                                   const std::vector<Command> &offered);

bool item_verb(std::string_view verb);

// A choice the original made at random, here repeatable from the fight's state
// so a replay or a test sees the same one: an index below `count`.
std::size_t pick(const rules::Snapshot &state, EntityId actor, std::uint64_t salt,
                 std::size_t count)
{
    std::uint64_t x =
        state.revision * 0x9E3779B97F4A7C15ull ^ (static_cast<std::uint64_t>(actor) << 32) ^ salt;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return static_cast<std::size_t>(x % count);
}

// The spells the computer casts with a slot, each with the original's priority
// (#266): it rolls 1d7 and tries the priorities from 7 down to 8 less the roll,
// so the strongest come first and the weakest only on a high roll. A helpful
// spell goes on the caster or an ally. Healing, cantrips and the spells with
// their own rules below (Moonbeam, Hex, Spiritual Weapon, Hunter's Mark) are
// not here; neither is any spell left to the player.
struct AiSpell
{
    std::string_view id;
    int priority;
    bool helpful{};
};

constexpr std::array ai_spells{AiSpell{"fireball", 7},
                               AiSpell{"hold_monster", 7},
                               AiSpell{"confusion", 7},
                               AiSpell{"ice_storm", 7},
                               AiSpell{"finger_of_death", 7},
                               AiSpell{"circle_of_death", 7},
                               AiSpell{"chain_lightning", 7},
                               AiSpell{"sunburst", 7},
                               AiSpell{"power_word_stun", 7},
                               AiSpell{"disintegrate", 7},
                               AiSpell{"harm", 7},
                               AiSpell{"prismatic_spray", 7},
                               AiSpell{"freezing_sphere", 7},
                               AiSpell{"hold_person", 6},
                               AiSpell{"lightning_bolt", 6},
                               AiSpell{"cone_of_cold", 6},
                               AiSpell{"flame_strike", 6},
                               AiSpell{"inflict_wounds", 6},
                               AiSpell{"fear", 6},
                               AiSpell{"charm_monster", 6},
                               AiSpell{"blight", 6},
                               AiSpell{"phantasmal_killer", 6},
                               AiSpell{"contagion", 6},
                               AiSpell{"flesh_to_stone", 6},
                               AiSpell{"eyebite", 6},
                               AiSpell{"dominate_person", 6},
                               AiSpell{"banishment", 6},
                               AiSpell{"hypnotic_pattern", 6},
                               AiSpell{"vitriolic_sphere", 6},
                               AiSpell{"irresistible_dance", 6},
                               AiSpell{"spirit_guardians", 5, true},
                               AiSpell{"stinking_cloud", 5},
                               AiSpell{"bestow_curse", 5},
                               AiSpell{"cloudkill", 5},
                               AiSpell{"sleet_storm", 5},
                               AiSpell{"black_tentacles", 5},
                               AiSpell{"shatter", 5},
                               AiSpell{"blindness", 5},
                               AiSpell{"vampiric_touch", 5},
                               AiSpell{"insect_plague", 5},
                               AiSpell{"magic_missile", 4},
                               AiSpell{"dispel_magic", 4},
                               AiSpell{"charm_person", 4},
                               AiSpell{"silence", 4},
                               AiSpell{"slow", 4},
                               AiSpell{"faerie_fire", 4},
                               AiSpell{"scorching_ray", 4},
                               AiSpell{"acid_arrow", 4},
                               AiSpell{"guiding_bolt", 4},
                               AiSpell{"mind_spike", 4},
                               AiSpell{"ray_of_enfeeblement", 4},
                               AiSpell{"hideous_laughter", 4},
                               AiSpell{"dissonant_whispers", 4},
                               AiSpell{"chromatic_orb", 4},
                               AiSpell{"ice_knife", 4},
                               AiSpell{"ray_of_sickness", 4},
                               AiSpell{"web", 4},
                               AiSpell{"mirror_image", 3, true},
                               AiSpell{"haste", 3, true},
                               AiSpell{"dragons_breath", 3, true},
                               AiSpell{"protection_from_normal_missiles", 3, true},
                               AiSpell{"entangle", 3},
                               AiSpell{"bane", 3},
                               AiSpell{"blur", 3, true},
                               AiSpell{"greater_invisibility", 3, true},
                               AiSpell{"stoneskin", 3, true},
                               AiSpell{"fire_shield", 3, true},
                               AiSpell{"heroism", 3, true},
                               AiSpell{"sleep", 2},
                               AiSpell{"burning_hands", 2},
                               AiSpell{"thunderwave", 2},
                               AiSpell{"color_spray", 2},
                               AiSpell{"blink", 2, true},
                               AiSpell{"mage_armor", 2, true},
                               AiSpell{"false_life", 2, true},
                               AiSpell{"magic_weapon", 2, true},
                               AiSpell{"barkskin", 2, true},
                               AiSpell{"bless", 1, true},
                               AiSpell{"protection_from_evil_and_good", 1, true},
                               AiSpell{"divine_favor", 1, true},
                               AiSpell{"shield_of_faith", 1, true}};

const AiSpell *ai_spell(std::string_view id)
{
    const auto found = std::find_if(ai_spells.begin(), ai_spells.end(),
                                    [&](const AiSpell &spell)
                                    {
                                        return spell.id == id;
                                    });
    return found == ai_spells.end() ? nullptr : &*found;
}

// The known spell a command casts: the longest of the creature's spell ids that
// is the verb or begins it before a suffix ("magic_missile_2"); empty if none.
std::string_view spell_of(const CombatantView &caster, std::string_view verb)
{
    std::string_view found;
    for (const auto &id : caster.spells)
    {
        if ((verb == id ||
             (verb.starts_with(id) && verb.size() > id.size() && verb[id.size()] == '_')) &&
            id.size() > found.size())
        {
            found = id;
        }
    }
    return found;
}

// How a cast ranks among the same spell's casts: its own level first, then the
// lowest slot above it, then any other form.
int cast_rank(std::string_view verb, std::string_view spell)
{
    if (verb == spell)
    {
        return 0;
    }
    const auto suffix = verb.substr(spell.size() + 1);
    return suffix.size() == 1 && suffix[0] >= '2' && suffix[0] <= '9' ? suffix[0] - '0' : 100;
}

// Whether an area aimed at `center` might catch one of the caster's side,
// judged on the safe side of the area the rules will draw (#266): anyone
// within its reach of the center, plus a square; or, for a cone, a cube
// beside the caster or a line, anyone but the caster ahead of it within the
// reach and near the line toward the center.
bool area_risks_side(const rules::Snapshot &state, const CombatantView &caster,
                     const SpellFacts &spell, Cell center)
{
    const int reach = (spell.area_feet + 4) / 5 + (spell.from_caster ? 2 : 1);
    return std::any_of(
        state.combatants.begin(), state.combatants.end(),
        [&](const auto &a)
        {
            if (a.side != caster.side || a.dead)
            {
                return false;
            }
            if (!spell.from_caster)
            {
                return std::max(std::abs(a.cell.x - center.x), std::abs(a.cell.y - center.y)) <=
                       reach;
            }
            if (a.id == caster.id)
            {
                return false;
            }
            const double dx = center.x - caster.cell.x, dy = center.y - caster.cell.y;
            const double length = std::max(1.0, std::sqrt(dx * dx + dy * dy));
            const double cx = a.cell.x - caster.cell.x, cy = a.cell.y - caster.cell.y;
            const double along = (cx * dx + cy * dy) / length;
            const double off = std::abs(cx * dy - cy * dx) / length;
            return along > 0 && along <= reach && off <= along / 2 + 1.5;
        });
}

// Whether an area spell can be aimed at `target` safely: within its range
// and in the caster's sight, as the rules let it be aimed, within the reach of
// a cone or line, and catching no one of the caster's side.
bool safe_area_target(const rules::Snapshot &state, const CombatantView &caster,
                      const SpellFacts &spell, const CombatantView &target)
{
    const int distance = 5 * std::max(std::abs(target.cell.x - caster.cell.x),
                                      std::abs(target.cell.y - caster.cell.y));
    return target.side != caster.side && target.conscious && distance <= spell.range_feet &&
           (!spell.from_caster || distance <= spell.area_feet) &&
           rules::has_line_of_sight(state.battlefield, caster.cell, target.cell) &&
           !area_risks_side(state, caster, spell, target.cell);
}

// Whether the creature already shows the spell's effect, by its name in the
// creature's status effects ("Paralyzed (Hold Person)").
bool shows_spell(const CombatantView &unit, std::string_view label)
{
    label = label.substr(0, label.find(" ("));
    return std::any_of(unit.status_effects.begin(), unit.status_effects.end(),
                       [&](const auto &effect)
                       {
                           return effect.name.source.find(label) != std::string::npos;
                       });
}
} // namespace

Command choose_demo_command(const CombatSession &session, Searching searching)
{
    return choose_from(session.snapshot(), session.legal_commands(), searching);
}

Command choose_demo_command(const rules::Snapshot &state, const std::vector<Command> &offered,
                            Searching searching)
{
    return choose_from(state, offered, searching);
}

namespace
{
// The AI does not know where a hidden enemy is (HIDE-1): it chooses as if the
// enemy were absent, and with no other enemy left it takes the Search action.
Command choose_from(const rules::Snapshot &state, const std::vector<Command> &offered,
                    Searching searching)
{
    const auto active = std::find_if(state.combatants.begin(), state.combatants.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == state.actor;
                                     });
    const auto hidden_enemy = [&](EntityId id)
    {
        return std::any_of(state.combatants.begin(), state.combatants.end(),
                           [&](const auto &a)
                           {
                               return a.id == id && a.hidden && active != state.combatants.end() &&
                                      a.side != active->side;
                           });
    };
    if (std::none_of(state.combatants.begin(), state.combatants.end(),
                     [&](const auto &a)
                     {
                         return hidden_enemy(a.id);
                     }))
    {
        return choose_in_sight(state, offered, searching);
    }
    auto seen = state;
    std::erase_if(seen.combatants,
                  [&](const auto &a)
                  {
                      return hidden_enemy(a.id);
                  });
    std::vector<Command> open;
    std::copy_if(offered.begin(), offered.end(), std::back_inserter(open),
                 [&](const auto &c)
                 {
                     return !hidden_enemy(c.target);
                 });
    const bool enemy_left = std::any_of(seen.combatants.begin(), seen.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.side != active->side && a.conscious;
                                        });
    if (!enemy_left)
    {
        for (const auto &command : open)
        {
            if (command.verb == "search")
            {
                return command;
            }
        }
    }
    return choose_in_sight(seen, open, searching);
}

Command choose_in_sight(const rules::Snapshot &state, const std::vector<Command> &offered,
                        Searching searching)
{
    if (offered.empty())
    {
        throw std::runtime_error("No legal combat command");
    }
    const auto &active = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto &a)
                                       {
                                           return a.id == state.actor;
                                       });
    // The module orders its pending check choices by its default AI preference.
    if (!state.initiative_choices.empty() || state.effect_targeting ||
        state.optional_effect_choice || state.ability_check_choice)
    {
        return offered.front();
    }
    if (state.temporary_hp_offer)
    {
        const auto verb =
            state.temporary_hp_offer->current.amount >= state.temporary_hp_offer->offered.amount
                ? "temp_hp_keep"
                : "temp_hp_use";
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "stand_up")
        {
            return command;
        }
    }
    // A creature whose morale broke runs for the edge; cornered, it fights on.
    if (active.panicked && !state.reaction_pending)
    {
        if (const auto run = run_for_edge(state, offered, active))
        {
            return *run;
        }
    }
    const auto &board = state.battlefield;
    // The enemies the creature can see: it goes for those, and with none in
    // view it holds its ground (#266).
    std::vector<const CombatantView *> in_view;
    for (const auto &enemy : state.combatants)
    {
        if (enemy.side != active.side && !enemy.dead && enemy.hit_points > 0 &&
            rules::has_line_of_sight(board, active.cell, enemy.cell))
        {
            in_view.push_back(&enemy);
        }
    }
    // A stand-in for the player, or in a check every creature, goes looking
    // for the enemies it cannot see, magic-users too.
    const bool seeking =
        in_view.empty() && (searching == Searching::everyone ||
                            (searching == Searching::party && active.side == Side::party));
    if (seeking)
    {
        for (const auto &enemy : state.combatants)
        {
            if (enemy.side != active.side && !enemy.dead && enemy.hit_points > 0)
            {
                in_view.push_back(&enemy);
            }
        }
    }
    // It keeps its target while that target stands in view, as the original
    // did; otherwise any enemy in view.
    const auto kept = std::find_if(in_view.begin(), in_view.end(),
                                   [&](const auto *enemy)
                                   {
                                       return enemy->id == active.last_target;
                                   });
    // One of the creatures commands with `verb` are aimed at: its target if one
    // is, else one at random from the other side (#266).
    const auto aim = [&](std::string_view verb) -> const Command *
    {
        std::vector<const Command *> aims;
        for (const auto &command : offered)
        {
            if (command.verb != verb)
            {
                continue;
            }
            const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                             [&](const auto &a)
                                             {
                                                 return a.id == command.target;
                                             });
            if (target == state.combatants.end() || target->side == active.side ||
                target->hit_points <= 0)
            {
                continue;
            }
            if (kept != in_view.end() && command.target == active.last_target)
            {
                return &command;
            }
            aims.push_back(&command);
        }
        return aims.empty() ? nullptr : aims[pick(state, active.id, verb.size(), aims.size())];
    };
    // Rank offered destinations by a geometric route around obstacles to its
    // target, or to the nearest enemy in view. Straight distance alone can
    // strand both sides on opposite corners of a wall. This is an AI
    // heuristic; legal movement and its costs remain module-owned.
    const auto index = [&](Cell p)
    {
        return p.y * board.width + p.x;
    };
    std::vector<int> routes(board.terrain.size(), 10000);
    std::queue<Cell> frontier;
    for (const auto *enemy : in_view)
    {
        if (kept == in_view.end() || enemy == *kept)
        {
            routes[index(enemy->cell)] = 0;
            frontier.push(enemy->cell);
        }
    }
    while (!frontier.empty())
    {
        const auto p = frontier.front();
        frontier.pop();
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
                if (routes[index(next)] <= routes[index(p)] + 1)
                {
                    continue;
                }
                routes[index(next)] = routes[index(p)] + 1;
                frontier.push(next);
            }
        }
    }
    const auto nearest = [&](Cell p)
    {
        return routes[index(p)];
    };
    // A free move (a Champion's) is taken toward the enemy, not given up.
    if (state.free_movement)
    {
        const Command *closer = nullptr;
        int closest = nearest(active.cell);
        for (const auto &command : offered)
        {
            if (command.verb == "move" && nearest(command.destination) < closest)
            {
                closer = &command;
                closest = nearest(command.destination);
            }
        }
        return closer ? *closer : offered.front();
    }
    // A choice of creatures left open casts on those already chosen. Magic
    // Missile and Scorching Ray cannot be cast early: every dart or ray goes
    // where the last one went while it can, else at the first creature offered.
    if (const auto &targeting = state.spell_targeting)
    {
        // A spell for several creatures, such as Bless, takes in more of the
        // side its first creature is on before it is cast.
        const auto side_of = [&](EntityId id)
        {
            return std::find_if(state.combatants.begin(), state.combatants.end(),
                                [&](const auto &a)
                                {
                                    return a.id == id;
                                });
        };
        if (!targeting->repeats && !targeting->chosen.empty())
        {
            const auto first = side_of(targeting->chosen.front());
            for (const auto &command : offered)
            {
                const auto other = side_of(command.target);
                if (command.verb == targeting->verb && first != state.combatants.end() &&
                    other != state.combatants.end() && other->side == first->side && !other->dead &&
                    std::find(targeting->chosen.begin(), targeting->chosen.end(), command.target) ==
                        targeting->chosen.end())
                {
                    return command;
                }
            }
        }
        for (const auto &command : offered)
        {
            if (command.verb == (targeting->chosen.empty() ? "spell_cancel" : "spell_cast"))
            {
                return command;
            }
        }
        const Command *aim = nullptr;
        for (const auto &command : offered)
        {
            if (command.verb == targeting->verb &&
                (!aim || command.target == targeting->chosen.back()))
            {
                aim = &command;
            }
        }
        if (aim)
        {
            return *aim;
        }
    }
    // Paladin's Smite costs no slot, so a Paladin uses it on the first hit it can;
    // slots stay for healing.
    for (const auto &command : offered)
    {
        if (command.verb == "divine_smite_free")
        {
            return command;
        }
    }
    // A smite that costs a slot only on a Critical Hit, from the lowest slot,
    // so slots stay for spells (#269).
    if (active.critical_hit)
    {
        for (const auto smite :
             {"divine_smite", "searing_smite", "shining_smite", "ensnaring_strike"})
        {
            const Command *lowest = nullptr;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == smite &&
                    !(command.spell.concentration &&
                      std::any_of(active.conditions.begin(), active.conditions.end(),
                                  [](const auto &c)
                                  {
                                      return c.source == "Concentrating";
                                  })) &&
                    (!lowest || cast_rank(command.verb, smite) < cast_rank(lowest->verb, smite)))
                {
                    lowest = &command;
                }
            }
            if (lowest)
            {
                return *lowest;
            }
        }
    }
    // Favored Enemy's Hunter's Mark and moving it cost no slot, and Horde
    // Breaker's attack, Superior Hunter's Prey and Divine Strike cost nothing,
    // so they come first.
    for (const auto verb :
         {"hunters_mark_move", "hunters_mark_free", "horde_breaker", "superior_hunters_prey",
          "divine_strike_radiant", "spiritual_weapon_strike"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return command;
            }
        }
    }
    // A Barbarian rages before it fights; its attacks keep the Rage going.
    for (const auto &command : offered)
    {
        if (command.verb == "rage")
        {
            return command;
        }
    }
    // Sacred Weapon lasts the fight, so it is taken before the first melee attack.
    const bool can_strike = std::any_of(offered.begin(), offered.end(),
                                        [](const auto &command)
                                        {
                                            return command.verb == "melee";
                                        });
    for (const auto &command : offered)
    {
        if (can_strike && command.verb == "sacred_weapon")
        {
            return command;
        }
    }
    // Burning at half its Hit Points or lower, a creature rolls on the ground
    // to put the fire out (#269); otherwise it fights on.
    for (const auto &command : offered)
    {
        if (command.verb == "extinguish" && active.hit_points * 2 <= active.max_hit_points)
        {
            return command;
        }
    }
    // A creature caught by vines spends its Action trying to break free.
    for (const auto &command : offered)
    {
        if (command.verb == "escape")
        {
            return command;
        }
    }
    for (const auto &command : offered)
    {
        // Shield only when its +5 turns the hit into a miss (#266), or
        // against Magic Missile, which it stops whole.
        const bool shield_helps =
            !state.reaction_attack ||
            state.reaction_attack->total < state.reaction_attack->armor_class + 5;
        if (command.verb == "opportunity" || (command.verb == "shield" && shield_helps) ||
            command.verb == "deflect" || command.verb == "redirect" || command.verb == "rebuke" ||
            command.verb == "giant_boon" || command.verb == "stones_endurance" ||
            command.verb == "storms_thunder" ||
            ((command.verb == "second_wind" || command.verb == "wholeness_of_body") &&
             active.hit_points * 2 <= active.max_hit_points))
        {
            return command;
        }
    }
    // The other reactions are used whenever offered; otherwise declined.
    if (state.reaction_pending)
    {
        for (const auto verb :
             {"uncanny_dodge", "superior_defense", "retaliate", "counterspell", "decline"})
        {
            for (const auto &command : offered)
            {
                if (command.verb == verb)
                {
                    return command;
                }
            }
        }
    }
    // The most wounded ally at half its Hit Points or lower is healed (#266),
    // by Lay On Hands first because it costs no spell slot. Borrowed view into
    // local offered commands.
    const auto heal_most_wounded =
        [&](std::initializer_list<std::string_view> verbs) -> const Command *
    {
        const Command *best = nullptr;
        const CombatantView *worst = nullptr;
        for (const auto &command : offered)
        {
            if (std::find(verbs.begin(), verbs.end(), command.verb) == verbs.end())
            {
                continue;
            }
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto &a)
                                               {
                                                   return a.id == command.target;
                                               });
            // The lower share of its maximum, compared without division.
            if (target.side == active.side && target.hit_points * 2 <= target.max_hit_points &&
                (!worst || static_cast<long long>(target.hit_points) * worst->max_hit_points <
                               static_cast<long long>(worst->hit_points) * target.max_hit_points))
            {
                best = &command;
                worst = &target;
            }
        }
        return best;
    };
    // Magic items first, as the original's Quick characters used their
    // readied ones, whether or not Quick magic is on (MAGIC-3).
    if (const auto item = choose_item(state, active, offered))
    {
        return *item;
    }
    // Lay On Hands and Divine Spark cost no spell slot.
    if (const auto *command = heal_most_wounded({"lay_on_hands", "divine_spark"}))
    {
        return *command;
    }
    // Preserve Life is offered only beside a Bloodied ally, and Lesser
    // Restoration only on a Blinded one.
    for (const auto &command : offered)
    {
        if (command.verb == "turn_undead" || command.verb == "preserve_life" ||
            command.verb.starts_with("lesser_restoration_") || command.verb == "spare_the_dying")
        {
            return command;
        }
    }
    if (const auto *command =
            heal_most_wounded({"cure_wounds", "cure_wounds_2", "healing_word", "healing_word_2",
                               "healing_word_innate_bonus"}))
    {
        return *command;
    }
    // A dying ally who is not healed is stabilized when no enemy is within reach.
    const bool threatened =
        std::any_of(state.combatants.begin(), state.combatants.end(),
                    [&](const auto &a)
                    {
                        const int dx = std::abs(a.cell.x - active.cell.x),
                                  dy = std::abs(a.cell.y - active.cell.y);
                        return a.side != active.side && a.conscious && std::max(dx, dy) <= 1;
                    });
    for (const auto &command : offered)
    {
        if (!threatened && command.verb == "stabilize")
        {
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto &a)
                                               {
                                                   return a.id == command.target;
                                               });
            if (target.side == active.side)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "shake_awake")
        {
            return command;
        }
    }
    const auto offers = [&](std::string_view verb) -> const Command *
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return &command;
            }
        }
        return nullptr;
    };
    const auto shows = [](const CombatantView &unit, std::string_view condition)
    {
        return std::any_of(unit.conditions.begin(), unit.conditions.end(),
                           [&](const auto &c)
                           {
                               return c.source == condition;
                           });
    };
    const auto unit = [&](EntityId id) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto &a)
                             {
                                 return a.id == id;
                             });
    };
    const bool concentrating = shows(active, "Concentrating");
    const auto beside = [](Cell a, Cell b)
    {
        return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) <= 1;
    };
    // Moonbeam's 5-foot radius is the target's square and those around it, so
    // it is aimed only at an enemy with no ally of the caster beside it.
    const auto clear_of_allies = [&](Cell center)
    {
        return std::none_of(state.combatants.begin(), state.combatants.end(),
                            [&](const auto &a)
                            {
                                return a.side == active.side && !a.dead && beside(a.cell, center);
                            });
    };
    if (state.area_targeting)
    {
        if (state.area_targeting->verb == "moonbeam" || state.area_targeting->verb == "lands_aid")
        {
            for (const auto &a : state.combatants)
            {
                if (a.side != active.side && a.conscious && clear_of_allies(a.cell) &&
                    std::max(std::abs(a.cell.x - active.cell.x),
                             std::abs(a.cell.y - active.cell.y)) <= 24)
                {
                    if (a.cell == state.area_targeting->center)
                    {
                        if (const auto *cast = offers("area_cast"))
                        {
                            return *cast;
                        }
                    }
                    for (const auto &command : offered)
                    {
                        if (command.verb == "area_move" && command.destination == a.cell)
                        {
                            return command;
                        }
                    }
                }
            }
        }
        // Any other area spell goes off where its drawn area holds an enemy
        // and no one of the caster's side; else it is aimed at the next enemy
        // where that is safe, else given up (#266).
        const auto &aimed = *state.area_targeting;
        const auto in_area = [&](const CombatantView &a)
        {
            return !a.dead &&
                   std::find(aimed.cells.begin(), aimed.cells.end(), a.cell) != aimed.cells.end();
        };
        if (aimed.verb != "moonbeam" && aimed.verb != "lands_aid")
        {
            const bool ally = std::any_of(state.combatants.begin(), state.combatants.end(),
                                          [&](const auto &a)
                                          {
                                              return a.side == active.side && in_area(a);
                                          });
            const bool enemy = std::any_of(in_view.begin(), in_view.end(),
                                           [&](const auto *a)
                                           {
                                               return a->conscious && in_area(*a);
                                           });
            if (enemy && !ally)
            {
                if (const auto *cast = offers("area_cast"))
                {
                    return *cast;
                }
            }
            for (const auto *target : in_view)
            {
                if (target->cell == aimed.center ||
                    !safe_area_target(state, active, aimed.spell, *target))
                {
                    continue;
                }
                for (const auto &command : offered)
                {
                    if (command.verb == "area_move" && command.destination == target->cell)
                    {
                        return command;
                    }
                }
            }
        }
        if (const auto *cancel = offers("spell_cancel"))
        {
            return *cancel;
        }
    }
    // Moonbeam reaches 120 feet: 24 squares, each 5 feet in any direction. A
    // target within it is always aimable, so aiming never starts over.
    const bool moonbeam_target =
        std::any_of(state.combatants.begin(), state.combatants.end(),
                    [&](const auto &a)
                    {
                        return a.side != active.side && a.conscious && clear_of_allies(a.cell) &&
                               std::max(std::abs(a.cell.x - active.cell.x),
                                        std::abs(a.cell.y - active.cell.y)) <= 24;
                    });

    // The extra attacks of Light and Nick weapons follow the Attack action at
    // the same target (#269).
    for (const auto verb :
         {"nick_melee", "nick_ranged", "nick_throw", "light_melee", "light_ranged", "light_throw"})
    {
        if (const auto *command = aim(verb))
        {
            return *command;
        }
    }
    // Bonus Action buffs come before the Action: Innate Sorcery before a spell
    // attack, a Druid's flame or staff, Hex and Bardic Inspiration on an ally.
    struct Buff
    {
        const char *verb, *condition;
    };

    for (const auto buff :
         {Buff{"innate_sorcery", "Innate Sorcery"}, Buff{"produce_flame", "Produce Flame"},
          Buff{"shillelagh", "Shillelagh"}})
    {
        if (const auto *command = offers(buff.verb);
            command && !shows(active, buff.condition) &&
            (std::string_view(buff.verb) != "innate_sorcery" || offers("eldritch_blast") ||
             offers("sorcerous_burst_fire")))
        {
            return *command;
        }
    }
    if (!concentrating)
    {
        for (const auto verb : {"hex", "spiritual_weapon"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "bardic_inspiration" && command.target != active.id &&
            !shows(unit(command.target), "Inspired"))
        {
            return command;
        }
    }
    // A Druid holds Moonbeam on the enemies, then fights as a Wolf; a Wolf
    // whose Moonbeam has ended leaves the form to cast it again.
    if (!active.form.empty() && !concentrating)
    {
        if (const auto *command = offers("leave_wild_shape"))
        {
            return *command;
        }
    }
    if (!concentrating && moonbeam_target)
    {
        if (const auto *command = offers("moonbeam"))
        {
            return *command;
        }
    }
    if (active.form.empty() && concentrating)
    {
        if (const auto *command = offers("wild_shape_wolf"))
        {
            return *command;
        }
    }
    if (const auto *command = offers("move_moonbeam"); command && moonbeam_target)
    {
        const auto &beamed = state.moonbeams;
        const bool burning =
            std::any_of(state.combatants.begin(), state.combatants.end(),
                        [&](const auto &a)
                        {
                            return a.side != active.side && a.conscious &&
                                   std::find(beamed.begin(), beamed.end(), a.cell) != beamed.end();
                        });
        if (!burning)
        {
            for (const auto &move : offered)
            {
                if (move.verb == "move_moonbeam" && unit(move.target).side != active.side &&
                    clear_of_allies(unit(move.target).cell))
                {
                    return move;
                }
            }
        }
    }
    // A spell with a slot, by the original's priorities (#266): a 1d7 roll,
    // then the priorities from 7 down to 8 less the roll, and at each one of
    // the spells that make sense, chosen at random. A harmful spell needs an
    // enemy in view not already under it, an area one a place where it
    // catches no one of the caster's side; a helpful one a creature of its
    // side not already under it. A spell that would end the caster's
    // Concentration waits.
    const auto makes_sense = [&](const Command &command, const AiSpell &spell)
    {
        if (command.aims_area)
        {
            return std::any_of(in_view.begin(), in_view.end(),
                               [&](const auto *enemy)
                               {
                                   return safe_area_target(state, active, command.spell, *enemy);
                               });
        }
        const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                         [&](const auto &a)
                                         {
                                             return a.id == command.target;
                                         });
        if (target == state.combatants.end() || shows_spell(*target, command.label))
        {
            return false;
        }
        if (spell.helpful)
        {
            return target->side == active.side && !target->dead;
        }
        return target->conscious &&
               std::find(in_view.begin(), in_view.end(), &*target) != in_view.end();
    };
    if (!in_view.empty())
    {
        const int roll = static_cast<int>(pick(state, active.id, 7, 7)) + 1;
        for (int priority = 7; priority >= 8 - roll; --priority)
        {
            std::vector<std::string_view> spells;
            for (const auto &command : offered)
            {
                const auto spell =
                    item_verb(command.verb) ? std::string_view{} : spell_of(active, command.verb);
                const auto *known = spell.empty() ? nullptr : ai_spell(spell);
                if (known && known->priority == priority &&
                    !(concentrating && command.spell.concentration) &&
                    makes_sense(command, *known) &&
                    std::find(spells.begin(), spells.end(), spell) == spells.end())
                {
                    spells.push_back(spell);
                }
            }
            if (spells.empty())
            {
                continue;
            }
            const auto spell = spells[pick(state, active.id, 77, spells.size())];
            const auto &known = *ai_spell(spell);
            // The lowest slot it is offered at, then the creature: its target
            // or the caster when one is offered, else one at random.
            int lowest = 1000;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == spell && makes_sense(command, known))
                {
                    lowest = std::min(lowest, cast_rank(command.verb, spell));
                }
            }
            std::vector<const Command *> casts;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == spell &&
                    cast_rank(command.verb, spell) == lowest && makes_sense(command, known))
                {
                    if (command.target == (known.helpful ? active.id : active.last_target) &&
                        (known.helpful || kept != in_view.end()))
                    {
                        return command;
                    }
                    casts.push_back(&command);
                }
            }
            return *casts[pick(state, active.id, 777, casts.size())];
        }
    }
    // A downed enemy is still offered only when it may rise again (a troll's
    // Regeneration): only Acid or Fire stops it, so finish it with one of those,
    // a spell or a Torch before a thrown flask that is used up.
    for (const auto verb :
         {"fire_bolt", "sorcerous_burst_fire", "scorching_ray", "acid_arrow", "hurl_flame",
          "flame_blade_strike", "torch", "throw_alchemists_fire", "throw_acid"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                                   [&](const auto &a)
                                                   {
                                                       return a.id == command.target;
                                                   });
                if (target.side != active.side && target.hit_points == 0)
                {
                    return command;
                }
            }
        }
    }
    // A standing regenerating enemy is set Burning with Alchemist's Fire: the fire
    // at the start of each of its turns keeps it from regenerating. Oil goes on
    // first when anyone on the side can follow with Alchemist's Fire for 5 more
    // damage; before a Fire spell or a Torch, an action spent on Oil is worth
    // less than a second attack.
    const auto target_of = [&](const Command &command) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto &a)
                             {
                                 return a.id == command.target;
                             });
    };
    const bool alchemists_fire_follows = std::any_of(
        state.combatants.begin(), state.combatants.end(),
        [&](const auto &ally)
        {
            return ally.side == active.side && ally.conscious &&
                   std::any_of(ally.thrown_gear_left.begin(), ally.thrown_gear_left.end(),
                               [](const auto &gear)
                               {
                                   return gear.first == "alchemists_fire" && gear.second > 0;
                               });
        });
    for (const auto &command : offered)
    {
        if (command.verb == "throw_oil" && alchemists_fire_follows)
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                !target.oiled && !target.burning)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "throw_alchemists_fire")
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                !target.burning)
            {
                return command;
            }
        }
    }
    // Fire stops the regeneration that would undo more than a spell's or a
    // weapon's damage, unless something already stopped it this round: a Fire
    // spell from afar, or a Torch in melee.
    for (const auto verb : {"scorching_ray", "fire_bolt", "sorcerous_burst_fire", "hurl_flame",
                            "flame_blade_strike", "torch"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                const auto &target = target_of(command);
                if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                    !target.burning && !target.regeneration_stopped)
                {
                    return command;
                }
            }
        }
    }
    // Attacks, the strongest kinds first. With no enemy beside it, a creature
    // whose ranged weapon rates more than half its melee weapon shoots, held
    // or drawn, as the original readied its bow (#266).
    const bool enemy_beside = std::any_of(
        in_view.begin(), in_view.end(),
        [&](const auto *enemy)
        {
            return enemy->conscious && std::max(std::abs(enemy->cell.x - active.cell.x),
                                                std::abs(enemy->cell.y - active.cell.y)) <= 1;
        });
    const bool shoots = !enemy_beside && active.ranged_rating * 2 > active.melee_rating;
    // A thrown weapon is rated as a ranged one, as the original did (#269).
    const bool throws = !enemy_beside && active.thrown_rating * 2 > active.melee_rating;
    // A Breath Weapon or Dragon's Breath goes off before the attacks, where it
    // catches an enemy and no ally (#269).
    for (const auto &command : offered)
    {
        if ((command.verb.starts_with("breath_weapon_") ||
             command.verb.starts_with("dragons_breath_exhale_")) &&
            std::any_of(in_view.begin(), in_view.end(),
                        [&](const auto *enemy)
                        {
                            return safe_area_target(state, active, command.spell, *enemy);
                        }))
        {
            return command;
        }
    }
    std::vector<std::string_view> attacks{"eldritch_blast", "sorcerous_burst_fire",
                                          "flame_blade_strike", "hurl_flame", "heat_metal_again"};
    if (shoots)
    {
        attacks.insert(attacks.end(), {"ranged", "shoot"});
    }
    if (throws)
    {
        attacks.push_back("throw");
    }
    attacks.insert(attacks.end(),
                   {"melee", "fire_bolt", "sacred_flame", "vicious_mockery", "starry_wisp",
                    "ray_of_frost", "chill_touch", "shocking_grasp", "poison_spray", "ranged"});
    // Able to attack without moving, a Rogue takes Steady Aim first (#269).
    if (const auto *steady = offers("steady_aim"); steady && active.action &&
                                                   std::any_of(attacks.begin(), attacks.end(),
                                                               [&](auto verb)
                                                               {
                                                                   return aim(verb) != nullptr;
                                                               }))
    {
        return *steady;
    }
    for (const auto verb : attacks)
    {
        if (const auto *command = aim(verb))
        {
            return *command;
        }
    }
    // Its Action spent, a Fighter surges for another while an enemy is in view
    // (#269).
    if (const auto *surge = offers("action_surge"); surge && !active.action && !in_view.empty())
    {
        return *surge;
    }
    // With the Action spent, a Bonus Action may still strike: Flurry of Blows,
    // a Martial Arts strike, Hex or Heat Metal again.
    if (!active.action)
    {
        for (const auto verb : {"flurry_of_blows", "martial_arts", "hex_move", "heat_metal_again"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    // With no action left, approaching for another attack cannot help this
    // turn. The bonus-action recovery choices above still get their chance.
    if (!active.action)
    {
        for (const auto &command : offered)
        {
            if (command.verb == "end")
            {
                return command;
            }
        }
    }
    // An unarmored Wizard or Sorcerer, or a monster magic-user, never closes
    // in, as the original's magic-users did not: it casts from where it stands.
    const bool stays_back = active.unarmored_arcane_caster && !active.panicked && !seeking;
    // Out of reach of its target even with all its movement, a Rogue or an Orc
    // dashes with its Bonus Action and keeps its Action to attack (#269).
    int reachable = nearest(active.cell);
    for (const auto &command : offered)
    {
        if (command.verb == "move")
        {
            reachable = std::min(reachable, nearest(command.destination));
        }
    }
    if (active.action && !stays_back && !in_view.empty() && reachable > 1 && reachable < 10000)
    {
        for (const auto verb : {"cunning_dash", "adrenaline_rush"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    // With nothing to attack yet, Aggressive covers more ground toward an
    // enemy in view.
    for (const auto &command : offered)
    {
        if (command.verb == "aggressive" && !in_view.empty() && !stays_back)
        {
            return command;
        }
    }
    const Command *move = nullptr;
    int closest = nearest(active.cell);
    for (const auto &command : offered)
    {
        if (!stays_back && command.verb == "move" && nearest(command.destination) < closest)
        {
            move = &command;
            closest = nearest(command.destination);
        }
    }
    if (move)
    {
        return *move;
    }
    // Unable to get any closer, a character draws a carried bow or crossbow and
    // shoots.
    if (const auto *shot = aim("shoot"))
    {
        return *shot;
    }
    // Stuck with no enemy it can strike in melee (and so none that can strike
    // it), a shield-bearer carrying a bow takes its shield off to shoot.
    if (!can_strike)
    {
        if (const auto *command = offers("doff_shield"))
        {
            return *command;
        }
    }
    // With its Action unused and an enemy beside it, it Dodges (#269).
    if (const auto *dodge = offers("dodge"); dodge && active.action && enemy_beside)
    {
        return *dodge;
    }
    for (const auto &command : offered)
    {
        if (command.verb == "end")
        {
            return command;
        }
    }
    return offered.front();
}
} // namespace

namespace
{
// Running off the field: the step toward an open edge of the field, around
// walls, Dashing for more movement, then the flee itself. A party member runs
// for the nearest edge; a panicked creature for the nearest edge square farther
// from its opponents than from itself (the original sends it away from them).
// Empty when no step gets it closer.
std::optional<Command> run_for_edge(const rules::Snapshot &state,
                                    const std::vector<Command> &offered,
                                    const rules::CombatantView &active)
{
    const auto offer = [&](std::string_view verb) -> const Command *
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb && command.actor == active.id)
            {
                return &command;
            }
        }
        return nullptr;
    };
    if (const auto *flee = offer("flee"))
    {
        return *flee;
    }
    // With an enemy beside it, it Disengages before running, so it is not hit
    // as it leaves (#269).
    const bool enemy_beside =
        std::any_of(state.combatants.begin(), state.combatants.end(),
                    [&](const auto &other)
                    {
                        return other.side != active.side && other.conscious &&
                               std::max(std::abs(other.cell.x - active.cell.x),
                                        std::abs(other.cell.y - active.cell.y)) <= 1;
                    });
    if (enemy_beside && !active.disengaged)
    {
        for (const auto verb : {"cunning_disengage", "disengage"})
        {
            if (const auto *disengage = offer(verb))
            {
                return *disengage;
            }
        }
    }
    const auto &board = state.battlefield;
    const auto cells = static_cast<std::size_t>(board.width * board.height);
    const auto index = [&](Cell p)
    {
        return static_cast<std::size_t>(p.y * board.width + p.x);
    };
    // Steps from the given squares to every open square, around walls.
    const auto distances = [&](const std::vector<Cell> &sources)
    {
        std::vector<int> steps(cells, -1);
        std::queue<Cell> frontier;
        for (const auto source : sources)
        {
            if (board.contains(source) && steps[index(source)] < 0)
            {
                steps[index(source)] = 0;
                frontier.push(source);
            }
        }
        while (!frontier.empty())
        {
            const auto p = frontier.front();
            frontier.pop();
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (const Cell next{p.x + dx, p.y + dy}; board.contains(next) &&
                                                             board.at(next) != Terrain::obstacle &&
                                                             steps[index(next)] < 0)
                    {
                        steps[index(next)] = steps[index(p)] + 1;
                        frontier.push(next);
                    }
                }
            }
        }
        return steps;
    };
    std::vector<Cell> edges;
    for (int y = 0; y < board.height; ++y)
    {
        for (int x = 0; x < board.width; ++x)
        {
            if ((x == 0 || y == 0 || x == board.width - 1 || y == board.height - 1) &&
                board.at({x, y}) != Terrain::obstacle)
            {
                edges.push_back({x, y});
            }
        }
    }
    if (active.side != Side::party)
    {
        std::vector<Cell> opponents;
        for (const auto &other : state.combatants)
        {
            if (other.side != active.side && other.hit_points > 0 && !other.dead && !other.fled)
            {
                opponents.push_back(other.cell);
            }
        }
        const auto mine = distances({active.cell}), theirs = distances(opponents);
        std::vector<Cell> away;
        for (const auto edge : edges)
        {
            if (mine[index(edge)] >= 0 &&
                (theirs[index(edge)] < 0 || theirs[index(edge)] > mine[index(edge)]))
            {
                away.push_back(edge);
            }
        }
        if (!away.empty())
        {
            edges = std::move(away);
        }
    }
    const auto to_edge = distances(edges);
    const auto steps_to_edge = [&](Cell p)
    {
        const auto d = to_edge[index(p)];
        return d < 0 ? 1 << 20 : d;
    };
    const Command *move = nullptr;
    int closest = steps_to_edge(active.cell);
    for (const auto &command : offered)
    {
        if (command.verb == "move" && command.actor == active.id &&
            steps_to_edge(command.destination) < closest)
        {
            move = &command;
            closest = steps_to_edge(command.destination);
        }
    }
    if (move)
    {
        return *move;
    }
    // Out of movement short of the edge: Dash for more.
    if (steps_to_edge(active.cell) > 0 && steps_to_edge(active.cell) < 1 << 20)
    {
        if (const auto *dash = offer("dash"))
        {
            return *dash;
        }
    }
    return std::nullopt;
}
} // namespace

namespace
{
// A carried item's use: a potion, wand, missile or dust (MAGIC-3), or a spell
// read from a scroll (MAGIC-6).
bool item_verb(std::string_view verb)
{
    return verb.ends_with("_item") || verb.ends_with("_item_bonus") ||
           verb.find("_scroll") != std::string_view::npos;
}

// Whether an item's area aimed at `center` might catch an ally, judged before
// aiming and on the safe side of the area the rules will draw: the necklace's
// Fireball within 5 squares of its center, and a lightning wand's bolt within
// a square of its line from the user out past the center.
bool area_risks_ally(const rules::Snapshot &state, const CombatantView &active,
                     std::string_view verb, Cell center)
{
    const auto lightning = verb.starts_with("lightning_bolt");
    return std::any_of(
        state.combatants.begin(), state.combatants.end(),
        [&](const auto &a)
        {
            if (a.side != active.side || a.dead)
            {
                return false;
            }
            if (!lightning)
            {
                return std::max(std::abs(a.cell.x - center.x), std::abs(a.cell.y - center.y)) <= 5;
            }
            if (a.id == active.id)
            {
                return false;
            }
            const double dx = center.x - active.cell.x, dy = center.y - active.cell.y;
            const double length = std::max(1.0, dx * dx + dy * dy);
            const double cx = a.cell.x - active.cell.x, cy = a.cell.y - active.cell.y;
            const double along = (cx * dx + cy * dy) / length;
            const double off_x = cx - along * dx, off_y = cy - along * dy;
            return along > 0 && off_x * off_x + off_y * off_y <= 2.0;
        });
}

// A magic item a Quick character uses now (MAGIC-3), as the original's did:
// healing for a creature at half its Hit Points or lower, Speed or Giant
// Strength once, a wand at an enemy (Paralysis at one not yet Paralyzed), a
// scroll's spell (MAGIC-6), and an area only where no ally can be caught. Dust
// of Disappearance is left to players: attacking ends its Invisibility.
std::optional<Command> choose_item(const rules::Snapshot &state, const CombatantView &active,
                                   const std::vector<Command> &offered)
{
    const auto unit = [&](EntityId id) -> const CombatantView *
    {
        const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.id == id;
                                        });
        return found == state.combatants.end() ? nullptr : &*found;
    };
    // An item's area being aimed: set off where the drawn area holds an enemy
    // and no ally, else aimed at the next safe enemy, else given up.
    if (const auto &aim = state.area_targeting; aim && item_verb(aim->verb))
    {
        const auto in_area = [&](const CombatantView &a)
        {
            return !a.dead &&
                   std::find(aim->cells.begin(), aim->cells.end(), a.cell) != aim->cells.end();
        };
        const bool ally = std::any_of(state.combatants.begin(), state.combatants.end(),
                                      [&](const auto &a)
                                      {
                                          return a.side == active.side && in_area(a);
                                      });
        const bool enemy =
            std::any_of(state.combatants.begin(), state.combatants.end(),
                        [&](const auto &a)
                        {
                            return a.side != active.side && a.conscious && in_area(a);
                        });
        for (const auto &command : offered)
        {
            if (enemy && !ally && command.verb == "area_cast")
            {
                return command;
            }
        }
        for (const auto &a : state.combatants)
        {
            if (a.side == active.side || !a.conscious || a.cell == aim->center ||
                area_risks_ally(state, active, aim->verb, a.cell))
            {
                continue;
            }
            for (const auto &command : offered)
            {
                if (command.verb == "area_move" && command.destination == a.cell)
                {
                    return command;
                }
            }
        }
        for (const auto &command : offered)
        {
            if (command.verb == "spell_cancel")
            {
                return command;
            }
        }
        return std::nullopt;
    }
    for (const auto &command : offered)
    {
        if (!item_verb(command.verb))
        {
            continue;
        }
        const auto *target = unit(command.target);
        const auto verb = std::string_view(command.verb);
        // A scroll's spell (MAGIC-6): healing for a creature at half its Hit
        // Points or lower, a spell at an enemy, or an area only where no ally
        // can be caught; spells that help an ally otherwise wait for the
        // player.
        if (verb.find("_scroll") != std::string_view::npos && !command.aims_area)
        {
            const bool heals = verb.starts_with("cure_wounds") || verb.starts_with("healing_word");
            if (heals ? target && target->side == active.side &&
                            target->hit_points * 2 <= target->max_hit_points
                      : target && target->side != active.side && target->conscious)
            {
                return command;
            }
            continue;
        }
        if (verb.starts_with("potion_of_healing") || verb.starts_with("potion_of_greater_healing"))
        {
            if (target && target->hit_points * 2 <= target->max_hit_points)
            {
                return command;
            }
        }
        else if (verb.starts_with("potion_of_speed") || verb.starts_with("potion_of_giant"))
        {
            if (target && target->id == active.id)
            {
                return command;
            }
        }
        else if (verb.starts_with("wand_of_paralysis"))
        {
            if (target && target->side != active.side &&
                std::none_of(target->conditions.begin(), target->conditions.end(),
                             [](const auto &c)
                             {
                                 return c.source.starts_with("Paralyzed");
                             }))
            {
                return command;
            }
        }
        else if (verb.starts_with("magic_missile"))
        {
            if (target && target->side != active.side)
            {
                return command;
            }
        }
        else if (command.aims_area)
        {
            const bool safe_enemy =
                std::any_of(state.combatants.begin(), state.combatants.end(),
                            [&](const auto &a)
                            {
                                return a.side != active.side && a.conscious &&
                                       !area_risks_ally(state, active, verb, a.cell);
                            });
            if (safe_enemy)
            {
                return command;
            }
        }
    }
    return std::nullopt;
}

// The commands a Quick member with magic off chooses from: all but the active
// creature's spells.
std::vector<Command> without_spells(const rules::Snapshot &state, std::vector<Command> commands)
{
    std::vector<std::string> spells;
    for (const auto &a : state.combatants)
    {
        if (a.id == state.actor)
        {
            spells = a.spells;
        }
    }
    // A magic item is used whether or not Quick magic is on (MAGIC-3).
    std::erase_if(commands,
                  [&](const auto &command)
                  {
                      return !item_verb(command.verb) &&
                             std::any_of(spells.begin(), spells.end(),
                                         [&](const auto &spell)
                                         {
                                             return command.verb == spell ||
                                                    command.verb.starts_with(spell + "_");
                                         });
                  });
    return commands;
}
} // namespace

Command choose_quick_command(const CombatSession &session, bool magic, Searching searching)
{
    if (magic)
    {
        return choose_demo_command(session, searching);
    }
    const auto state = session.snapshot();
    return choose_from(state, without_spells(state, session.legal_commands()), searching);
}

Command choose_flee_command(const CombatSession &session, bool magic, Searching searching)
{
    const auto state = session.snapshot();
    const auto offered = session.legal_commands();
    const auto active = std::find_if(state.combatants.begin(), state.combatants.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == state.actor;
                                     });
    const auto end = std::find_if(offered.begin(), offered.end(),
                                  [&](const auto &c)
                                  {
                                      return c.verb == "end" && c.actor == state.actor;
                                  });
    // Reactions, choices and those who cannot flee are left to the demo AI.
    if (active == state.combatants.end() || active->side != Side::party || !active->can_flee ||
        state.reaction_pending || end == offered.end())
    {
        return choose_quick_command(session, magic, searching);
    }
    if (const auto run = run_for_edge(state, offered, *active))
    {
        return *run;
    }
    return *end;
}
} // namespace opengold
