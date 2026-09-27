#include "opengold/rolf_tour.h"
#include <algorithm>
#include <set>

namespace opengold::por
{
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
        for (const std::uint8_t op : {11, 29, 30, 34, 35, 41, 54})
            machine_.enable_host(op);
}

namespace
{
constexpr std::array<std::uint16_t, 7> money{0x6BBB, 0x6BBD, 0x6BBF, 0x6BC1,
                                             0x6BC3, 0x6BC5, 0x6BC7};
constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
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
    if (campaign_)
        selected_character_ = campaign_->state().selected;
    for (const auto &w : character_reply(selected_character_).writes)
        machine_.bind_variable(w.address, w.value);
    for (const std::uint8_t op : {10, 28, 32, 33, 36, 39, 40, 50, 55, 56, 57})
        machine_.enable_host(op);
    if (campaign_)
        for (const std::uint8_t op : {11, 29, 30, 34, 35, 41, 54})
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
                    {0x49CC, static_cast<std::uint16_t>((days / 360) % 256)}};
    return reply;
}

EclHostReply RolfTourSession::character_reply(unsigned index) const
{
    if (campaign_)
        return campaign_->character_reply(index);
    EclHostReply reply;
    std::array<std::uint16_t, 285> fields{};
    if (index == 0)
    {
        for (std::size_t n = 0; n < party_.name.size() && n < 15; ++n)
            fields[n] = party_.name[n];
        fields[0x18] = 14; // Verified GBVM Constitution field; no guessed raw-record offsets.
        fields[0x100] = 1;
        fields[0x119] = party_.hit_points;
        for (unsigned n = 0; n < money.size(); ++n)
            fields[money[n] - 0x6B00] = party_.wealth[n];
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
        campaign_->read_character(selected_character_, machine_);
        return;
    }
    if (selected_character_ != 0)
        return;
    for (unsigned n = 0; n < money.size(); ++n)
        party_.wealth[n] = machine_.variable(money[n]);
    party_.hit_points = machine_.variable(0x6C19);
}

void RolfTourSession::bind_pose(PartyPose pose)
{
    machine_.bind_variable(0xC04B, pose.x);
    machine_.bind_variable(0xC04C, pose.y);
    machine_.bind_variable(0xC04D, pose.facing);
    const auto &cell = map_.at(pose.x, pose.y);
    machine_.bind_variable(0xC04E, cell.walls[pose.facing]);
    machine_.bind_variable(0xC04F, cell.event_raw);
    publish_pose();
}

bool RolfTourSession::move_party(ExplorationCommand command)
{
    auto pose = snapshot_.pose;
    if (command == ExplorationCommand::turn_left)
        pose.facing = (pose.facing + 3) % 4;
    else if (command == ExplorationCommand::turn_right)
        pose.facing = (pose.facing + 1) % 4;
    else if (command == ExplorationCommand::turn_around)
        pose.facing = (pose.facing + 2) % 4;
    else if (command == ExplorationCommand::forward)
    {
        if (machine_.variable(0x6DC9) == 255)
            return false;
        const int x = static_cast<int>(pose.x) + dx[pose.facing],
                  y = static_cast<int>(pose.y) + dy[pose.facing];
        const auto wrapped_x = (x + 16) % 16, wrapped_y = (y + 16) % 16;
        const auto &a = map_.at(pose.x, pose.y);
        const auto &b = map_.at(wrapped_x, wrapped_y);
        const auto side = pose.facing, other = (side + 2) % 4;
        // Ordinary doors are traversable. Locks retain their distinct codes.
        const auto blocked = [](unsigned wall, unsigned door)
        {
            return door > 1 || (wall && !door);
        };
        if (blocked(a.walls[side], a.doors[side]) || blocked(b.walls[other], b.doors[other]))
            return false;
        machine_.bind_variable(0x49F0, pose.x);
        machine_.bind_variable(0x49F1, pose.y);
        pose.x = wrapped_x;
        pose.y = wrapped_y;
        ++snapshot_.footsteps;
    }
    if (campaign_ && command == ExplorationCommand::forward)
        campaign_->advance_time_milliseconds(6000);
    bind_pose(pose);
    synchronize_clock();
    return true;
}

void RolfTourSession::begin_event(unsigned slot)
{
    claim_loot();
    synchronize_clock();
    if (campaign_)
    {
        selected_character_ = campaign_->state().selected;
        for (const auto &w : character_reply(selected_character_).writes)
            machine_.bind_variable(w.address, w.value);
        saved_campaign_ = campaign_->checkpoint();
    }
    checkpoint_ = machine_;
    saved_party_ = party_;
    saved_script_ = current_script_;
    saved_area_ = current_area_;
    saved_visited_areas_ = visited_areas_;
    saved_seen_areas_ = seen_areas_;
    saved_snapshot_ = snapshot_;
    saved_selected_character_ = selected_character_;
    event_stage_ = slot == 0 ? 1 : slot == 2 ? 4 : 2;
    machine_.bind_variable(0x6DC9, 0);
    bool off_map = false;
    if (slot == 0 && pending_movement_)
    {
        const auto p = snapshot_.pose;
        const int x = static_cast<int>(p.x) + dx[p.facing],
                  y = static_cast<int>(p.y) + dy[p.facing];
        const auto &cell = map_.at(p.x, p.y);
        off_map = (x < 0 || y < 0 || x >= 16 || y >= 16) && cell.doors[p.facing] <= 1 &&
                  (!cell.walls[p.facing] || cell.doors[p.facing]);
    }
    machine_.bind_variable(0x6DD5, off_map ? 1 : 0);
    machine_.bind_variable(0x6DCA, slot == 1 ? 2 : 0);
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

void RolfTourSession::commit_rest_recovery()
{
    if (snapshot_.phase == TourPhase::combat && encounter_ && campaign_ &&
        !campaign_->in_combat() && saved_campaign_)
        saved_campaign_ = campaign_->checkpoint();
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
        }
        else if (unit.side != 0 || !expected_party.contains(unit.id) ||
                 !party_ids.insert(unit.id).second ||
                 campaign_->member(unit.id).vitals != unit.persistent)
            return false;
    }
    if (party_ids != expected_party || enemies.size() != staged_records_.size() ||
        (result.outcome == rules::Outcome::victory && defeated != enemies.size()))
        return false;
    if (result.outcome == rules::Outcome::defeat)
    {
        snapshot_.phase = TourPhase::defeated;
        ++snapshot_.revision;
        return true;
    }
    const bool first = staged_records_ == std::vector<unsigned>{13, 4, 4, 4};
    const auto reward = first ? std::string("por:ECL2:20:search1:orcs:v1")
                              : "por:ECL2:20:roaming:" + std::to_string(++next_ticket_);
    unsigned experience = 0;
    for (auto record : staged_records_)
        experience += record == 63                                 ? 200
                      : record == 0                                ? 25
                      : record == 1 || record == 2 || record == 11 ? 50
                      : record == 3 || record == 12                ? 100
                      : record == 4 || record == 13                ? 75
                                                                   : 150;
    campaign_->award_experience(experience, reward);
    pending_loot_.push_back(
        slums_loot(staged_records_, reward + ":loot", machine_.variable(0x6DE3) != 1));
    claim_loot();
    auto reply = character_reply(selected_character_);
    for (auto write : std::array<EclMemoryWrite, 7>{{{0x6DC7, 0},
                                                     {0x6DC8, static_cast<std::uint16_t>(defeated)},
                                                     {0x6DCB, 0},
                                                     {0x6DE3, 0},
                                                     {0x6E70, 0},
                                                     {0x6E71, 0},
                                                     {0x6E72, 0}}})
        reply.writes.push_back(write);
    if (!machine_.resume_host(combat_request_, reply))
        throw EclError("Combat result rejected by original script");
    combat_request_ = 0;
    encounter_.reset();
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    // A later script fault must never restore pre-combat HP or erase a victory.
    checkpoint_.reset();
    saved_campaign_.reset();
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    if (!pending_loot_.empty())
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
    advance(0);
    return true;
}

PendingLoot RolfTourSession::slums_loot(std::vector<unsigned> records, std::string reward,
                                        bool items) const
{
    if (records.empty() || records.size() > 56 || reward.empty() || reward.size() > 160)
        throw EclError("Invalid pending encounter loot");
    PendingLoot loot;
    loot.reward_id = std::move(reward);
    loot.records = std::move(records);
    loot.include_items = items;
    for (auto id : loot.records)
    {
        const auto &creature = town_->districts.at(20)->encounter_creatures.at(id);
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
            snapshot_.dialogue = "The way is blocked.";
        pending_movement_.reset();
        event_stage_ = 2;
        if (!machine_.start(1))
            throw EclError("Cannot search destination");
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
            const bool watch = interval && chance;
            // Only the verified guaranteed New Phlan profile is supported.
            if (watch && (interval != 1 || (chance != 100 && chance != 101)))
                throw EclError("Unsupported probabilistic camp interruption");
            if (resuming_camp_)
                campaign_->resume_rest(campaign_->state().rest_activity->ticket);
            else
                (void)campaign_->begin_rest(camp_kind_);
            bool completed = false;
            if (campaign_->state().rest_activity)
            {
                const auto duration = campaign_->remaining_rest_milliseconds();
                completed = campaign_
                                ->advance_rest(campaign_->state().rest_activity->ticket,
                                               watch ? std::min<std::uint64_t>(duration, 5 * 60000)
                                                     : duration,
                                               campaign_->state().rest_activity->work)
                                .has_value();
            }
            else if (watch)
                campaign_->advance_time(5);
            if (watch && !completed)
            {
                std::vector<MemberId> affected;
                for (auto id : campaign_->state().slots)
                    if (id)
                        affected.push_back(id);
                campaign_->loud_noise(affected);
                synchronize_clock();
                event_stage_ = 5;
                if (!machine_.start(3))
                    throw EclError("Cannot enter camp interruption script");
                return;
            }
            if (completed && resuming_camp_)
                snapshot_.dialogue +=
                    "\nLong rest complete: eligible members recovered HP and supported resources.";
            if (completed && !resuming_camp_)
                snapshot_.dialogue +=
                    camp_kind_ == RestKind::short_rest
                        ? "\nShort rest complete: one hour passed; eligible members can spend Hit Dice."
                        : "\nLong rest complete: eight hours passed; eligible members recovered HP and supported resources.";
            else if (!completed)
                snapshot_.dialogue += "\nRest denied: no active member is eligible.";
            for (const auto &w : character_reply(selected_character_).writes)
                machine_.bind_variable(w.address, w.value);
            synchronize_clock();
        }
    }
    if (event_stage_ == 5)
    {
        if (campaign_->state().rest_activity)
            campaign_->abandon_rest(campaign_->state().rest_activity->ticket);
        snapshot_.dialogue += "\nRest interrupted after five minutes; no recovery granted.";
    }
    checkpoint_.reset();
    snapshot_.phase = TourPhase::completed;
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
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
    snapshot_.choices = {"Fight", "Wait", "Flee", "Advance", "Parley"};
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

bool RolfTourSession::choose_encounter(std::size_t choice)
{
    if (!encounter_menu_ || choice > 4)
        return false;
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
        who_request_ = 0;
        who_slots_.clear();
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
    if (party_.inventory.size() >= 16 || party_.wealth[3] < price)
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
    party_.wealth[3] -= price;
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
        if (current_area_ != 20 || !area_resources().encounter_creatures.contains(record) ||
            !count || staged_enemies_.size() + count > 56)
            throw EclError("Encounter needs an explicit supported creature conversion: record " +
                           std::to_string(record) + ", count " + std::to_string(count) + ", icon " +
                           std::to_string(arg(2)));
        const auto &creature = area_resources().encounter_creatures.at(record);
        auto icon = decode_ega_combat_icon(area_resources().combat_archive, arg(2), 0);
        if (!icon)
            throw EclError("Invalid original Slums combat icon");
        const auto definition = record == 63                  ? "slums-bugbear"
                                : record == 0                 ? "slums-kobold"
                                : record == 1                 ? "slums-kobold-leader"
                                : record == 11                ? "slums-kobold-leader-sword"
                                : record == 2                 ? "slums-goblin"
                                : record == 3 || record == 12 ? "slums-goblin-leader"
                                : record == 4 || record == 13 ? "slums-orc"
                                                              : "slums-orc-leader";
        for (unsigned i = 0; i < count; ++i)
        {
            const auto id = static_cast<rules::EntityId>(1000 + staged_enemies_.size());
            staged_enemies_.push_back(
                {id,
                 definition,
                 creature.stored.name + " " + std::to_string(staged_enemies_.size() + 1),
                 1,
                 {}});
            staged_art_.push_back({id, icon.image});
            staged_records_.push_back(record);
        }
        break;
    }
    case 28:
        treasure_.clear();
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
        const int party_threshold = 2 + int(arg(3)) - int(machine_.variable(arg(0)));
        const int monster_threshold = 2 + int(machine_.variable(arg(1))) - int(arg(2));
        const bool party = int(machine_.host_random(request.id, 6)) + 1 <= party_threshold;
        const bool monsters = int(machine_.host_random(request.id, 6)) + 1 <= monster_threshold;
        reply.writes.push_back(
            {0x6DCB, static_cast<std::uint16_t>((party ? 1 : 0) | (monsters ? 2 : 0))});
        break;
    }
    case 41:
        if (current_area_ != 20 || arg(1) > 2)
            throw EclError("Unsupported encounter menu context");
        encounter_menu_ = request;
        encounter_distance_ = arg(1);
        show_encounter_menu();
        return true;
    case 29:
        read_character();
        reply.writes.push_back({static_cast<std::uint16_t>(arg(0)),
                                static_cast<std::uint16_t>(campaign_->strength())});
        break;
    case 30:
    {
        read_character();
        const auto values = campaign_->query(arg(0), arg(1));
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
        const auto found = town_->npc_profiles.find(arg(0));
        if (found == town_->npc_profiles.end() || arg(0) == 24)
            throw EclError("NPC needs an explicit supported conversion/allegiance profile");
        campaign_->recruit("por:MON3CHA:" + std::to_string(arg(0)), found->second, arg(1));
        break;
    }
    case 39:
    {
        for (unsigned n = 0; n < 7; ++n)
            if (arg(n))
                throw EclError("Non-shop treasure awards are not implemented");
        if (arg(7) == 255)
            break;
        const auto found = town_->treasure.find(arg(7));
        if (found == town_->treasure.end())
            throw EclError("Unsupported town treasure list");
        treasure_.insert(treasure_.end(), found->second.begin(), found->second.end());
        break;
    }
    case 36:
        if (current_area_ == 20 && !staged_enemies_.empty())
        {
            read_character();
            const auto p = snapshot_.pose;
            encounter_ = opengold::CampaignEncounter{
                dungeon_battlefield(map_, p.x, p.y), staged_enemies_, staged_art_,
                area_resources().terrain_art,        p.facing,        machine_.variable(0x6DCB)};
            (void)campaign_->prepare_combat();
            combat_request_ = request.id;
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
                    if (!m.vitals.dead && m.vitals.hit_points < m.character.sheet().hit_points)
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
        throw EclError(machine_.variable(0x6DE2) == 1 ? "Temple healing service"
                                                      : "This town combat encounter");
    case 32:
    {
        const auto found = town_->programs.find(arg(0));
        if (found == town_->programs.end())
            throw EclError("Travel outside New Phlan (script " + std::to_string(arg(0)) + ")");
        reply.next_program = found->second;
        reply.writes = {{0x49F2, static_cast<std::uint16_t>(current_script_)},
                        {0x6E12, static_cast<std::uint16_t>(arg(0) == 20 ? 2 : 3)}};
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
        if (arg(0) == 20 && arg(1) == 2 && arg(2) == 255 && current_script_ == 20)
            change_area(20);
        else if (arg(0) == 0 && arg(1) == 0 && arg(2) == 0)
            change_area(0);
        else
            throw EclError("Resources outside the town/Slums profiles");
        const auto &cell = map_.at(machine_.variable(0xC04B), machine_.variable(0xC04C));
        const auto facing = machine_.variable(0xC04D);
        if (facing >= 4)
            throw EclError("Invalid district entry facing");
        reply.writes = {{0xC04E, cell.walls[facing]}, {0xC04F, cell.event_raw}};
        break;
    }
    case 55:
        if (current_area_ == 20)
        {
            if (arg(0) != 2 || arg(1) != 4 || arg(2) != 1)
                throw EclError("Unverified Slums wall resource profile");
        }
        else if (arg(0) != 127 || arg(1) != 127 || arg(2) != 127)
            throw EclError("Unverified town wall resource profile");
        break;
    case 50:
    {
        const bool found = campaign_ ? campaign_->has_item(arg(0))
                                     : std::any_of(party_.inventory.begin(), party_.inventory.end(),
                                                   [&](const auto &i)
                                                   {
                                                       return i.stored.type == arg(0);
                                                   });
        reply.conditions = EclConditions{found, !found, false, false, false, false};
        break;
    }
    case 40:
        throw EclError("Pickpocket money and item removal");
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
