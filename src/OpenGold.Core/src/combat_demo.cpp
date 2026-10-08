#include "opengold/authored_items.h"
#include "opengold/combat_demo.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/character_pool.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>

namespace opengold
{
using namespace rules;
using namespace por;

namespace
{
Battlefield arena()
{
    Battlefield b{12, 9, std::vector<std::uint8_t>(108, 0)};
    for (int y = 2; y <= 6; ++y)
        if (y != 4)
            b.terrain[y * 12 + 6] = 1;
    b.terrain[4 * 12 + 5] = 2;
    b.terrain[4 * 12 + 6] = 2;
    return b;
}

std::vector<Participant> party()
{
    return {{1, "vanguard", "Vanguard", 0, {2, 2}},
        {2, "scout", "Scout", 0, {2, 4}},
        {3, "adept", "Adept", 0, {1, 3}},
        {4, "healer", "Healer", 0, {1, 5}}};
}

std::optional<Image> original_icon(const std::filesystem::path &directory, unsigned record)
{
    for (const auto &file : std::filesystem::directory_iterator(directory))
    {
        auto name = file.path().filename().string();
        for (auto &c : name)
            if (c >= 'a' && c <= 'z')
                c -= 32;
        if (name != "CPIC2.DAX")
            continue;
        if (std::filesystem::file_size(file.path()) > 32 * 1024 * 1024)
            throw std::runtime_error("Combat art archive exceeds limit");
        std::ifstream input(file.path(), std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        if (input.bad())
            throw std::runtime_error("Cannot read combat art");
        auto decoded = decode_ega_combat_icon(bytes, static_cast<std::uint8_t>(record), 0);
        if (!decoded)
            throw std::runtime_error("Cannot decode the script-selected combat icon");
        return std::move(decoded.image);
    }
    return std::nullopt;
}
} // namespace

CombatDemo::CombatDemo(std::unique_ptr<RulesModule> module) : module_(std::move(module))
{
    if (!module_)
        throw std::runtime_error("A combat rules module is required");
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
        party_->end_combat();
}

void CombatDemo::campaign_party(std::shared_ptr<CampaignParty> party)
{
    if (combat_)
        throw std::runtime_error("Attach party before starting combat");
    campaign_ = std::move(party);
}

void CombatDemo::synchronize_party()
{
    if (!campaign_combat_)
        return;
    const auto state = combat_->snapshot();
    campaign_->apply_combat(state);
    finish_campaign_combat(state.outcome);
}

void CombatDemo::finish_campaign_combat(Outcome outcome)
{
    if (outcome == Outcome::ongoing)
        return;
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
        throw std::runtime_error("Rules module returned no combat session");
    if (!campaign_)
    {
        combat_ = std::move(next);
        return;
    }
    if (campaign_->identity() != module_->identity())
        throw std::runtime_error("Party and combat rules differ");
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
    participants.insert(participants.end(), enemies.begin(), enemies.end());
    auto next = module_->create(
    {arena(), std::move(participants), campaign_ ? campaign_->state().next_combat_scope : 1},
    seed_);
    install_combat(std::move(next), std::move(reward_id));
}

void CombatDemo::encounter(CampaignEncounter encounter, std::uint64_t seed)
{
    if (!campaign_ || combat_)
        throw std::runtime_error("A new campaign combat owner is required");
    if (campaign_->identity() != module_->identity() || encounter.facing >= 4)
        throw std::runtime_error("Invalid campaign encounter context");
    const auto &board = encounter.field.geometry;
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64 ||
            board.terrain.size() != static_cast<std::size_t>(board.width * board.height) ||
            encounter.surprise > 3)
        throw std::runtime_error("Invalid campaign battlefield");
    // Converted party formation stays inside one reachable component of the
    // original geometry. Never erase walls or silently omit a participant.
    std::vector<Cell> cells;
    for (int y = 0; y < board.height; ++y)
        for (int x = 0; x < board.width; ++x)
            if (board.at({x, y}) != 1)
                cells.push_back({x, y});
    const auto distance = [](Cell a, Cell b)
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    };
    const Cell origin{25, 13};
    if (cells.empty())
        throw std::runtime_error("Battlefield has no open cells");
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
            for (int x = -1; x <= 1; ++x)
            {
                const Cell next{p.x + x, p.y + y};
                if ((!x && !y) || board.at(next) == 1)
                    continue;
                if (x && y && (board.at({p.x + x, p.y}) == 1 || board.at({p.x, p.y + y}) == 1))
                    continue;
                const auto index = next.y * board.width + next.x;
                if (seen[index])
                    continue;
                seen[index] = true;
                frontier.push(next);
            }
    }
    auto participants = campaign_->participants();
    for (auto &participant : participants)
        participant.resting = encounter.party_resting;
    if (cells.size() < participants.size() + encounter.enemies.size())
        throw std::runtime_error("Original battlefield cannot fit the complete encounter");
    if (!encounter.positions.empty())
    {
        if (encounter.positions.size() != participants.size() + encounter.enemies.size())
            throw std::runtime_error("Invalid authored combat formation");
        std::set<Cell> occupied;
        for (std::size_t i = 0; i < encounter.positions.size(); ++i)
        {
            const auto cell = encounter.positions[i];
            if (board.at(cell) == 1 || !occupied.insert(cell).second)
                throw std::runtime_error("Invalid authored combat position");
            if (i < participants.size())
                participants[i].cell = cell;
            else
                encounter.enemies[i - participants.size()].cell = cell;
        }
    }
    const auto place = [&](Participant & participant, Cell target)
    {
        const auto cell = std::min_element(cells.begin(), cells.end(),
                                           [&](Cell a, Cell b)
        {
            return distance(a, target) < distance(b, target);
        });
        participant.cell = *cell;
        cells.erase(cell);
    };
    for (auto &participant : participants)
    {
        if (encounter.positions.empty())
            place(participant, origin);
        participant.surprised = encounter.surprise == 1;
    }
    constexpr std::array<Cell, 4> forward{{{-5, -5}, {6, 0}, {5, 5}, {-6, 0}}};
    const auto offset = forward[encounter.facing];
    const Cell target{origin.x + offset.x, origin.y + offset.y};
    for (auto &enemy : encounter.enemies)
    {
        if (encounter.positions.empty())
            place(enemy, target);
        enemy.surprised = encounter.surprise == 2;
        participants.push_back(std::move(enemy));
    }
    auto next = module_->create(
    {encounter.field.geometry, std::move(participants), campaign_->state().next_combat_scope},
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
                                 const std::filesystem::path &body_catalog_file,
                                 std::span<const std::string> classes, unsigned level,
                                 std::span<const std::string> enemies,
                                 std::span<const std::string> gear)
{
    if (!rules)
        throw std::runtime_error("Combat demo requires combat rules");
    auto art = CharacterArt::load(game_directory);
    auto pool = character_pool(characters, art);
    const auto body_catalog = body_catalog_file.empty()
                              ? std::optional<CombatBodyCatalog> {}
                              :
                              std::optional<CombatBodyCatalog> {CombatBodyCatalog::load(
                                          body_catalog_file, body_catalog_file.parent_path() /
                                          "combat-weapon-options.tsv")
                                                               };
    auto party = std::make_shared<CampaignParty>(std::move(rules));
    CombatDemoSetup result{party, {}};
    result.encounter.field.geometry = {12, 12, std::vector<std::uint8_t>(144, 0)};
    result.encounter.field.tiles = std::vector<std::uint8_t>(144, 0);
    const std::array<std::string_view, 6> showcase{"fighter", "paladin", "cleric",
            "ranger",  "rogue",   "bard"};
    const std::array<unsigned, 6> weapons{36, 36, 23, 36, 8, 33};
    const std::array<Cell, 6> positions{{{5, 5}, {6, 5}, {7, 5}, {5, 6}, {6, 6}, {7, 6}}};
    if (classes.size() > positions.size())
        throw std::runtime_error("A play-test party has at most six members");
    const bool custom = !classes.empty();
    for (std::size_t i = 0; i < (custom ? classes.size() : showcase.size()); ++i)
    {
        const std::string_view klass = custom ? std::string_view(classes[i]) : showcase[i];
        // A repeated class takes the class's next pool variant.
        const auto earlier =
            custom ? unsigned(std::count(classes.begin(), classes.begin() + std::ptrdiff_t(i),
                                         std::string(klass)))
            : 0u;
        unsigned seen = 0;
        const auto found =
            std::find_if(pool.begin(), pool.end(),
                         [&](const Character & candidate)
        {
            if (candidate.creation_data().character_class != klass)
                return false;
            if (custom)
                return seen++ == earlier % 4;
            return i != 0 || candidate.creation_data().race == "goliath";
        });
        if (found == pool.end())
            throw std::runtime_error("Missing showcase hero in character pool");
        const auto id = party->add_pc(*found);
        party->set_wealth(id, {0, 0, 0, 10, 0, 0, 0});
        if (custom)
            outfit_pool_member(*party, id);
        else
            for (const unsigned type :
                    {
                        weapons[i], i < 2 ? 55u : 50u
                    })
            {
                Equipment item;
                item.stored.type = type;
                item.stored.stack_size = 1;
                item.stored.value = 1;
                party->purchase(id, item);
                party->equip(id, party->member(id).character.inventory().items().back().id);
            }

        for (const auto &key : gear)
        {
            Equipment flask;
            flask.stored.type = key == "oil"    ? original_item::flask_of_oil
                                : key == "acid" ? authored_item::acid
                                : key == "alchemists_fire" ? authored_item::alchemists_fire
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
                missing = resolved.selection.label;
            result.encounter.art.push_back(
            {id, resolved.icon(art, false), resolved.icon(art, true), missing});
        }
        else
            result.encounter.art.push_back(
        {id, art.icon(appearance, false), art.icon(appearance, true), missing});
    }
    // Every member gains the experience for `level` and takes default choices.
    if (level > 1)
    {
        party->award_experience(level >= 4 ? 2700 : level == 3 ? 900 : 300, "playtest:level");
        for (const auto id : party->state().slots)
            while (id && party->can_advance(id))
                party->advance(id, party->default_advancement(id));
    }
    result.encounter.positions.assign(positions.begin(),
                                      positions.begin() + std::ptrdiff_t(party->state().roster.size()));
    if (!enemies.empty())
    {
        // In a row east of the party, with their original Slums icons.
        for (std::size_t n = 0; n < enemies.size(); ++n)
        {
            const auto &definition = enemies[n];
            const unsigned icon = definition == "troll" ? 31 : definition == "ogre" ? 8 : 0;
            const auto picture = original_icon(game_directory, icon);
            if (!picture)
                throw std::runtime_error("Missing original combat icon for " + definition);
            const auto id = static_cast<EntityId>(1000 + n);
            const Cell cell{10, 4 + int(n)};
            result.encounter.enemies.push_back({id, definition, definition + " " +
                                                std::to_string(n + 1), 1, cell});
            result.encounter.positions.push_back(cell);
            result.encounter.art.push_back({id, *picture, original_icon(game_directory, icon + 128)});
        }
        return result;
    }
    const auto kobold = original_icon(game_directory, 0);
    const auto kobold_action = original_icon(game_directory, 128);
    const auto leader = original_icon(game_directory, 1);
    const auto leader_action = original_icon(game_directory, 129);
    if (!kobold || !leader)
        throw std::runtime_error("Missing original Kobold combat icon");
    constexpr std::array<Cell, 6> removed{{{6, 4}, {9, 4}, {9, 7}, {7, 8}, {4, 8}, {4, 5}}};
    unsigned number = 0;
    for (int y = 4; y <= 8; ++y)
        for (int x = 4; x <= 9; ++x)
        {
            if (x > 4 && x < 9 && y > 4 && y < 8)
                continue;
            const Cell cell{x, y};
            const bool is_leader = cell == Cell{6, 4};
            if (!is_leader && std::find(removed.begin(), removed.end(), cell) != removed.end())
                continue;
            const auto id = static_cast<EntityId>(1000 + result.encounter.enemies.size());
            result.encounter.enemies.push_back(
            {
                id, is_leader ? "slums-kobold-leader" : "slums-kobold",
                is_leader ? "Kobold Leader" : "Kobold " + std::to_string(++number), 1, cell});
            result.encounter.positions.push_back({x, y});
            result.encounter.art.push_back(
            {id, is_leader ? *leader : *kobold, is_leader ? leader_action : kobold_action});
        }
    return result;
}

const CombatSession &CombatDemo::combat() const
{
    if (!combat_)
        throw std::runtime_error("No active combat");
    return *combat_;
}

void CombatDemo::training(std::uint64_t seed, bool conditions)
{
    if (campaign_)
    {
        seed_ = seed;
        start_encounter({{1000, "bandit", "Bandit", 1, {9, 4}}}, "preview:bandit:v1");
        vm_.reset();
        art_.clear();
        dialogue_ = "Party combat preview. HP and spent resources carry back to the party.";
        status_ = "Party training";
        return;
    }
    auto next =
    module_->create({arena(),
        {   {
                1,
                conditions ? "blindness-adept" : "vanguard",
                conditions ? "Adept" : "Vanguard",
                0,
                {2, 4}
            },
            {10, "bandit", "Bandit", 1, conditions ? Cell{3, 4} : Cell{9, 4}}
        }},
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
        throw std::runtime_error("Finish combat before changing the party");
    const auto catalog = EclCatalog::load(directory);
    const auto program = catalog.find({"ECL2.DAX", 20});
    if (!program)
        throw std::runtime_error("Slums profile requires ECL2.DAX:20");
    auto creatures = CreatureCatalog::load(directory);
    for (auto record :
            {
                4, 13
            })
        if (!creatures.find({2, static_cast<std::uint8_t>(record)}))
            throw std::runtime_error("Missing Slums creature record");
    EclMachine vm(program);
    for (auto [first, last] : std::array<std::array<unsigned, 2>, 3>
{
    {{0x4900, 0x4cff}, {0x6b00, 0x6eff}, {0x9700, 0x98ff}}
})
    for (unsigned a = first; a <= last; ++a)
        vm.bind_variable(static_cast<std::uint16_t>(a), 0);
    vm.bind_variable(0xC04F, 1);
    for (std::uint8_t opcode :
            {
                11, 12, 13, 14, 28, 36
            })
        vm.enable_host(opcode);
    if (!vm.start(1))
        throw std::runtime_error("Cannot enter Slums event 1");
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
        return;
    for (unsigned step = 0; step < 64; ++step)
    {
        const auto result = vm_->run(1000);
        if (result.state == EclState::faulted)
            throw std::runtime_error(result.diagnostic);
        if (result.state == EclState::completed)
        {
            status_ = "Script complete; event flag " + std::to_string(vm_->variable(0x4ACA)) +
                      ", fight count " + std::to_string(vm_->variable(0x4ABB));
            return;
        }
        if (!result.request)
            continue;
        const auto &request = *result.request;
        if (request.kind == EclRequestKind::text)
        {
            if (request.clear)
                dialogue_.clear();
            dialogue_ += request.text;
            if (dialogue_.size() > 8192)
                throw std::runtime_error("Slums dialogue exceeds limit");
            if (!vm_->resume(request.id))
                throw std::runtime_error("Text acknowledgement rejected");
        }
        else if (request.kind == EclRequestKind::menu)
        {
            if (request.choices.size() != 1)
                throw std::runtime_error("Unsupported Slums menu");
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
                if (a.size() != 3 || a[0].value != record || a[1].value != count ||
                        a[2].value != 4 || enemies_.size() > 1)
                    throw std::runtime_error("Unrecognized Slums creature/count/icon profile");
                const auto &creature =
                    creatures_->find({2, static_cast<std::uint8_t>(record)})->get();
                const auto icon = original_icon(game_directory_, a[2].value);
                const auto action = original_icon(game_directory_, a[2].value + 128);
                for (unsigned i = 0; i < count; ++i)
                {
                    const auto id = static_cast<EntityId>(1000 + enemies_.size());
                    enemies_.push_back(
                    {
                        id,
                        "slums-orc",
                        creature.stored.name + " " + std::to_string(enemies_.size() + 1),
                        1,
                        {9, 2 + static_cast<int>(enemies_.size())}});
                    if (icon)
                        art_.push_back({id, *icon, action});
                }
            }
            else if (opcode == 36)
            {
                if (enemies_.size() != 4 || encounters_ != 0 || vm_->variable(0x6DC6) != 99 ||
                        vm_->variable(0x6DCB) != 0)
                    throw std::runtime_error("Unsupported Slums combat context");
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
                throw std::runtime_error("Unsupported Slums presentation service");
            if (!vm_->resume_host(request.id, {}))
                throw std::runtime_error("Slums host acknowledgement rejected");
        }
        else
            throw std::runtime_error("Unsupported Slums input");
    }
    throw std::runtime_error("Slums script exceeded request budget");
}

void CombatDemo::continue_script()
{
    if (!vm_ || !menu_ticket_)
        return;
    if (!vm_->resume(menu_ticket_, 0))
        throw std::runtime_error("Slums menu acknowledgement rejected");
    menu_ticket_ = 0;
    pump();
}

void CombatDemo::finish_combat()
{
    if (!vm_ || !combat_ticket_ || !combat_)
        return;
    const auto state = combat_->snapshot();
    if (state.outcome == Outcome::ongoing)
        return;
    unsigned defeated = 0;
    for (const auto &unit : state.combatants)
        if (unit.side == 1 && unit.hit_points == 0)
            ++defeated;
    EclHostReply reply;
    reply.writes =
    {
        {0x6DC7, static_cast<std::uint16_t>(state.outcome == Outcome::victory ? 0 : 128)},
        {0x6DC8, static_cast<std::uint16_t>(defeated)},
        {0x6DCB, 0},
        {0x6DE3, 0},
        {0x6E70, 0},
        {0x6E71, 0},
        {0x6E72, 0}
    };
    if (!vm_->resume_host(combat_ticket_, reply))
        throw std::runtime_error("Combat outcome rejected by ECL");
    combat_ticket_ = 0;
    pump();
}

bool CombatDemo::submit(const Command &command)
{
    if (!combat_ || !combat_->submit(command))
        return false;
    synchronize_party();
    finish_combat();
    return true;
}

void CombatDemo::revisit()
{
    if (!script_complete() || !combat_ || combat_->snapshot().outcome != Outcome::victory)
        throw std::runtime_error("Revisit requires completed victory");
    if (!vm_->start(1))
        throw std::runtime_error("Slums revisit rejected");
    dialogue_.clear();
    pump();
}

std::string CombatDemo::save_combat() const
{
    if (vm_ || campaign_)
        throw std::runtime_error(
            "Campaign checkpoints are pending; save/load currently supports training combat only");
    return combat().save();
}

void CombatDemo::restore_combat(std::string_view checkpoint)
{
    if (vm_ || campaign_)
        throw std::runtime_error("Cannot replace a campaign combat with a training checkpoint");
    auto restored = module_->restore(checkpoint);
    combat_ = std::move(restored);
}

unsigned CombatDemo::script_variable(std::uint16_t address) const
{
    if (!vm_)
        throw std::runtime_error("No active campaign script");
    return vm_->variable(address);
}

Command choose_demo_command(const CombatSession &session)
{
    const auto state = session.snapshot();
    const auto offered = session.legal_commands();
    if (offered.empty())
        throw std::runtime_error("No legal combat command");
    const auto &active = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == state.actor;
    });
    // The module orders its pending check choices by its default AI preference.
    if (!state.initiative_choices.empty() || state.effect_targeting ||
            state.optional_effect_choice || state.ability_check_choice || state.free_movement)
        return offered.front();
    if (state.temporary_hp_offer)
    {
        const auto verb =
            state.temporary_hp_offer->current.amount >= state.temporary_hp_offer->offered.amount
            ? "temp_hp_keep"
            : "temp_hp_use";
        for (const auto &command : offered)
            if (command.verb == verb)
                return command;
    }
    for (const auto &command : offered)
        if (command.verb == "stand_up")
            return command;
    // Rank offered destinations by a geometric route around obstacles. Straight
    // distance alone can strand both sides on opposite corners of a wall.
    // This is an AI heuristic; legal movement and its costs remain module-owned.
    const auto &board = state.battlefield;
    const auto index = [&](Cell p)
    {
        return p.y * board.width + p.x;
    };
    std::vector<int> routes(board.terrain.size(), 10000);
    std::queue<Cell> frontier;
    for (const auto &a : state.combatants)
        if (a.side != active.side && !a.dead && a.hit_points > 0)
        {
            routes[index(a.cell)] = 0;
            frontier.push(a.cell);
        }
    while (!frontier.empty())
    {
        const auto p = frontier.front();
        frontier.pop();
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                const Cell next{p.x + x, p.y + y};
                if ((!x && !y) || board.at(next) == 1)
                    continue;
                if (x && y && (board.at({p.x + x, p.y}) == 1 || board.at({p.x, p.y + y}) == 1))
                    continue;
                if (routes[index(next)] <= routes[index(p)] + 1)
                    continue;
                routes[index(next)] = routes[index(p)] + 1;
                frontier.push(next);
            }
    }
    const auto nearest = [&](Cell p)
    {
        return routes[index(p)];
    };
    // A choice of creatures left open casts on those already chosen.
    if (state.spell_targeting)
        for (const auto &command : offered)
            if (command.verb == (state.spell_targeting->chosen.empty() ? "spell_cancel"
                                 : "spell_cast"))
                return command;
    // Paladin's Smite costs no slot, so a Paladin uses it on the first hit it can;
    // slots stay for healing.
    for (const auto &command : offered)
        if (command.verb == "divine_smite_free")
            return command;
    // Favored Enemy's Hunter's Mark and moving it cost no slot, and Horde
    // Breaker's attack costs nothing, so they come first.
    for (const auto verb : {"hunters_mark_move", "hunters_mark_free", "horde_breaker",
                            "spiritual_weapon_strike"
                           })
        for (const auto &command : offered)
            if (command.verb == verb)
                return command;
    // A Barbarian rages before it fights; its attacks keep the Rage going.
    for (const auto &command : offered)
        if (command.verb == "rage")
            return command;
    // Sacred Weapon lasts the fight, so it is taken before the first melee attack.
    const bool can_strike = std::any_of(offered.begin(), offered.end(), [](const auto & command)
    {
        return command.verb == "melee";
    });
    for (const auto &command : offered)
        if (can_strike && command.verb == "sacred_weapon")
            return command;
    // A creature caught by vines spends its Action trying to break free.
    for (const auto &command : offered)
        if (command.verb == "escape")
            return command;
    for (const auto &command : offered)
        if (command.verb == "opportunity" || command.verb == "shield" ||
                command.verb == "deflect" || command.verb == "redirect" ||
                command.verb == "rebuke" || command.verb == "inspire" ||
                command.verb == "cutting" ||
                (command.verb == "second_wind" && active.hit_points * 2 <= active.max_hit_points))
            return command;
    // An ally below half Hit Points is healed, by Lay On Hands first because it
    // costs no spell slot. Borrowed view into local offered commands.
    const auto heal_below_half =
        [&](std::initializer_list<std::string_view> verbs) -> const Command *
    {
        for (const auto &command : offered)
            if (std::find(verbs.begin(), verbs.end(), command.verb) != verbs.end())
            {
                const auto &target =
                    *std::find_if(state.combatants.begin(), state.combatants.end(),
                                  [&](const auto & a)
                {
                    return a.id == command.target;
                });
                if (target.side == active.side && target.hit_points * 2 < target.max_hit_points)
                    return &command;
            }
        return nullptr;
    };
    // Lay On Hands and Divine Spark cost no spell slot.
    if (const auto *command = heal_below_half({"lay_on_hands", "divine_spark"}))
        return *command;
    // Preserve Life is offered only beside a Bloodied ally, and Lesser
    // Restoration only on a Blinded one.
    for (const auto &command : offered)
        if (command.verb == "turn_undead" || command.verb == "preserve_life" ||
                command.verb == "lesser_restoration" || command.verb == "spare_the_dying")
            return command;
    if (const auto *command = heal_below_half({"cure_wounds", "cure_wounds_2", "healing_word",
                                                "healing_word_2"
                                               }))
        return *command;
    // A dying ally who is not healed is stabilized when no enemy is within reach.
    const bool threatened =
        std::any_of(state.combatants.begin(), state.combatants.end(), [&](const auto & a)
    {
        const int dx = std::abs(a.cell.x - active.cell.x), dy = std::abs(a.cell.y - active.cell.y);
        return a.side != active.side && a.conscious && std::max(dx, dy) <= 1;
    });
    for (const auto &command : offered)
        if (!threatened && command.verb == "stabilize")
        {
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto & a)
            {
                return a.id == command.target;
            });
            if (target.side == active.side)
                return command;
        }
    for (const auto &command : offered)
        if (command.verb == "shake_awake")
            return command;
    const auto offers = [&](std::string_view verb) -> const Command *
    {
        for (const auto &command : offered)
            if (command.verb == verb)
                return &command;
        return nullptr;
    };
    const auto shows = [](const CombatantView & unit, std::string_view condition)
    {
        return std::any_of(unit.conditions.begin(), unit.conditions.end(),
                           [&](const auto & c)
        {
            return c.source == condition;
        });
    };
    const auto unit = [&](EntityId id) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto & a)
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
        return std::none_of(state.combatants.begin(), state.combatants.end(), [&](const auto & a)
        {
            return a.side == active.side && !a.dead && beside(a.cell, center);
        });
    };
    if (state.area_targeting)
    {
        if (state.area_targeting->verb == "moonbeam" || state.area_targeting->verb == "lands_aid")
        {
            for (const auto &a : state.combatants)
                if (a.side != active.side && a.conscious && clear_of_allies(a.cell) &&
                        std::max(std::abs(a.cell.x - active.cell.x),
                                 std::abs(a.cell.y - active.cell.y)) <= 24)
                {
                    if (a.cell == state.area_targeting->center)
                        if (const auto *cast = offers("area_cast"))
                            return *cast;
                    for (const auto &command : offered)
                        if (command.verb == "area_move" && command.destination == a.cell)
                            return command;
                }
        }
        if (const auto *cancel = offers("spell_cancel"))
            return *cancel;
    }
    // Moonbeam reaches 120 feet: 24 squares, each 5 feet in any direction. A
    // target within it is always aimable, so aiming never starts over.
    const bool moonbeam_target = std::any_of(state.combatants.begin(), state.combatants.end(),
                                 [&](const auto & a)
    {
        return a.side != active.side && a.conscious && clear_of_allies(a.cell) &&
               std::max(std::abs(a.cell.x - active.cell.x), std::abs(a.cell.y - active.cell.y)) <= 24;
    });
    // Bonus Action buffs come before the Action: Innate Sorcery before a spell
    // attack, a Druid's flame or staff, Hex and Bardic Inspiration on an ally.
    struct Buff
    {
        const char *verb, *condition;
    };
    for (const auto buff : {Buff{"innate_sorcery", "Innate Sorcery"},
                            Buff{"produce_flame", "Produce Flame"}, Buff{"shillelagh", "Shillelagh"}
                           })
        if (const auto *command = offers(buff.verb);
                command && !shows(active, buff.condition) &&
                (std::string_view(buff.verb) != "innate_sorcery" || offers("eldritch_blast") ||
                 offers("sorcerous_burst_fire")))
            return *command;
    if (!concentrating)
        for (const auto verb : {"hex", "spiritual_weapon"})
            if (const auto *command = offers(verb))
                return *command;
    for (const auto &command : offered)
        if (command.verb == "bardic_inspiration" && command.target != active.id &&
                !shows(unit(command.target), "Inspired"))
            return command;
    // A Druid holds Moonbeam on the enemies, then fights as a Wolf; a Wolf
    // whose Moonbeam has ended leaves the form to cast it again.
    if (!active.form.empty() && !concentrating)
        if (const auto *command = offers("leave_wild_shape"))
            return *command;
    if (!concentrating && moonbeam_target)
        if (const auto *command = offers("moonbeam"))
            return *command;
    if (active.form.empty() && concentrating)
        if (const auto *command = offers("wild_shape_wolf"))
            return *command;
    if (const auto *command = offers("move_moonbeam"); command && moonbeam_target)
    {
        const auto &beamed = state.moonbeams;
        const bool burning = std::any_of(state.combatants.begin(), state.combatants.end(),
                                         [&](const auto & a)
        {
            return a.side != active.side && a.conscious &&
                   std::find(beamed.begin(), beamed.end(), a.cell) != beamed.end();
        });
        if (!burning)
            for (const auto &move : offered)
                if (move.verb == "move_moonbeam" && unit(move.target).side != active.side &&
                        clear_of_allies(unit(move.target).cell))
                    return move;
    }
    for (const auto &command : offered)
        if (command.verb == "blindness" || command.verb == "hold_person" ||
                command.verb == "hideous_laughter" || command.verb == "ray_of_enfeeblement")
        {
            const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                             [&](const auto & a)
            {
                return a.id == command.target;
            });
            if (target->conditions.empty())
                return command;
        }
    // A downed enemy is still offered only when it may rise again (a troll's
    // Regeneration): only Acid or Fire stops it, so finish it with one of those,
    // a spell or a Torch before a thrown flask that is used up.
    for (const auto verb :
            {"fire_bolt", "sorcerous_burst_fire", "scorching_ray", "acid_arrow", "hurl_flame",
             "flame_blade_strike", "torch", "throw_alchemists_fire", "throw_acid"
            })
        for (const auto &command : offered)
            if (command.verb == verb)
            {
                const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                                   [&](const auto & a)
                {
                    return a.id == command.target;
                });
                if (target.side != active.side && target.hit_points == 0)
                    return command;
            }
    // A standing regenerating enemy is set Burning with Alchemist's Fire: the fire
    // at the start of each of its turns keeps it from regenerating. Oil goes on
    // first when anyone on the side can follow with Alchemist's Fire for 5 more
    // damage; before a Fire spell or a Torch, an action spent on Oil is worth
    // less than a second attack.
    const auto target_of = [&](const Command &command) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto & a)
        {
            return a.id == command.target;
        });
    };
    const bool alchemists_fire_follows =
        std::any_of(state.combatants.begin(), state.combatants.end(), [&](const auto & ally)
    {
        return ally.side == active.side && ally.conscious &&
               std::any_of(ally.thrown_gear_left.begin(), ally.thrown_gear_left.end(),
                           [](const auto & gear)
        {
            return gear.first == "alchemists_fire" && gear.second > 0;
        });
    });
    for (const auto &command : offered)
        if (command.verb == "throw_oil" && alchemists_fire_follows)
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                    !target.oiled && !target.burning)
                return command;
        }
    for (const auto &command : offered)
        if (command.verb == "throw_alchemists_fire")
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                    !target.burning)
                return command;
        }
    // Fire stops the regeneration that would undo more than a spell's or a
    // weapon's damage, unless something already stopped it this round: a Fire
    // spell from afar, or a Torch in melee.
    for (const auto verb : {"scorching_ray", "fire_bolt", "sorcerous_burst_fire", "hurl_flame",
                            "flame_blade_strike", "torch"
                           })
        for (const auto &command : offered)
            if (command.verb == verb)
            {
                const auto &target = target_of(command);
                if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                        !target.burning && !target.regeneration_stopped)
                    return command;
            }
    for (const auto verb :
            {"magic_missile", "magic_missile_2", "scorching_ray", "acid_arrow", "mind_spike",
             "inflict_wounds", "guiding_bolt", "dissonant_whispers", "eldritch_blast",
             "sorcerous_burst_fire", "flame_blade_strike", "hurl_flame", "heat_metal_again",
             "melee", "fire_bolt", "sacred_flame", "vicious_mockery", "starry_wisp",
             "ray_of_frost", "chill_touch", "shocking_grasp", "poison_spray", "ranged"
            })
    {
        const Command *best = nullptr;
        int hp = 100000; // Borrowed view into local offered commands.
        for (const auto &command : offered)
            if (command.verb == verb)
            {
                const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                                   [&](const auto & a)
                {
                    return a.id == command.target;
                });
                // Some attack spells, such as Eldritch Blast, may target any
                // creature; the policy attacks only the other side.
                if (target.side != active.side && target.hit_points > 0 && target.hit_points < hp)
                {
                    best = &command;
                    hp = target.hit_points;
                }
            }
        if (best)
            return *best;
    }
    // With the Action spent, a Bonus Action may still strike: Flurry of Blows,
    // a Martial Arts strike, Hex or Heat Metal again.
    if (!active.action)
        for (const auto verb : {"flurry_of_blows", "martial_arts", "hex_move", "heat_metal_again"})
            if (const auto *command = offers(verb))
                return *command;
    // With no action left, approaching for another attack cannot help this
    // turn. The bonus-action recovery choices above still get their chance.
    if (!active.action)
        for (const auto &command : offered)
            if (command.verb == "end")
                return command;
    // The nearest square with a clear line to an enemy, when none is in view here.
    const auto sees_enemy = [&](Cell from)
    {
        return std::any_of(state.combatants.begin(), state.combatants.end(), [&](const auto & a)
        {
            return a.side != active.side && a.hit_points > 0 &&
                   rules::has_line_of_sight(board, from, a.cell);
        });
    };
    const auto step_into_view = [&]() -> const Command *
    {
        if (sees_enemy(active.cell))
            return nullptr;
        const Command *step = nullptr;
        int shortest = 1000;
        for (const auto &command : offered)
            if (command.verb == "move" && sees_enemy(command.destination))
            {
                const int steps = std::max(std::abs(command.destination.x - active.cell.x),
                                           std::abs(command.destination.y - active.cell.y));
                if (steps < shortest)
                {
                    step = &command;
                    shortest = steps;
                }
            }
        return step;
    };
    // With nothing to attack yet, Aggressive covers more ground toward the enemy.
    for (const auto &command : offered)
        if (command.verb == "aggressive")
            return command;
    // A character with a ranged attack cantrip that sees no enemy steps into
    // view to cast from there rather than walking up to the enemy.
    const auto &cantrips = active.known_cantrips;
    const bool casts_from_afar = std::any_of(cantrips.begin(), cantrips.end(), [](const auto & id)
    {
        return id == "fire_bolt" || id == "eldritch_blast" || id == "ray_of_frost" ||
               id == "sacred_flame" || id == "vicious_mockery" || id == "starry_wisp" ||
               id == "produce_flame" || id == "sorcerous_burst";
    });
    if (casts_from_afar)
        if (const auto *step = step_into_view())
            return *step;
    const Command *move = nullptr;
    int closest = nearest(active.cell);
    for (const auto &command : offered)
        if (command.verb == "move" && nearest(command.destination) < closest)
        {
            move = &command;
            closest = nearest(command.destination);
        }
    if (move)
        return *move;
    // Unable to get any closer, a character draws a carried bow or crossbow and
    // shoots the weakest enemy in range.
    const Command *shot = nullptr;
    int weakest = 100000;
    for (const auto &command : offered)
        if (command.verb == "shoot")
        {
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto & a)
            {
                return a.id == command.target;
            });
            if (target.hit_points > 0 && target.hit_points < weakest)
            {
                shot = &command;
                weakest = target.hit_points;
            }
        }
    if (shot)
        return *shot;
    // Seeing no enemy from here (behind a wall corner, say), a character steps to
    // the nearest square with a clear line to one, to shoot or cast from there.
    if (const auto *step = step_into_view())
        return *step;
    // Stuck with no enemy it can strike in melee (and so none that can strike
    // it), a shield-bearer carrying a bow takes its shield off to shoot.
    if (!can_strike)
        if (const auto *command = offers("doff_shield"))
            return *command;
    for (const auto &command : offered)
        if (command.verb == "end")
            return command;
    return offered.front();
}
} // namespace opengold
