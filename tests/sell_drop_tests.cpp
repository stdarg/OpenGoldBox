// Sell and Drop (SELL-1 in docs/SRD-DECISIONS.md): a shop buys any item a
// member carries for half its price, and any item can be dropped, gone for
// good; a cursed item, worn or packed, can be neither.
#include "opengold/campaign_party.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace opengold;
using namespace opengold::rules;

namespace
{
using opengold::test::check;

// Original item types (the original's item list).
constexpr std::uint8_t long_sword = 36, chain_mail = 55;

std::unique_ptr<RulesModule> module()
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    check(bool(in), "Read the rules");
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(content);
}

Character fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Fighter";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{5, 5, 5, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

por::Equipment item(std::uint8_t type, std::uint16_t price)
{
    por::Equipment equipment;
    equipment.stored.type = type;
    equipment.stored.value = price;
    equipment.stored.stack_size = 1;
    return equipment;
}

// The member buys the item; its inventory id is returned.
std::uint64_t buy(CampaignParty &party, MemberId id, const por::Equipment &bought)
{
    party.purchase(id, bought);
    return party.member(id).character.inventory().items().back().id;
}

bool carries(const CampaignParty &party, MemberId id, std::uint64_t item)
{
    return party.member(id).character.inventory().find(item).has_value();
}

std::uint32_t gold(const CampaignParty &party)
{
    return coins(party.state().wealth, Coin::gold);
}

// A worn Long Sword sells for half its price and comes off first.
void sell()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 100, 0, 0, 0});
    const auto sword = buy(party, id, item(long_sword, 15));
    (void)party.equip(id, sword);
    check(gold(party) == 85 && party.sale_offer(id, sword) == 7,
          "A shop offers half a Long Sword's price, rounded down");
    check(party.sell(id, sword) == 7 && gold(party) == 92 && !carries(party, id, sword) &&
              party.member(id).equipped.empty(),
          "Selling a worn sword takes it off, and the party gets the offer");
}

// A dropped item is gone, and nothing is paid for it.
void drop()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 100, 0, 0, 0});
    const auto armor = buy(party, id, item(chain_mail, 75));
    party.drop(id, armor);
    check(!carries(party, id, armor) && gold(party) == 25, "A dropped item is gone, unpaid");
}

// A cursed sword can be neither sold nor dropped, in the pack or in hand.
void cursed()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 100, 0, 0, 0});
    auto cursed_sword = item(long_sword, 10);
    cursed_sword.stored.magic_bonus = -2;
    cursed_sword.stored.cursed_raw = 1;
    const auto sword = buy(party, id, cursed_sword);
    const auto refused = [&]
    {
        bool sold = true, dropped = true;
        try
        {
            (void)party.sell(id, sword);
        }
        catch (const std::exception &)
        {
            sold = false;
        }
        try
        {
            party.drop(id, sword);
        }
        catch (const std::exception &)
        {
            dropped = false;
        }
        return !sold && !dropped && carries(party, id, sword) && gold(party) == 90;
    };
    check(refused(), "A packed cursed sword can be neither sold nor dropped");
    (void)party.equip(id, sword);
    check(refused() && party.member(id).equipped.size() == 1,
          "A wielded cursed sword can be neither sold nor dropped");
}
} // namespace

int main()
{
    try
    {
        sell();
        drop();
        cursed();
        std::cout << "Sell and drop tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
