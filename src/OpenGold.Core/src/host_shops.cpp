#include "opengold/rolf_tour.h"
#include "opengold/shop_service.h"
#include "opengold/authored_items.h"
#include "opengold/diagnostic_log.h"

#include <algorithm>

namespace opengold::por
{
namespace
{
// The original item a shop's record names, at the record's price. Its type
// must still match the list's, so a record made from other files fails here.
// An original item the shop sells with its record's enchantment added, and
// named by the record, such as "Leather Armor +1".
void enchant(Equipment &item, const opengold::ShopOffering &offering)
{
    auto &stored = item.stored;
    stored.magic_bonus += offering.magic_bonus;
    stored.raw[50] = static_cast<std::uint8_t>(static_cast<std::int8_t>(stored.magic_bonus));
    item.bonuses = equipment_bonuses(stored, item.base);
    item.decoded_name = offering.name;
    item.unidentified_name = offering.name;
}

Equipment original_offering(const PhlanResources &town, const opengold::ShopOffering &offering,
                            opengold::ShopStock stock)
{
    const std::map<unsigned, std::vector<Equipment>> *lists = nullptr;
    if (offering.archive == "ITEM3.DAX")
    {
        lists = &town.treasure;
    }
    for (const auto &[id, district] : town.districts)
    {
        if (offering.archive == "ITEM" + std::to_string(district->bank) + ".DAX")
        {
            lists = &district->treasure;
        }
    }
    if (lists)
    {
        const auto list = lists->find(offering.record);
        if (list != lists->end() && offering.index < list->second.size() &&
            list->second[offering.index].stored.type == offering.item_type)
        {
            auto item = list->second[offering.index];
            if (stock == opengold::ShopStock::modified)
            {
                item.stored.value = static_cast<std::uint16_t>(offering.price_gp);
            }
            if (offering.magic_bonus)
            {
                enchant(item, offering);
            }
            return item;
        }
    }
    throw EclError("A shop sells an item the game's lists lack: " + offering.name);
}

// Gear OpenGoldBox adds (authored_items.h), sold by the record's bundle.
Equipment authored_offering(const opengold::ShopOffering &offering)
{
    Equipment item;
    item.stored.type = static_cast<std::uint8_t>(offering.item_type);
    item.stored.stored_name = offering.name;
    item.stored.value = static_cast<std::uint16_t>(offering.price_gp);
    item.stored.stack_size = static_cast<std::uint8_t>(offering.bundle_quantity);
    return item;
}
} // namespace

void ShopService::shops(const PhlanResources *town, std::vector<opengold::ShopSite> sites,
                        opengold::ShopStock kept)
{
    const bool original = kept == opengold::ShopStock::original;
    if (original)
    {
        std::erase_if(sites,
                      [](const opengold::ShopSite &site)
                      {
                          return site.entrance.has_value();
                      });
    }
    std::vector<std::vector<Equipment>> stocks;
    for (const auto &site : sites)
    {
        auto &stock = stocks.emplace_back();
        for (const auto &offering : site.offerings)
        {
            // A record may still list arrows or quarrels; there is no ammunition.
            if (offering.item_type < 128 && opengold::original_item::is_ammunition(
                                                static_cast<std::uint8_t>(offering.item_type)))
            {
                continue;
            }
            // The original sold neither OpenGoldBox's gear nor enchanted items.
            if (original && (offering.archive.empty() || offering.magic_bonus != 0))
            {
                continue;
            }
            stock.push_back(offering.archive.empty() ? authored_offering(offering)
                                                     : original_offering(*town, offering, kept));
            stock.back().index = stock.size() - 1;
            // A shop says what it sells: the Rogue's Shop's magic weapons and
            // dust are known by their full names (SECRET-1).
            auto &item = stock.back();
            item.identify();
            if (!item.decoded_name.empty())
            {
                item.stored.stored_name = item.decoded_name;
            }
        }
    }
    shops_ = std::move(sites);
    shop_stocks_ = std::move(stocks);
}

void ShopService::enter_shop(TownServiceHost &host, std::uint64_t request)
{
    shop_request_ = request;
    host.screen().phase = TourPhase::shopping;
    host.screen().continue_ticket = host.new_ticket();
    host.screen().choices.clear();
    host.screen().diagnostic.clear();
    ++host.screen().revision;
}

bool ShopService::buy(TownServiceHost &host, std::uint64_t ticket, std::size_t item)
{
    if (host.screen().phase != TourPhase::shopping || !ticket ||
        ticket != host.screen().continue_ticket || item >= treasure_.size())
    {
        return false;
    }
    const auto &offered = treasure_[item];
    host.log_player("buys " + offered.label() + " (item " + std::to_string(item + 1) + " of " +
                    std::to_string(treasure_.size()) + ")");
    if (host.campaign())
    {
        host.screen().gear_changes.clear();
        const auto before = host.held_items();
        try
        {
            host.campaign()->purchase(host.campaign()->selected(), offered);
            host.screen().diagnostic = "Bought " + offered.label();
        }
        catch (const std::exception &e)
        {
            host.screen().diagnostic = e.what();
            ++host.screen().revision;
            return false;
        }
        host.equip_gained(before);
        ++host.screen().revision;
        return true;
    }
    const auto price = offered.stored.value;
    if (host.town_party().inventory.size() >= 16 || gold_value(host.town_party().wealth) < price)
    {
        host.screen().diagnostic = host.town_party().inventory.size() >= 16
                                       ? "Inventory is full (16 items)."
                                       : "Not enough gold.";
        ++host.screen().revision;
        return false;
    }
    auto purchased = offered;
    purchased.index = host.town_party().inventory.size();
    purchased.stored.readied_raw = 0;
    host.town_party().inventory.push_back(std::move(purchased)); // Allocate before charging.
    host.town_party().wealth = pay_gold(host.town_party().wealth, price).purse;
    host.screen().diagnostic =
        "Bought " + offered.label() + " for " + std::to_string(price) + " gp.";
    ++host.screen().revision;
    return true;
}

bool RolfTourSession::buy(std::uint64_t ticket, std::size_t item)
{
    return shop_service_.buy(*this, ticket, item);
}

bool RolfTourSession::leave_shop(std::uint64_t ticket)
{
    return shop_service_.leave_shop(*this, ticket);
}

TownServiceHost::HeldItems RolfTourSession::held_items() const
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

bool ShopService::leave_shop(TownServiceHost &host, std::uint64_t ticket)
{
    if (host.screen().phase != TourPhase::shopping || !ticket ||
        ticket != host.screen().continue_ticket)
    {
        return false;
    }
    host.log_player("leaves the shop");
    if (!shop_request_)
    {
        // OpenGoldBox's own shop: no script waits on it.
        host.step_out_of_shop();
        host.screen().continue_ticket = 0;
        host.screen().phase = TourPhase::completed;
        ++host.screen().revision;
        return true;
    }
    const auto selected = host.campaign() ? host.campaign()->state().selected_slot : PartySlot{};
    auto reply = host.character_reply(selected);
    reply.writes.push_back({host.game_cell(ecl_shop_service), 0});
    if (!host.script_machine().resume_host(shop_request_, reply))
    {
        return false;
    }
    host.select_character(selected);
    shop_request_ = 0;
    step_out_after_shop_ = true;
    host.screen().continue_ticket = 0;
    host.screen().phase = TourPhase::running;
    ++host.screen().revision;
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

void ShopService::open_own_shop(TownServiceHost &host)
{
    for (std::size_t n = 0; n < shops_.size(); ++n)
    {
        if (!shops_[n].entrance ||
            !host.standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
        {
            continue;
        }
        host.choose_shopper();
        treasure_ = shop_stocks_[n];
        enter_shop(host, 0);
        return;
    }
}

bool ShopService::stock_from_site(const TownServiceHost &host)
{
    bool stocked = false;
    for (std::size_t n = 0; n < shops_.size(); ++n)
    {
        if (host.standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
        {
            treasure_ = shop_stocks_[n];
            stocked = true;
        }
    }
    return stocked;
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
