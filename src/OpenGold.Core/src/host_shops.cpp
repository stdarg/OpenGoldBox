#include "opengold/rolf_tour.h"
#include "opengold/diagnostic_log.h"

#include <algorithm>

namespace opengold::por
{
void RolfTourSession::enter_shop(std::uint64_t request)
{
    shop_request_ = request;
    snapshot_.phase = TourPhase::shopping;
    snapshot_.continue_ticket = ++next_ticket_;
    snapshot_.choices.clear();
    snapshot_.diagnostic.clear();
    ++snapshot_.revision;
}

bool RolfTourSession::buy(std::uint64_t ticket, std::size_t item)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket ||
        item >= treasure_.size())
    {
        return false;
    }
    const auto &offered = treasure_[item];
    log_player("buys " + offered.label() + " (item " + std::to_string(item + 1) + " of " +
               std::to_string(treasure_.size()) + ")");
    if (campaign_)
    {
        snapshot_.gear_changes.clear();
        const auto before = held_items();
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
        equip_gained(before);
        ++snapshot_.revision;
        return true;
    }
    const auto price = offered.stored.value;
    if (party_.inventory.size() >= 16 || gold_value(party_.wealth) < price)
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
    party_.wealth = pay_gold(party_.wealth, price).purse;
    snapshot_.diagnostic = "Bought " + offered.label() + " for " + std::to_string(price) + " gp.";
    ++snapshot_.revision;
    return true;
}

RolfTourSession::HeldItems RolfTourSession::held_items() const
{
    HeldItems held;
    for (const auto &member : campaign_->state().roster)
    {
        for (const auto &item : member.character.inventory().items())
        {
            held.emplace_back(member.id, item.id);
        }
    }
    return held;
}

void RolfTourSession::equip_gained(const HeldItems &before)
{
    if (!auto_equip_)
    {
        return;
    }
    for (const auto &[member, item] : held_items())
    {
        if (std::find(before.begin(), before.end(), std::pair{member, item}) != before.end())
        {
            continue;
        }
        if (auto change = campaign_->auto_equip(member, item, *auto_equip_))
        {
            if (diagnostic_log_enabled())
            {
                std::string line = change->name;
                for (const auto &name : change->equipped)
                {
                    line += " equips " + name + ";";
                }
                for (const auto &name : change->unequipped)
                {
                    line += " packs " + name + ";";
                }
                if (!change->burned_by.empty())
                {
                    line += " is burned by " + change->burned_by + " for " +
                            std::to_string(change->burn_damage);
                }
                log_diagnostic(LogPriority::info, "auto-equip", line);
            }
            snapshot_.gear_changes.push_back(std::move(*change));
        }
    }
}

bool RolfTourSession::leave_shop(std::uint64_t ticket)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket)
    {
        return false;
    }
    log_player("leaves the shop");
    if (!shop_request_)
    {
        // OpenGoldBox's own shop: no script waits on it.
        step_out_of_shop();
        snapshot_.continue_ticket = 0;
        snapshot_.phase = TourPhase::completed;
        ++snapshot_.revision;
        return true;
    }
    const auto selected = campaign_ ? campaign_->state().selected_slot : PartySlot{};
    auto reply = character_reply(selected);
    reply.writes.push_back({game_cell(ecl_shop_service), 0});
    if (!machine_.resume_host(shop_request_, reply))
    {
        return false;
    }
    selected_character_ = selected;
    shop_request_ = 0;
    step_out_after_shop_ = true;
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::step_out_of_shop()
{
    const auto shop = snapshot_.pose;
    // The square the party last stepped from (0x49F0, 0x49F1), when it is
    // beside the shop; otherwise the square behind the party, which came in
    // facing forward.
    const PartyPose from{machine_.variable(game_cell(EclAddress{0x49F0})),
                         machine_.variable(game_cell(EclAddress{0x49F1})), shop.facing};
    for (const auto side :
         {MapDirection::north, MapDirection::east, MapDirection::south, MapDirection::west})
    {
        const auto x = map_.wrapped_x(static_cast<int>(shop.x) + step_x(side));
        const auto y = map_.wrapped_y(static_cast<int>(shop.y) + step_y(side));
        if (static_cast<unsigned>(x) == from.x && static_cast<unsigned>(y) == from.y)
        {
            bind_pose({from.x, from.y, side});
            return;
        }
    }
    const auto behind = reversed(shop.facing);
    bind_pose({map_.wrapped_x(static_cast<int>(shop.x) + step_x(behind)),
               map_.wrapped_y(static_cast<int>(shop.y) + step_y(behind)), behind});
}

void RolfTourSession::open_own_shop()
{
    for (std::size_t n = 0; n < shops_.size(); ++n)
    {
        if (!shops_[n].entrance ||
            !standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
        {
            continue;
        }
        choose_shopper();
        treasure_ = shop_stocks_[n];
        enter_shop(0);
        return;
    }
}

void RolfTourSession::choose_shopper()
{
    if (!campaign_ || member_chosen_)
    {
        return;
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (campaign_->state().slots[slot] && campaign_->state().slots[slot] == campaign_->leader())
        {
            campaign_->select(PartySlot{slot});
        }
    }
}

} // namespace opengold::por
