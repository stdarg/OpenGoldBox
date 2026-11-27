#include "opengold/town_service_host.h"
#include "opengold/vault_service.h"

#include <limits>
#include <stdexcept>

namespace opengold::por
{
namespace
{
constexpr std::array<Coin, 3> vault_coins{Coin::platinum, Coin::gems, Coin::jewelry};
constexpr std::size_t vault_capacity = 50;

unsigned combined(unsigned first, unsigned second)
{
    if (second > std::numeric_limits<unsigned>::max() - first)
    {
        throw std::runtime_error("There is no room for that money.");
    }
    return first + second;
}
} // namespace

void VaultService::open_dark_queen_vault(TownServiceHost &host, std::uint64_t request)
{
    dark_queen_vault_visit_ = VaultVisit{request};
    show_dark_queen_vault(host, VaultMenu::home);
}

void VaultService::show_dark_queen_vault(TownServiceHost &host, VaultMenu menu,
                                         std::vector<rules::Message> notice)
{
    auto &visit = *dark_queen_vault_visit_;
    visit.choices.clear();
    std::vector<rules::Message> choices;
    const auto add = [&](rules::Message label, VaultChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(choice);
    };
    if (menu == VaultMenu::home)
    {
        notice.push_back(
            {"The vault holds {steel} steel, {gems} gems, {jewelry} jewelry and {items} items.",
             {{"steel", std::to_string(dark_queen_vault_money_[0])},
              {"gems", std::to_string(dark_queen_vault_money_[1])},
              {"jewelry", std::to_string(dark_queen_vault_money_[2])},
              {"items", std::to_string(dark_queen_vault_items_.size())}}});
        add({"Money", {}}, {VaultAction::money});
        add({"Items", {}}, {VaultAction::deposit});
        add({"Take", {}}, {VaultAction::withdraw});
        add({"Exit", {}}, {VaultAction::leave});
    }
    else if (menu == VaultMenu::deposit)
    {
        notice.push_back({"Choose an item to leave in the vault.", {}});
        for (const auto id : host.campaign()->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto &member = host.campaign()->member(id);
            for (const auto &item : member.character.inventory().items())
            {
                if (member.item_sources.contains(item.id))
                {
                    add({"{name}: {item}",
                         {{"name", member.character.sheet().name}, {"item", item.name}}},
                        {VaultAction::deposit_item, id, item.id});
                }
            }
        }
        add({"Back", {}}, {VaultAction::back});
    }
    else if (menu == VaultMenu::withdraw)
    {
        notice.push_back({"Choose what to take from the vault.", {}});
        add({"Money", {}}, {VaultAction::withdraw_money});
        for (std::size_t item = 0; item < dark_queen_vault_items_.size(); ++item)
        {
            add({"{item}", {{"item", dark_queen_vault_items_[item].label()}}},
                {VaultAction::withdraw_item, 0, item});
        }
        add({"Back", {}}, {VaultAction::back});
    }
    else
    {
        notice.push_back(
            {"Who will take {item}?", {{"item", dark_queen_vault_items_.at(visit.item).label()}}});
        for (const auto id : host.campaign()->state().slots)
        {
            if (id)
            {
                add({"{name}", {{"name", host.campaign()->member(id).character.sheet().name}}},
                    {VaultAction::recipient, id});
            }
        }
        add({"Back", {}}, {VaultAction::back});
    }
    host.show_host_menu(std::move(notice), std::move(choices));
}

bool VaultService::choose_dark_queen_vault(TownServiceHost &host, std::size_t choice)
{
    auto &visit = *dark_queen_vault_visit_;
    const auto picked = visit.choices.at(choice);
    if (picked.action == VaultAction::leave)
    {
        auto reply = host.character_reply(host.selected_character());
        reply.writes.push_back({EclAddress{0x31}, 0});
        if (!host.script_machine().resume_host(visit.request, reply))
        {
            throw EclError("Vault reply rejected");
        }
        dark_queen_vault_visit_.reset();
        host.screen().host_dialogue.clear();
        host.screen().host_choices.clear();
        host.screen().choices.clear();
        host.screen().dialogue.clear();
        host.screen().phase = TourPhase::running;
        ++host.screen().revision;
        return true;
    }
    try
    {
        switch (picked.action)
        {
        case VaultAction::money:
        {
            auto wealth = host.campaign()->state().wealth;
            auto deposited = dark_queen_vault_money_;
            for (std::size_t coin = 0; coin < vault_coins.size(); ++coin)
            {
                auto &carried = wealth[static_cast<std::size_t>(vault_coins[coin])];
                deposited[coin] = combined(deposited[coin], carried);
                carried = 0;
            }
            host.campaign()->set_wealth(wealth);
            dark_queen_vault_money_ = deposited;
            break;
        }
        case VaultAction::deposit:
            show_dark_queen_vault(host, VaultMenu::deposit);
            return true;
        case VaultAction::withdraw:
            show_dark_queen_vault(host, VaultMenu::withdraw);
            return true;
        case VaultAction::deposit_item:
        {
            if (dark_queen_vault_items_.size() >= vault_capacity)
            {
                throw std::runtime_error("Vault is full!");
            }
            auto deposited = dark_queen_vault_items_;
            auto item = host.campaign()->member(picked.member).item_sources.at(picked.item);
            item.stored.readied_raw = 0;
            deposited.push_back(std::move(item));
            host.campaign()->drop(picked.member, picked.item);
            dark_queen_vault_items_ = std::move(deposited);
            break;
        }
        case VaultAction::withdraw_money:
        {
            auto wealth = host.campaign()->state().wealth;
            for (std::size_t coin = 0; coin < vault_coins.size(); ++coin)
            {
                auto &carried = wealth[static_cast<std::size_t>(vault_coins[coin])];
                carried = combined(carried, dark_queen_vault_money_[coin]);
            }
            host.campaign()->set_wealth(wealth);
            dark_queen_vault_money_ = {};
            break;
        }
        case VaultAction::withdraw_item:
            visit.item = static_cast<std::size_t>(picked.item);
            show_dark_queen_vault(host, VaultMenu::recipient);
            return true;
        case VaultAction::recipient:
            host.campaign()->take_item(picked.member, dark_queen_vault_items_.at(visit.item));
            dark_queen_vault_items_.erase(dark_queen_vault_items_.begin() +
                                          static_cast<std::ptrdiff_t>(visit.item));
            break;
        case VaultAction::back:
            break;
        case VaultAction::leave:
            break;
        }
    }
    catch (const std::runtime_error &error)
    {
        show_dark_queen_vault(host, VaultMenu::home, {{"{reason}", {{"reason", error.what()}}}});
        return true;
    }
    show_dark_queen_vault(host, VaultMenu::home);
    return true;
}
} // namespace opengold::por
