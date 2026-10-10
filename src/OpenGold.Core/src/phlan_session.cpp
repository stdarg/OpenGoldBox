#include "opengold/rolf_tour.h"
#include "opengold/random_treasure.h"
#include "opengold/ecl_party_host.h"
#include <algorithm>
#include <set>

namespace opengold::por
{
// Arrow volleys come from Norris the Gray's hidden kobold archers: each arrow is
// an attack roll with the SRD 5.2.1 Kobold Warrior's +4 ranged bonus.
constexpr int hidden_archer_attack_bonus = 4;

const PhlanResources &RolfTourSession::area_resources() const
{
    return current_area_ ? *town_->districts.at(current_area_) : *town_;
}

void RolfTourSession::change_area(unsigned id)
{
    if (id == current_area_)
        return;
    const auto &resource = id ? *town_->districts.at(id) : *town_;
    if (!resource.map)
        throw EclError("District map is missing");
    visited_areas_[current_area_] = snapshot_.visited;
    seen_areas_[current_area_] = snapshot_.seen;
    current_area_ = id;
    snapshot_.area_id = id;
    map_ = *resource.map;
    wall_art_ = resource.wall_art;
    snapshot_.visited = visited_areas_[id];
    snapshot_.seen = seen_areas_[id];
    picture_.reset();
    ++snapshot_.picture_revision;
    snapshot_.sprite_frame = -1;
    ++snapshot_.revision;
}

void RolfTourSession::campaign_party(std::shared_ptr<opengold::CampaignParty> party)
{
    campaign_ = std::move(party);
    restart();
}

void RolfTourSession::attach_restored_party(std::shared_ptr<opengold::CampaignParty> party)
{
    campaign_ = std::move(party);
    if (campaign_)
        for (const std::uint8_t op :
                {
                    11, 29, 30, 34, 35, 41, 46, 54
                })
            machine_.enable_host(op);
}

namespace
{

// GEO door code 2. The original engine answers it with "Locked." and Bash/Pick/
// Knock/Exit; code 3 (wizard locked) needs magic or exceptional AD&D Strength.
constexpr std::uint8_t locked_door = 2;
// Authored SRD conversion of the original Strength-table bash: a Medium (DC 15)
// Strength (Athletics) check.
constexpr int locked_door_difficulty = 15;

struct Edge
{
    unsigned x{}, y{};
    MapDirection side{};
    unsigned next_x{}, next_y{};
    MapDirection other{};
};

Edge edge_ahead(PartyPose pose)
{
    const int x = static_cast<int>(pose.x) + step_x(pose.facing),
              y = static_cast<int>(pose.y) + step_y(pose.facing);
    return {pose.x, pose.y, pose.facing, static_cast<unsigned>((x + 16) % 16),
            static_cast<unsigned>((y + 16) % 16), reversed(pose.facing)};
}
} // namespace

void RolfTourSession::configure_town()
{
    // Campaign flags, area-local flags, host registers and scratch strings have
    // explicit lifetimes. These are logical VM cells, never native addresses.
    for (unsigned a = 0x49C3; a <= 0x4AFF; ++a)
        machine_.bind_variable(a, 0);
    for (unsigned a = 0x6B00; a <= 0x6C1C; ++a)
        machine_.bind_variable(a, 0);
    for (unsigned a = 0x6DA8; a <= 0x6EFF; ++a)
        machine_.bind_variable(a, 0);
    for (unsigned a = 0x9800; a <= 0x98FF; ++a)
        machine_.bind_variable(a, 0);
    machine_.bind_variable(0xB8, 0);
    machine_.bind_variable(0xB9, 0);
    machine_.bind_variable(0x49C9, 12);
    machine_.bind_variable(0x49CA, 1);
    machine_.bind_variable(0x6E12, 3);
    machine_.bind_variable(0x6E3E, 1);
    // Kuto's Well's setup writes these two engine cells (65 or 8, and a table
    // value) when its plaza or catacombs load. Their meaning is unverified;
    // they are stored for the script and have no effect yet.
    machine_.bind_variable(0xC059, 0);
    machine_.bind_variable(0xC05F, 0);
    if (campaign_)
        selected_character_ = campaign_->state().selected;
    for (const auto &w : character_reply(selected_character_).writes)
        machine_.bind_variable(w.address, w.value);
    for (const std::uint8_t op :
            {
                10, 28, 32, 33, 36, 39, 40, 50, 55, 56, 57
            })
        machine_.enable_host(op);
    if (campaign_)
        for (const std::uint8_t op :
                {
                    11, 29, 30, 34, 35, 41, 46, 54
                })
            machine_.enable_host(op);
    synchronize_clock();
}

void RolfTourSession::synchronize_clock()
{
    for (const auto &w : clock_reply().writes)
        machine_.bind_variable(w.address, w.value);
}

EclHostReply RolfTourSession::clock_reply() const
{
    EclHostReply reply;
    if (!campaign_)
        return reply;
    const auto minutes = campaign_->state().time_minutes;
    const auto minute_of_day = (minutes % 1440 + 720) % 1440;
    const auto days = minutes / 1440 + (minutes % 1440 + 720) / 1440;
    reply.writes = {{0x49C7, static_cast<std::uint16_t>(minute_of_day % 10)},
        {0x49C8, static_cast<std::uint16_t>((minute_of_day % 60) / 10)},
        {0x49C9, static_cast<std::uint16_t>(minute_of_day / 60)},
        {0x49CA, static_cast<std::uint16_t>(days % 30 + 1)},
        {0x49CB, static_cast<std::uint16_t>((days / 30) % 12 + 1)},
        {0x49CC, static_cast<std::uint16_t>((days / 360) % 256)}
    };
    return reply;
}

EclHostReply RolfTourSession::character_reply(unsigned index) const
{
    if (campaign_)
        return party_character_reply(*campaign_, index);
    EclHostReply reply;
    std::array<std::uint16_t, 285> fields{};
    if (index == 0)
    {
        for (std::size_t n = 0; n < party_.name.size() && n < 15; ++n)
            fields[n] = party_.name[n];
        fields[0x18] = 14; // Verified GBVM Constitution field; no guessed raw-record offsets.
        fields[0x100] = 1;
        fields[0x119] = party_.hit_points;
        for (unsigned n = 0; n < ecl_coin_addresses.size(); ++n)
            fields[ecl_coin_addresses[n] - 0x6B00] = party_.wealth[n];
    }
    for (unsigned n = 0; n < fields.size(); ++n)
        reply.writes.push_back({static_cast<std::uint16_t>(0x6B00 + n), fields[n]});
    reply.writes.push_back({0x6DB1, static_cast<std::uint16_t>(index)});
    reply.writes.push_back({0x6DB4, static_cast<std::uint16_t>(index)});
    return reply;
}

void RolfTourSession::read_character()
{
    if (campaign_)
    {
        const auto exchange = campaign_->read_character(selected_character_, machine_);
        if (exchange)
        {
            const auto payer = campaign_->state().slots[selected_character_];
            const auto &name = campaign_->member(payer).character.sheet().name;
            snapshot_.payments.push_back({name, *exchange});
        }
        return;
    }
    if (selected_character_ != 0)
        return;
    for (unsigned n = 0; n < ecl_coin_addresses.size(); ++n)
        party_.wealth[n] = machine_.variable(ecl_coin_addresses[n]);
    party_.hit_points = machine_.variable(0x6C19);
}

void RolfTourSession::bind_pose(PartyPose pose)
{
    machine_.bind_variable(0xC04B, pose.x);
    machine_.bind_variable(0xC04C, pose.y);
    machine_.bind_variable(0xC04D, static_cast<std::uint16_t>(index(pose.facing)));
    const auto &cell = map_.at(pose.x, pose.y);
    machine_.bind_variable(0xC04E, cell.walls[index(pose.facing)]);
    machine_.bind_variable(0xC04F, cell.event_raw);
    publish_pose();
}

bool RolfTourSession::move_party(ExplorationCommand command)
{
    auto pose = snapshot_.pose;
    if (command == ExplorationCommand::turn_left)
        pose.facing = turned_left(pose.facing);
    else if (command == ExplorationCommand::turn_right)
        pose.facing = turned_right(pose.facing);
    else if (command == ExplorationCommand::turn_around)
        pose.facing = reversed(pose.facing);
    else if (command == ExplorationCommand::forward)
    {
        if (machine_.variable(0x6DC9) == 255)
            return false;
        const int x = static_cast<int>(pose.x) + step_x(pose.facing),
                  y = static_cast<int>(pose.y) + step_y(pose.facing);
        const auto wrapped_x = (x + 16) % 16, wrapped_y = (y + 16) % 16;
        const auto &a = map_.at(pose.x, pose.y);
        const auto &b = map_.at(wrapped_x, wrapped_y);
        const auto side = pose.facing, other = reversed(side);
        // Ordinary doors are traversable. Locks retain their distinct codes.
        const auto blocked = [](unsigned wall, unsigned door)
        {
            return door > 1 || (wall && !door);
        };
        if (blocked(a.walls[index(side)], a.doors[index(side)]) || blocked(b.walls[index(other)], b.doors[index(other)]))
            return false;
        machine_.bind_variable(0x49F0, pose.x);
        machine_.bind_variable(0x49F1, pose.y);
        pose.x = wrapped_x;
        pose.y = wrapped_y;
        pick_tried_ = false;
        ++snapshot_.footsteps;
    }
    if (campaign_ && command == ExplorationCommand::forward)
        campaign_->advance_time(std::chrono::seconds{6});
    bind_pose(pose);
    synchronize_clock();
    return true;
}

void RolfTourSession::begin_event(unsigned slot)
{
    claim_loot();
    synchronize_clock();
    snapshot_.payments.clear();
    snapshot_.door_checks.clear();
    snapshot_.door_opened = false;
    std::optional<opengold::PartyState> campaign;
    if (campaign_)
    {
        selected_character_ = campaign_->state().selected;
        for (const auto &w : character_reply(selected_character_).writes)
            machine_.bind_variable(w.address, w.value);
        campaign = campaign_->checkpoint();
    }
    event_checkpoint_ = EventCheckpoint{.machine = machine_,
                                        .party = party_,
                                        .campaign = std::move(campaign),
                                        .script = current_script_,
                                        .area = current_area_,
                                        .visited_areas = visited_areas_,
                                        .seen_areas = seen_areas_,
                                        .rest_checks = rest_checks_,
                                        .map = map_,
                                        .pending_loot = pending_loot_,
                                        .snapshot = snapshot_,
                                        .selected_character = selected_character_};
    event_stage_ = slot == 0 ? 1 : slot == 2 ? 4 : 2;
    machine_.bind_variable(0x6DC9, 0);
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
    member_chosen_ = false;
    bool off_map = false;
    if (slot == 0 && pending_movement_)
    {
        const auto p = snapshot_.pose;
        const int x = static_cast<int>(p.x) + step_x(p.facing),
                  y = static_cast<int>(p.y) + step_y(p.facing);
        const auto &cell = map_.at(p.x, p.y);
        off_map = (x < 0 || y < 0 || x >= 16 || y >= 16) && cell.doors[index(p.facing)] <= 1 &&
                  (!cell.walls[index(p.facing)] || cell.doors[index(p.facing)]);
    }
    machine_.bind_variable(0x6DD5, off_map ? 1 : 0);
    machine_.bind_variable(0x6DCA, slot == 1 ? 2 : 0);
    // The spokesman's reaction score (0x6DCF), which the scripts add to reaction
    // rolls. The original's formula is unknown; twice Charisma, as New Phlan's
    // own script doubles it, was chosen (LEADER-1).
    if (campaign_)
        if (const auto speaker = campaign_->spokesman())
            machine_.bind_variable(
                0x6DCF,
                static_cast<std::uint16_t>(2 * campaign_->member(speaker).character.sheet().scores[5]));
    if (!machine_.start(slot))
    {
        fail("Unable to start town script");
        return;
    }
    ++snapshot_.event_runs;
    snapshot_.phase = TourPhase::running;
    snapshot_.diagnostic.clear();
    snapshot_.choices.clear();
    ++snapshot_.revision;
}

void RolfTourSession::claim_loot()
{
    if (!campaign_)
        return;
    for (auto it = pending_loot_.begin(); it != pending_loot_.end();)
    {
        if (campaign_->award_loot(it->wealth, it->items, it->reward_id))
        {
            it = pending_loot_.erase(it);
            snapshot_.dialogue += "\nRecovered the encounter's original money and items.";
            ++snapshot_.revision;
        }
        else
            ++it;
    }
}

const std::vector<AnimationFrame> *RolfTourSession::monster_picture() const
{
    if (!showing_monster_picture_)
        return nullptr;
    return &area_resources().animations.at(*monster_picture_id_);
}

bool RolfTourSession::start_encounter()
{
    if (snapshot_.phase != TourPhase::combat || !showing_monster_picture_)
        return false;
    showing_monster_picture_ = false;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::reject_combat(std::string diagnostic)
{
    if (snapshot_.phase != TourPhase::combat || !encounter_ || !combat_request_ || !campaign_ ||
            campaign_->in_combat())
        return false;
    fail(std::move(diagnostic));
    return true;
}

bool RolfTourSession::resolve_combat(const rules::Snapshot &result)
{
    if (snapshot_.phase != TourPhase::combat || !encounter_ || !combat_request_ || !campaign_ ||
            campaign_->in_combat() || result.outcome == rules::Outcome::ongoing ||
            result.identity != campaign_->identity())
        return false;
    unsigned defeated = 0;
    // Monsters that ran off the field are worth neither experience nor treasure.
    std::vector<unsigned> escaped_records;
    std::set<rules::EntityId> enemies, party_ids;
    std::set<rules::EntityId> expected_party;
    for (auto id : campaign_->state().slots)
        if (id)
            expected_party.insert(id);
    for (const auto &unit : result.combatants)
    {
        if (unit.side == 1)
        {
            if (unit.id < 1000 || unit.id >= 1000 + staged_records_.size() ||
                    !enemies.insert(unit.id).second)
                return false;
            if (unit.hit_points == 0)
                ++defeated;
            else if (unit.fled)
                escaped_records.push_back(staged_records_.at(unit.id - 1000));
        }
        else if (unit.side != 0 || !expected_party.contains(unit.id) ||
                 !party_ids.insert(unit.id).second ||
                 campaign_->member(unit.id).vitals != unit.persistent)
            return false;
    }
    // Members who were already dead never joined the fight.
    for (auto id : expected_party)
        if (!party_ids.contains(id) && !campaign_->member(id).vitals.dead)
            return false;
    if (enemies.size() != staged_records_.size() ||
            (result.outcome == rules::Outcome::victory &&
             defeated + escaped_records.size() != enemies.size()))
        return false;
    if (result.outcome == rules::Outcome::defeat)
    {
        snapshot_.phase = TourPhase::defeated;
        ++snapshot_.revision;
        return true;
    }
    const bool first =
        current_area_ == 20 && encounter_records_ == std::vector<unsigned> {13, 4, 4, 4};
    const auto reward = first ? std::string("por:ECL2:20:search1:orcs:v1")
                        : "por:ECL" + std::to_string(area_resources().bank) + ":" +
                        std::to_string(current_script_) + ":roaming:" +
                        std::to_string(++next_ticket_);
    const bool fled = result.outcome == rules::Outcome::fled;
    // Members are lost and rewards claimed before the original script hears the
    // result; any failure from here rolls the whole event back rather than
    // leaving those half applied (Effective C++ Item 29).
    try
    {
        if (fled)
        {
            // The party fled (the original's rule): members left on the field are
            // lost for good, and only the monsters it killed are worth experience.
            // The treasure stays with the monsters.
            snapshot_.dialogue = "Your party flees the battle.";
            for (const auto &unit : result.combatants)
                if (unit.side == 0 && !unit.fled)
                {
                    campaign_->lose(unit.id);
                    snapshot_.dialogue += "\n" + unit.name + " is left behind and lost.";
                }
            unsigned experience = 0;
            for (const auto &unit : result.combatants)
                if (unit.side == 1 && unit.hit_points == 0)
                    experience += area_resources()
                                  .conversions.at(staged_records_.at(unit.id - 1000))
                                  .award_xp;
            if (experience)
                campaign_->award_experience(experience, reward);
        }
        else
        {
            // Experience and loot stay the original encounter's, however many fought,
            // less a record for each monster that got away.
            auto paying = encounter_records_;
            for (const auto record : escaped_records)
                if (const auto it = std::find(paying.begin(), paying.end(), record); it != paying.end())
                    paying.erase(it);
            // Monsters that all got away leave nothing to pay.
            if (!paying.empty())
            {
                unsigned experience = 0;
                for (auto record : paying)
                    experience += area_resources().conversions.at(record).award_xp;
                campaign_->award_experience(experience, reward);
                pending_loot_.push_back(encounter_loot(current_area_, paying, reward + ":loot",
                                                       machine_.variable(0x6DE3) != 1));
                claim_loot();
            }
            // Treasure the script added to the fight is won with it.
            if (staged_treasure_)
                award_staged_treasure();
        }
        auto reply = character_reply(selected_character_);
        // The original's combat results: 0 won, 128 the party fled.
        for (auto write : std::array<EclMemoryWrite, 7> {{{0x6DC7, static_cast<std::uint16_t>(fled ? 128 : 0)},
            {0x6DC8, static_cast<std::uint16_t>(defeated)},
                {0x6DCB, 0},
                {0x6DE3, 0},
                {0x6E70, 0},
                {0x6E71, 0},
                {0x6E72, 0}
            }
        })
        reply.writes.push_back(write);
        if (!machine_.resume_host(combat_request_, reply))
            throw EclError("Combat result rejected by original script");
    }
    catch (const std::exception &error)
    {
        fail(error.what());
        return true;
    }
    combat_request_ = 0;
    encounter_.reset();
    monster_picture_id_.reset();
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    // A later script fault must never restore pre-combat HP or erase a victory.
    event_checkpoint_.reset();
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    if (!pending_loot_.empty())
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
    advance(0);
    return true;
}

PendingLoot RolfTourSession::encounter_loot(unsigned area, std::vector<unsigned> records,
        std::string reward, bool items) const
{
    const auto district = town_->districts.find(area);
    if (district == town_->districts.end() || records.empty() || records.size() > 56 ||
            reward.empty() || reward.size() > 160 ||
            std::any_of(records.begin(), records.end(), [&](auto id)
{
    return !district->second->encounter_creatures.contains(id);
    }))
    throw EclError("Invalid pending encounter loot");
    PendingLoot loot;
    loot.reward_id = std::move(reward);
    loot.records = std::move(records);
    loot.include_items = items;
    loot.area = area;
    for (auto id : loot.records)
    {
        const auto &creature = district->second->encounter_creatures.at(id);
        for (unsigned coin = 0; coin < 7; ++coin)
            loot.wealth[coin] += creature.stored.wealth[coin];
        if (items)
            loot.items.insert(loot.items.end(), creature.equipment.begin(),
                              creature.equipment.end());
    }
    return loot;
}

void RolfTourSession::finish_event()
{
    read_character();
    // Another script may follow in this event; it must see the settled purse.
    for (const auto &w : character_reply(selected_character_).writes)
        machine_.bind_variable(w.address, w.value);
    publish_pose();
    if (transition_)
    {
        transition_ = false;
        event_stage_ = 3;
        if (!machine_.start(4))
            throw EclError("Cannot enter town building script");
        return;
    }
    if (event_stage_ == 1)
    {
        if (!move_party(*pending_movement_))
        {
            if (locked_door_ahead())
            {
                show_locked_door();
                return;
            }
            snapshot_.dialogue = "The way is blocked.";
        }
        search_destination();
        return;
    }
    if (event_stage_ == 3)
    {
        event_stage_ = 2;
        if (!machine_.start(1))
            throw EclError("Cannot search building entrance");
        return;
    }
    if (event_stage_ == 4)
    {
        if (!campaign_)
            throw EclError("Rest requires campaign rules");
        const auto interval = machine_.variable(0x6DD2), chance = machine_.variable(0x6DD3);
        if (chance == 255)
            snapshot_.dialogue += "\nRest is not allowed here.";
        else
        {
            // An interrupted rest grants nothing and leaves nothing to resume;
            // the party rests again.
            if (const auto rested = rest_interruption(interval, chance))
            {
                campaign_->advance_time(std::chrono::minutes{*rested});
                synchronize_clock();
                event_stage_ = 5;
                if (!machine_.start(3))
                    throw EclError("Cannot enter camp interruption script");
                return;
            }
            if (campaign_->rest(camp_kind_))
                snapshot_.dialogue +=
                    camp_kind_ == RestKind::short_rest
                    ? "\nShort rest complete: one hour passed; eligible members can spend Hit Dice."
                    : "\nLong rest complete: eight hours passed; eligible members recovered HP and supported resources.";
            else
                snapshot_.dialogue += "\nRest denied: no active member is eligible.";
            for (const auto &w : character_reply(selected_character_).writes)
                machine_.bind_variable(w.address, w.value);
            synchronize_clock();
        }
    }
    // A party the script moved away from a meeting (Flee, or Run after a
    // parley) got away, unless it was robbed and sent off.
    if (meeting_pose_ && encounter_outcome_.empty() && !robbed_ &&
            (meeting_pose_->x != snapshot_.pose.x || meeting_pose_->y != snapshot_.pose.y))
        encounter_outcome_ = "You get away.";
    if (!encounter_outcome_.empty())
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") + encounter_outcome_;
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
    if (event_stage_ == 5)
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") +
                              std::string("The rest was interrupted. Rest again to recover.");
    event_checkpoint_.reset();
    snapshot_.phase = TourPhase::completed;
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
}

// The original engine rests in five-minute steps. Each step counts toward the
// area's check interval (0x6DD2); on reaching it, the count restarts and d100 at
// or below the chance (0x6DD3) interrupts. The count carries over between rests.
// Returns the minutes rested before an interruption, or nothing when none occurs.
std::optional<unsigned> RolfTourSession::rest_interruption(unsigned interval, unsigned chance)
{
    const bool city_watch = interval == 1 && (chance == 100 || chance == 101);
    const bool slums_street = interval == 24 && chance == 24;
    const bool kutos_well_plaza = interval == 12 && chance == 12;
    if (interval && chance && !city_watch && !slums_street && !kutos_well_plaza)
        throw EclError("Unsupported probabilistic camp interruption");
    if (!interval)
        return std::nullopt;
    const auto &rules = campaign_->rule_module();
    const auto policy =
        camp_kind_ == RestKind::short_rest ? rules.short_rest_policy() : rules.long_rest_policy();
    constexpr unsigned step_minutes = 5;
    for (unsigned rested = step_minutes; rested <= policy.duration_minutes; rested += step_minutes)
    {
        if (++rest_checks_ < interval)
            continue;
        rest_checks_ = 0;
        if (machine_.engine_random(100) + 1 <= chance)
            return rested;
    }
    return std::nullopt;
}

// As in the original, the party's cell is searched even when the step failed.
void RolfTourSession::search_destination()
{
    pending_movement_.reset();
    event_stage_ = 2;
    if (!machine_.start(1))
        throw EclError("Cannot search destination");
}

bool RolfTourSession::locked_door_ahead() const
{
    if (!campaign_ || !pending_movement_ || machine_.variable(0x6DC9) == 255)
        return false;
    const auto edge = edge_ahead(snapshot_.pose);
    const auto &from = map_.at(edge.x, edge.y);
    const auto &to = map_.at(edge.next_x, edge.next_y);
    const auto passable_or_locked = [](unsigned wall, unsigned door)
    {
        return !wall || door == 1 || door == locked_door;
    };
    return (from.doors[index(edge.side)] == locked_door || to.doors[index(edge.other)] == locked_door) &&
           passable_or_locked(from.walls[index(edge.side)], from.doors[index(edge.side)]) &&
           passable_or_locked(to.walls[index(edge.other)], to.doors[index(edge.other)]);
}

void RolfTourSession::show_locked_door()
{
    door_menu_ = true;
    door_choices_ = {DoorMethod::bash};
    snapshot_.choices = {"Bash"};
    if (!pick_tried_ && campaign_->can_try_door(DoorMethod::pick))
    {
        door_choices_.push_back(DoorMethod::pick);
        snapshot_.choices.push_back("Pick");
    }
    if (campaign_->can_try_door(DoorMethod::knock))
    {
        door_choices_.push_back(DoorMethod::knock);
        snapshot_.choices.push_back("Knock");
    }
    snapshot_.choices.push_back("Exit");
    snapshot_.dialogue = "Locked.";
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

void RolfTourSession::try_locked_door(DoorMethod method)
{
    if (method == DoorMethod::pick)
        pick_tried_ = true;
    const auto attempts = campaign_->try_door(method, locked_door_difficulty);
    for (const auto &attempt : attempts)
        snapshot_.door_checks.push_back(
        {
            campaign_->member(attempt.member).character.sheet().name, method, attempt.roll.die,
            attempt.roll.total, locked_door_difficulty
        });
    if (attempts.empty() || attempts.back().roll.total < locked_door_difficulty)
        return;
    snapshot_.door_opened = true;
    // The original unlocks both faces in its loaded map; reloading the district relocks them.
    const auto edge = edge_ahead(snapshot_.pose);
    const auto unlock = [](std::uint8_t &door)
    {
        if (door == locked_door)
            door = 1;
    };
    unlock(map_.cells[edge.y * 16 + edge.x].doors[index(edge.side)]);
    unlock(map_.cells[edge.next_y * 16 + edge.next_x].doors[index(edge.other)]);
    if (!move_party(*pending_movement_))
        throw EclError("The opened door is still blocked");
}

std::string RolfTourSession::treasure_identity(std::uint16_t address) const
{
    // A TREASURE instruction is identified by its script and original address.
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    for (int shift = 12; shift >= 0; shift -= 4)
        hex += digits[(address >> shift) & 15];
    const std::string archive = "ECL" + std::to_string(current_area_ ? area_resources().bank : 3);
    return "por:" + archive + ":" + std::to_string(current_script_) + ":treasure:" + hex + ":v1";
}

void RolfTourSession::stage_treasure(const EclRequest &request)
{
    // Operands: copper, silver, electrum, gold, platinum, gems, jewelry, then an
    // item code: an ITEMn list below 128, 128 + n random items, or 255 for none.
    const auto items = request.arguments.at(7).value;
    if (!campaign_)
        throw EclError("Treasure awards require a campaign party");
    PendingLoot loot;
    for (unsigned coin = 0; coin < 7; ++coin)
        loot.wealth[coin] = request.arguments.at(coin).value;
    loot.reward_id = treasure_identity(request.instruction->address);
    loot.include_items = false;
    if (items < 128)
    {
        // A fixed list from the area's item archive.
        const auto &lists = area_resources().treasure;
        const auto found = lists.find(items);
        if (found == lists.end())
            throw EclError("Unknown treasure list " + std::to_string(items));
        for (auto item : found->second)
        {
            item.index = loot.items.size();
            loot.items.push_back(std::move(item));
        }
    }
    else if (items != 255)
    {
        // Rolled on the script's own saved random stream, as the original did here.
        const auto roll = [&](unsigned sides)
        {
            return machine_.host_random(request.id, sides) + 1;
        };
        for (auto &record : random_treasure_items(items - 128, roll))
        {
            Equipment item;
            item.index = loot.items.size();
            const auto &templates = town_->item_templates;
            if (!templates.empty())
            {
                if (record.type >= templates.size())
                    throw EclError("Missing item template for generated treasure");
                item.base = templates[record.type];
            }
            item.bonuses = equipment_bonuses(record, item.base);
            item.stored = std::move(record);
            loot.items.push_back(std::move(item));
        }
    }
    staged_treasure_ = std::move(loot);
}

void RolfTourSession::award_staged_treasure()
{
    auto loot = std::move(*staged_treasure_);
    staged_treasure_.reset();
    const auto &claimed = campaign_->state().claimed_rewards;
    const bool repeated =
        std::find(claimed.begin(), claimed.end(), loot.reward_id) != claimed.end() ||
        std::any_of(pending_loot_.begin(), pending_loot_.end(),
                    [&](const auto & pending)
    {
        return pending.reward_id == loot.reward_id;
    });
    if (repeated)
        throw EclError("This script treasure was already awarded; repeatable treasure is not "
                       "supported");
    pending_loot_.push_back(std::move(loot));
    claim_loot();
    if (!pending_loot_.empty())
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
}

void RolfTourSession::show_encounter_menu()
{
    const auto &args = encounter_menu_->arguments;
    for (unsigned frame = 0; frame < 3; ++frame)
    {
        auto image = decode_ega_sprite(area_resources().sprite_archive, args[2].value, frame);
        if (!image)
            throw EclError("Unsupported roaming encounter sprite");
        sprites_[frame] = std::move(image.image);
    }
    snapshot_.sprite_id = args[2].value;
    snapshot_.sprite_frame = encounter_distance_;
    snapshot_.dialogue = args[9 + encounter_distance_].text;
    if (snapshot_.dialogue.empty())
        snapshot_.dialogue = args[9].text;
    announce_camp_attack();
    if (!meeting_pose_)
        meeting_pose_ = snapshot_.pose;
    // As in the original, Parley replaces Advance once the monsters are adjacent.
    snapshot_.choices = {"Fight", "Wait", "Flee", encounter_distance_ ? "Advance" : "Parley"};
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}


// Shrinks the staged original encounter to the party's SRD XP budget at the
// encounter challenge and to one creature per living character, the size the
// party can face without being swarmed. Each LOAD MONSTER group keeps its first.
void RolfTourSession::fit_staged_encounter()
{
    std::vector<unsigned> levels;
    for (const auto id : campaign_->state().slots)
        if (id && !campaign_->member(id).vitals.dead)
            levels.push_back(campaign_->member(id).character.sheet().level);
    std::vector<EncounterGroup> groups;
    for (std::size_t n = 0; n < staged_records_.size(); ++n)
    {
        if (n == 0 || staged_records_[n] != staged_records_[n - 1])
            groups.push_back({area_resources().conversions.at(staged_records_[n]).fit_xp, 0});
        ++groups.back().count;
    }
    const auto budget = encounter_xp_budget(levels, encounter_challenge_);
    const auto counts =
        fit_encounter_to_budget(groups, budget, static_cast<unsigned>(levels.size()));
    std::vector<rules::Participant> enemies;
    std::vector<opengold::CombatArt> art;
    std::vector<unsigned> records;
    for (std::size_t n = 0, group = 0, kept = 0; n < staged_records_.size(); ++n)
    {
        if (n && staged_records_[n] != staged_records_[n - 1])
        {
            ++group;
            kept = 0;
        }
        if (kept++ >= counts[group])
            continue;
        const auto id = static_cast<rules::EntityId>(1000 + enemies.size());
        const auto &creature = area_resources().encounter_creatures.at(staged_records_[n]);
        enemies.push_back(staged_enemies_[n]);
        enemies.back().id = id;
        enemies.back().name = creature.stored.name + " " + std::to_string(enemies.size());
        art.push_back(staged_art_[n]);
        art.back().entity = id;
        records.push_back(staged_records_[n]);
    }
    staged_enemies_ = std::move(enemies);
    staged_art_ = std::move(art);
    staged_records_ = std::move(records);
}

// Monsters that interrupt a camp are announced ahead of the script's own text.
void RolfTourSession::announce_camp_attack()
{
    constexpr std::string_view attack = "Your camp is attacked!";
    if (event_stage_ == 5 && !snapshot_.dialogue.starts_with(attack))
        snapshot_.dialogue = std::string(attack) + "\n" + snapshot_.dialogue;
}

// Names who parleys: the spokesman whose reaction score the event started with.
void RolfTourSession::announce_spokesman()
{
    if (!campaign_)
        return;
    if (const auto speaker = campaign_->spokesman())
        snapshot_.dialogue +=
            "\n" + campaign_->member(speaker).character.sheet().name + " speaks for the party.";
}

bool RolfTourSession::choose_encounter(std::size_t choice)
{
    if (!encounter_menu_ || choice > 3)
        return false;
    // The fourth choice is Advance at a distance and Parley when adjacent.
    if (choice == 3 && encounter_distance_ == 0)
        choice = 4;
    const auto &request = *encounter_menu_;
    const auto &args = request.arguments;
    const auto response = args[4 + choice].value;
    if (response > 4)
        throw EclError("Invalid encounter response");
    int slowest = 1000, fastest = 0;
    for (auto id : campaign_->state().slots)
        if (id)
        {
            const auto &member = campaign_->member(id);
            // Conversion boundary: original movement 12 corresponds to a modern
            // ordinary 30-foot creature. Unconscious members cannot escape on foot.
            const int movement = member.vitals.dead || member.vitals.hit_points == 0
                                 ? 0
                                 : campaign_->profile(id).movement_feet * 2 / 5;
            slowest = std::min(slowest, movement);
            fastest = std::max(fastest, movement);
        }
    unsigned result = 1;
    if (choice == 2 && slowest >= args[12].value)
        result = 2;
    else if (response == 2 && (choice != 0 || args[13].value > fastest))
        result = 0;
    else if (response == 4)
        result = 3;
    else if (response == 1 && (choice == 1 || (choice == 3 && encounter_distance_ > 0)))
    {
        if (choice == 3)
            --encounter_distance_;
        show_encounter_menu();
        return true;
    }
    else if (response == 3 && encounter_distance_ > 0)
    {
        --encounter_distance_;
        show_encounter_menu();
        return true;
    }
    EclHostReply reply;
    reply.writes = {{args[3].value, static_cast<std::uint16_t>(result)}};
    if (!machine_.resume_host(request.id, reply))
        return false;
    // The script ends the meeting quietly (0); the event's end says so.
    encounter_outcome_ = result == 0 ? "The monsters go on their way." : "";
    encounter_menu_.reset();
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::notice(std::string message)
{
    message_only_ = true;
    snapshot_.dialogue = std::move(message);
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.choices = {"Continue"};
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

bool RolfTourSession::choose(std::uint64_t ticket, std::size_t choice)
{
    if (snapshot_.phase != TourPhase::awaiting_continue || !ticket ||
            ticket != snapshot_.continue_ticket || choice >= snapshot_.choices.size())
        return false;
    if (temple_request_)
    {
        // The original COMBAT request remains suspended until payment or cancellation.
        auto before = campaign_->checkpoint();
        try
        {
            const bool cancel = choice == temple_targets_.size();
            if (!cancel)
                campaign_->temple_heal(temple_targets_.at(choice));
            auto reply = character_reply(selected_character_);
            reply.writes.push_back({0x6DE2, 0});
            if (!machine_.resume_host(temple_request_, reply))
                throw EclError("Temple reply rejected");
            temple_request_ = 0;
            temple_targets_.clear();
            snapshot_.phase = TourPhase::running;
            snapshot_.dialogue =
                cancel ? "Temple service cancelled." : "Cure Wounds completed for 100 gp.";
            snapshot_.diagnostic.clear();
        }
        catch (const std::exception &e)
        {
            campaign_->restore(std::move(before));
            snapshot_.diagnostic = e.what();
            ++snapshot_.revision;
            return false;
        }
    }
    else if (damage_request_)
    {
        if (!machine_.resume_host(damage_request_, character_reply(selected_character_)))
            return false;
        damage_request_ = 0;
        snapshot_.phase = TourPhase::running;
    }
    else if (encounter_menu_)
    {
        return choose_encounter(choice);
    }
    else if (who_request_)
    {
        const auto slot = who_slots_.at(choice);
        auto reply = character_reply(slot);
        if (!machine_.resume_host(who_request_, reply))
            return false;
        campaign_->select(slot);
        selected_character_ = slot;
        member_chosen_ = true;
        who_request_ = 0;
        who_slots_.clear();
        snapshot_.phase = TourPhase::running;
    }
    else if (door_menu_)
    {
        door_menu_ = false;
        try
        {
            if (choice < door_choices_.size())
                try_locked_door(door_choices_[choice]);
            search_destination();
        }
        catch (const std::exception &e)
        {
            fail(e.what());
            return true;
        }
        snapshot_.phase = TourPhase::running;
    }
    else if (message_only_)
    {
        message_only_ = false;
        snapshot_.phase = TourPhase::completed;
    }
    else
    {
        if (!machine_.resume(menu_request_, choice))
            return false;
        snapshot_.phase = TourPhase::running;
    }
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::input(std::uint64_t ticket, std::string_view value)
{
    if (snapshot_.phase != TourPhase::awaiting_input || !ticket ||
            ticket != snapshot_.continue_ticket)
        return false;
    if (!machine_.resume_input(menu_request_, value))
    {
        snapshot_.diagnostic = snapshot_.number_input ? "Enter a whole number from 0 to 65535."
                               : "Enter up to 40 characters.";
        ++snapshot_.revision;
        return false;
    }
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::running;
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::buy(std::uint64_t ticket, std::size_t item)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket ||
            item >= treasure_.size())
        return false;
    const auto &offered = treasure_[item];
    if (campaign_)
    {
        try
        {
            campaign_->purchase(campaign_->selected(), offered);
            snapshot_.diagnostic = "Bought " + offered.label();
        }
        catch (const std::exception &e)
        {
            snapshot_.diagnostic = e.what();
            ++snapshot_.revision;
            return false;
        }
        ++snapshot_.revision;
        return true;
    }
    const auto price = offered.stored.value;
    if (party_.inventory.size() >= 16 || coins(party_.wealth, Coin::gold) < price)
    {
        snapshot_.diagnostic =
            party_.inventory.size() >= 16 ? "Inventory is full (16 items)." : "Not enough gold.";
        ++snapshot_.revision;
        return false;
    }
    auto purchased = offered;
    purchased.index = party_.inventory.size();
    purchased.stored.readied_raw = 0;
    party_.inventory.push_back(std::move(purchased)); // Allocate before charging.
    coins(party_.wealth, Coin::gold) -= price;
    snapshot_.diagnostic = "Bought " + offered.label() + " for " + std::to_string(price) + " gp.";
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::leave_shop(std::uint64_t ticket)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket)
        return false;
    const auto selected = campaign_ ? campaign_->state().selected : 0;
    auto reply = character_reply(selected);
    reply.writes.push_back({0x6E6C, 0});
    if (!machine_.resume_host(shop_request_, reply))
        return false;
    selected_character_ = selected;
    shop_request_ = 0;
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    return true;
}

std::string RolfTourSession::shoot_arrows(unsigned arrows, const rules::HazardAttack &arrow)
{
    std::string report;
    for (unsigned n = 0; n < arrows; ++n)
    {
        const auto hit = campaign_->hazard_attack(arrow);
        if (!hit)
            break;
        const auto &result = hit->result;
        const auto &member = campaign_->member(hit->target);
        const auto &name = member.character.sheet().name;
        const auto roll = " (" + std::to_string(result.total) + " vs AC " +
                          std::to_string(result.armor_class) + ")";
        if (!result.hit)
        {
            report += "\nAn arrow misses " + name + roll + ".";
            continue;
        }
        report += "\nAn arrow " + std::string(result.critical ? "critically hits " : "hits ") +
                  name + roll + " for " + std::to_string(result.damage) + " damage.";
        if (member.vitals.dead)
            report += " " + name + " dies.";
        else if (!result.death_saves.empty())
            report += " " + name + (member.vitals.hit_points ? " falls, then regains 1 HP."
                                    : " falls unconscious and is stable.");
    }
    return report;
}

bool RolfTourSession::handle_town_host(const EclRequest &request)
{
    const auto op = request.instruction->opcode;
    EclHostReply reply;
    const auto arg = [&](unsigned n)
    {
        return request.arguments.at(n).value;
    };
    switch (op)
    {
    case 10:
        if (arg(0) >= 128)
        {
            if ((arg(0) & 127) != selected_character_)
                throw EclError("Selected character store mismatch");
            read_character();
            reply = character_reply(selected_character_);
        }
        else
        {
            read_character();
            selected_character_ = arg(0);
            reply = character_reply(selected_character_);
            // LOAD CHARACTER is also used to scan slots. WHO, not that VM
            // cursor, changes the player's chosen party member.
        }
        break;
    case 57:
        read_character();
        if (campaign_)
        {
            who_slots_.clear();
            snapshot_.choices.clear();
            for (unsigned slot = 0; slot < 8; ++slot)
                if (auto id = campaign_->state().slots[slot])
                {
                    who_slots_.push_back(slot);
                    snapshot_.choices.push_back(campaign_->member(id).character.sheet().name);
                }
            if (who_slots_.empty())
                throw EclError("WHO requires a party member");
            who_request_ = request.id;
            snapshot_.phase = TourPhase::awaiting_continue;
            snapshot_.continue_ticket = ++next_ticket_;
            snapshot_.dialogue = "Choose a party member.";
            ++snapshot_.revision;
            return true;
        }
        selected_character_ = 0;
        reply = character_reply(0);
        break;
    case 12:
    {
        // Encounter distance images use the already verified SPRIT3 format.
        std::array<Image, 3> decoded;
        if (arg(1) > 2)
            throw EclError("Unsupported encounter distance");
        for (unsigned n = 0; n < 3; ++n)
        {
            auto image = decode_ega_sprite(area_resources().sprite_archive, arg(0), n);
            if (!image)
                throw EclError("Unsupported encounter sprite " + std::to_string(arg(0)));
            decoded[n] = std::move(image.image);
        }
        sprites_ = std::move(decoded);
        snapshot_.sprite_frame = arg(1);
        snapshot_.sprite_id = arg(0);
        monster_picture_id_ = arg(2);
        picture_.reset();
        ++snapshot_.picture_revision;
        break;
    }
    case 14:
    {
        snapshot_.sprite_frame = -1;
        picture_.reset();
        ++snapshot_.picture_revision;
        if (arg(0) == 255)
            break;
        const auto head_id = machine_.variable(0x6DE1);
        if (head_id == 255)
        {
            const auto found = area_resources().pictures.find(arg(0));
            if (found == area_resources().pictures.end())
                throw EclError("Unsupported town picture " + std::to_string(arg(0)));
            picture_ = found->second;
        }
        else
        {
            const auto head = area_resources().heads.find(head_id),
                       body = area_resources().bodies.find(arg(0));
            if (head == area_resources().heads.end() || body == area_resources().bodies.end() ||
                    head->second.width != 88 || body->second.width != 88 || head->second.height != 40 ||
                    body->second.height != 48)
                throw EclError("Unsupported town portrait composition");
            Image portrait;
            portrait.width = portrait.height = 88;
            portrait.rgba = head->second.rgba;
            portrait.rgba.insert(portrait.rgba.end(), body->second.rgba.begin(),
                                 body->second.rgba.end());
            picture_ = std::move(portrait);
        }
        break;
    }
    case 11:
    {
        const unsigned record = arg(0), count = arg(1);
        if (!current_area_ || !area_resources().conversions.contains(record) ||
                !count || staged_enemies_.size() + count > 56)
            throw EclError("Encounter needs an explicit supported creature conversion: record " +
                           std::to_string(record) + ", count " + std::to_string(count) + ", icon " +
                           std::to_string(arg(2)));
        const auto &creature = area_resources().encounter_creatures.at(record);
        auto icon = decode_ega_combat_icon(area_resources().combat_archive, arg(2), 0);
        if (!icon)
            throw EclError("Invalid original combat icon");
        // A leader shoots a bow only when its art shows one: of the Slums
        // combat icons, only the orc leader's icon 5 does.
        auto definition = area_resources().conversions.at(record).definition;
        if (definition == "slums-orc-leader" && arg(2) == 5)
            definition = "slums-orc-leader-archer";
        for (unsigned i = 0; i < count; ++i)
        {
            const auto id = static_cast<rules::EntityId>(1000 + staged_enemies_.size());
            staged_enemies_.push_back(
            {
                id,
                definition,
                creature.stored.name + " " + std::to_string(staged_enemies_.size() + 1),
                1,
                {}});
            // Morale is record byte 0x84: from 0x80 up a creature's own is
            // (byte & 0x7F) * 2, and above 102 it has none (the encounter's
            // decides). Most monsters have none; a few never break.
            const auto byte = creature.stored.raw[0x84];
            const unsigned own = byte >= 0x80 ? (byte & 0x7Fu) * 2 : 0;
            staged_enemies_.back().morale = own > 102 ? 0 : std::min(own, 100u);
            staged_enemies_.back().intelligence =
                std::min<unsigned>(creature.stored.abilities.intelligence, 30);
            staged_art_.push_back({id, icon.image});
            staged_records_.push_back(record);
        }
        break;
    }
    case 28:
        treasure_.clear();
        staged_treasure_.reset();
        staged_enemies_.clear();
        staged_art_.clear();
        staged_records_.clear();
        break;
    case 34:
    {
        // Supported converted party profiles have no original surprise modifiers.
        std::map<std::uint16_t, std::uint16_t> outputs;
        outputs[arg(0)] = 0;
        outputs[arg(1)] = 0;
        for (auto [address, value] : outputs)
            reply.writes.push_back({address, value});
        break;
    }
    case 35:
    {
        const int party_threshold =
            2 + static_cast<int>(arg(3)) - static_cast<int>(machine_.variable(arg(0)));
        const int monster_threshold =
            2 + static_cast<int>(machine_.variable(arg(1))) - static_cast<int>(arg(2));
        const bool party =
            static_cast<int>(machine_.host_random(request.id, 6)) + 1 <= party_threshold;
        const bool monsters =
            static_cast<int>(machine_.host_random(request.id, 6)) + 1 <= monster_threshold;
        reply.writes.push_back(
        {0x6DCB, static_cast<std::uint16_t>((party ? 1 : 0) | (monsters ? 2 : 0))});
        break;
    }
    case 41:
        if (!current_area_ || arg(1) > 2)
            throw EclError("Unsupported encounter menu context");
        encounter_menu_ = request;
        encounter_distance_ = arg(1);
        show_encounter_menu();
        return true;
    case 29:
        read_character();
        reply = character_reply(selected_character_);
        reply.writes.push_back({static_cast<std::uint16_t>(arg(0)),
                                static_cast<std::uint16_t>(party_strength(*campaign_))});
        break;
    case 30:
    {
        read_character();
        reply = character_reply(selected_character_);
        const auto values = check_party(*campaign_, arg(0), arg(1));
        std::map<std::uint16_t, std::uint16_t> outputs;
        for (unsigned n = 0; n < 4; ++n)
            outputs[static_cast<std::uint16_t>(arg(n + 2))] = values[n];
        for (const auto &[address, value] : outputs)
            reply.writes.push_back({address, value});
        break;
    }
    case 54:
    {
        read_character();
        reply = character_reply(selected_character_);
        const auto found = town_->npc_profiles.find(arg(0));
        if (found == town_->npc_profiles.end() || arg(0) == 24)
            throw EclError("NPC needs an explicit supported conversion/allegiance profile");
        campaign_->recruit("por:MON3CHA:" + std::to_string(arg(0)), found->second, arg(1));
        break;
    }
    case 39:
    {
        bool money = false;
        for (unsigned n = 0; n < 7; ++n)
            money = money || arg(n);
        // In an area, a fixed item list is script treasure like money.
        if (money || (current_area_ && arg(7) != 255))
        {
            stage_treasure(request);
            break;
        }
        if (arg(7) == 255)
            break;
        const auto found = town_->treasure.find(arg(7));
        if (found == town_->treasure.end())
            throw EclError("Unsupported town treasure list");
        treasure_.insert(treasure_.end(), found->second.begin(), found->second.end());
        break;
    }
    case 36:
        if (current_area_ && !staged_enemies_.empty())
        {
            read_character();
            encounter_records_ = staged_records_;
            fit_staged_encounter();
            const auto p = snapshot_.pose;
            encounter_ = opengold::CampaignEncounter
            {
                dungeon_battlefield(map_, p.x, p.y), staged_enemies_, staged_art_,
                area_resources().terrain_art,        p.facing,        machine_.variable(0x6DCB)};
            encounter_->party_resting = event_stage_ == 5;
            // The script sets the encounter's morale before COMBAT.
            encounter_->morale = std::min<unsigned>(machine_.variable(0x6DC6), 100);
            announce_camp_attack();
            combat_request_ = request.id;
            // The original shows the approached monster's close-up until a key press.
            showing_monster_picture_ = monster_picture_id_ &&
                                       area_resources().animations.contains(*monster_picture_id_);
            snapshot_.phase = TourPhase::combat;
            ++snapshot_.revision;
            return true;
        }
        if (machine_.variable(0x6DE2) == 1)
        {
            if (!campaign_)
                throw EclError("Temple service requires a campaign party");
            read_character();
            temple_targets_.clear();
            snapshot_.choices.clear();
            for (auto id : campaign_->state().slots)
                if (id)
                {
                    const auto &m = campaign_->member(id);
                    if (!m.vitals.dead && m.vitals.hit_points < campaign_->hit_point_maximum(id))
                    {
                        temple_targets_.push_back(id);
                        snapshot_.choices.push_back("Cure Wounds: " + m.character.sheet().name +
                                                    " (100 gp)");
                    }
                }
            snapshot_.choices.push_back("Cancel");
            temple_request_ = request.id;
            snapshot_.dialogue +=
                "\nCure Wounds costs 100 gp from active party purses and heals 2d8 + 3 HP. Choose a wounded living member. Other temple services are not supported.";
            snapshot_.diagnostic.clear();
            snapshot_.phase = TourPhase::awaiting_continue;
            snapshot_.continue_ticket = ++next_ticket_;
            ++snapshot_.revision;
            return true;
        }
        if (machine_.variable(0x6E6C) == 1)
        {
            // The leader deals with shopkeepers unless the event already asked who;
            // another buyer can still be chosen.
            if (campaign_ && !member_chosen_)
                for (unsigned slot = 0; slot < 8; ++slot)
                    if (campaign_->state().slots[slot] && campaign_->state().slots[slot] == campaign_->leader())
                        campaign_->select(slot);
            if (campaign_ && !campaign_->state().slots.at(campaign_->state().selected))
                throw EclError("Shopping requires a selected party member");
            read_character();
            shop_request_ = request.id;
            snapshot_.phase = TourPhase::shopping;
            snapshot_.continue_ticket = ++next_ticket_;
            snapshot_.choices.clear();
            snapshot_.diagnostic.clear();
            ++snapshot_.revision;
            return true;
        }
        if (staged_treasure_)
        {
            // With no monsters loaded, the original COMBAT only hands out the treasure.
            read_character();
            award_staged_treasure();
            reply = character_reply(selected_character_);
            break;
        }
        throw EclError(machine_.variable(0x6DE2) == 1 ? "Temple healing service"
                       : "This town combat encounter");
    case 32:
    {
        const auto found = town_->programs.find(arg(0));
        if (found == town_->programs.end())
            throw EclError("Travel outside New Phlan (script " + std::to_string(arg(0)) + ")");
        reply.next_program = found->second;
        // 0x6E12 names the new script's archive bank: ECL3 for the town.
        unsigned bank = 3;
        for (const auto &[id, district] : town_->districts)
            if (district->script == arg(0))
                bank = district->bank;
        reply.writes = {{0x49F2, static_cast<std::uint16_t>(current_script_)},
            {0x6E12, static_cast<std::uint16_t>(bank)}
        };
        current_script_ = arg(0);
        snapshot_.script_id = current_script_;
        transition_ = true;
        pending_movement_.reset();
        break;
    }
    case 33:
    {
        // Original gate transition refresh before NEW ECL selects its district.
        if (arg(0) == 255 && arg(1) == 255 && arg(2) == 127)
            break;
        // A district's script loads its own maps: the Slums map 20, Kuto's
        // Well its plaza (29) and catacombs (32).
        const auto district = town_->districts.find(arg(0));
        if (district != town_->districts.end() && district->second->script == current_script_ &&
                (arg(1) == 2 || arg(1) == 255) && arg(2) == 255)
            change_area(arg(0));
        else if (arg(0) == 0 && arg(1) == 0 && arg(2) == 0)
            change_area(0);
        else
            throw EclError("Resources outside the supported areas (map " + std::to_string(arg(0)) + ")");
        const auto &cell = map_.at(machine_.variable(0xC04B), machine_.variable(0xC04C));
        const auto facing = machine_.variable(0xC04D);
        if (facing >= 4)
            throw EclError("Invalid district entry facing");
        reply.writes = {{0xC04E, cell.walls[facing]}, {0xC04F, cell.event_raw}};
        break;
    }
    case 55:
        if (current_area_)
        {
            if (std::array<unsigned, 3> {arg(0), arg(1), arg(2)} != area_resources().pieces)
                throw EclError("Unverified district wall resource profile");
        }
        else if (arg(0) != 127 || arg(1) != 127 || arg(2) != 127)
            throw EclError("Unverified town wall resource profile");
        break;
    case 50:
    {
        const bool found = campaign_ ? party_has_item(*campaign_, arg(0))
                           : std::any_of(party_.inventory.begin(), party_.inventory.end(),
                                         [&](const auto & i)
        {
            return i.stored.type == arg(0);
        });
        reply.conditions = EclConditions{found, !found, false, false, false, false};
        break;
    }
    case 46:
    {
        // Arrow volleys: arg(0) arrows of arg(1)d arg(2) + arg(3). A high bit
        // marks the original's other forms (every member, a saving throw).
        if (!campaign_ || (arg(0) & 0x80) || (arg(4) & 0x80))
            throw EclError("Unsupported DAMAGE form");
        read_character();
        snapshot_.dialogue += shoot_arrows(arg(0), {hidden_archer_attack_bonus, arg(1), arg(2),
                                                    static_cast<int>(arg(3)), "piercing"});
        damage_request_ = request.id;
        snapshot_.phase = TourPhase::awaiting_continue;
        snapshot_.choices = {"Continue"};
        snapshot_.continue_ticket = ++next_ticket_;
        ++snapshot_.revision;
        return true;
    }
    case 40:
    {
        // Surrender (Kuto's Well): arg(0) 1 robs the whole party of arg(1)% of its
        // coins. The original also takes arg(2)% of the items; by the user's choice
        // only money is taken.
        if (!campaign_ || arg(0) != 1 || arg(1) > 100)
            throw EclError("Unsupported ROB form");
        read_character();
        for (const auto id : campaign_->state().slots)
            if (id)
            {
                auto wealth = campaign_->member(id).wealth;
                for (auto &coins : wealth)
                    coins = static_cast<std::uint16_t>(coins * (100 - arg(1)) / 100);
                campaign_->set_wealth(id, wealth);
            }
        robbed_ = true;
        reply = character_reply(selected_character_);
        break;
    }
    case 56:
        if (arg(0) != 9 || !campaign_)
            throw EclError("Training/camp service " + std::to_string(arg(0)));
        // The original inn invokes its pre-camp subroutine before PROGRAM 9.
        read_character();
        if (machine_.variable(0x6DD3) == 255 ||
                (machine_.variable(0x6DD2) && machine_.variable(0x6DD3)))
            throw EclError("Inn rest is not safe in this script context");
        if (!campaign_->rest())
            throw EclError("Party is not eligible for a long rest");
        reply = character_reply(selected_character_);
        for (const auto &w : clock_reply().writes)
            reply.writes.push_back(w);
        snapshot_.dialogue +=
            "\nLong rest complete: eight hours passed; eligible members recovered HP and supported resources.";
        break;
    default:
        return false;
    }
    if (!machine_.resume_host(request.id, reply))
        throw EclError("Town host reply rejected");
    if (op == 33)
        publish_pose();
    ++snapshot_.revision;
    return true;
}
} // namespace opengold::por
