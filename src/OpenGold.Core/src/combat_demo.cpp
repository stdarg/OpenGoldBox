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
    status_ = "Slums encounter / original dungeon geometry";
    dialogue_ = "The original script has requested combat.";
}

CombatDemoSetup make_combat_demo(std::unique_ptr<RulesModule> rules,
                                 const CharacterRules &characters,
                                 const std::filesystem::path &game_directory,
                                 const std::filesystem::path &body_catalog_file)
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
    const std::array<std::string_view, 6> classes{"fighter", "paladin", "cleric",
            "ranger",  "rogue",   "bard"};
    const std::array<unsigned, 6> weapons{36, 36, 23, 36, 8, 33};
    const std::array<Cell, 6> positions{{{5, 5}, {6, 5}, {7, 5}, {5, 6}, {6, 6}, {7, 6}}};
    for (std::size_t i = 0; i < classes.size(); ++i)
    {
        const auto found =
            std::find_if(pool.begin(), pool.end(),
                         [&](const Character & candidate)
        {
            return candidate.creation_data().character_class == classes[i] &&
                   (i != 0 || candidate.creation_data().race == "goliath");
        });
        if (found == pool.end())
            throw std::runtime_error("Missing showcase hero in character pool");
        const auto id = party->add_pc(*found);
        party->set_wealth(id, {0, 0, 0, 10, 0, 0, 0});
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
    result.encounter.positions.assign(positions.begin(), positions.end());
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
        if (command.verb == "opportunity" ||
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
        if (command.verb == "blindness" || command.verb == "hold_person")
        {
            const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                             [&](const auto & a)
            {
                return a.id == command.target;
            });
            if (target->conditions.empty())
                return command;
        }
    for (const auto verb :
            {"magic_missile", "magic_missile_2", "scorching_ray", "inflict_wounds", "guiding_bolt", "melee",
             "fire_bolt", "sacred_flame", "ranged"
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
                if (target.hit_points < hp)
                {
                    best = &command;
                    hp = target.hit_points;
                }
            }
        if (best)
            return *best;
    }
    // With no action left, approaching for another attack cannot help this
    // turn. The bonus-action recovery choices above still get their chance.
    if (!active.action)
        for (const auto &command : offered)
            if (command.verb == "end")
                return command;
    // With nothing to attack yet, Aggressive covers more ground toward the enemy.
    for (const auto &command : offered)
        if (command.verb == "aggressive")
            return command;
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
    for (const auto &command : offered)
        if (command.verb == "end")
            return command;
    return offered.front();
}
} // namespace opengold
