// Sell and Drop (SELL-1 in docs/SRD-DECISIONS.md): a shop buys any item a
// member carries for half the town's price, bargained by Charisma, and any
// item can be dropped, gone for good; a cursed item, worn or packed, can be
// neither.
#include "opengold/campaign_party.h"
#include "opengold/shop_sale.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <array>
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

// Original item types (the original's item list), and name words: a type's
// own word, "+1" and "+2" (0xA2, 0xA3), "Silver" (177) and "Leather" (49).
constexpr std::uint8_t long_sword = 36, short_sword = 37, leather = 50, chain_mail = 55,
                       flask_of_oil = 86, ring = 69;
constexpr std::uint8_t plus_one = 0xA2, plus_two = 0xA3, silver = 177, leather_word = 49;

std::unique_ptr<RulesModule> module()
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    check(bool(in), "Read the rules");
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(content);
}

// Three kept dice that add up to `score`, the fourth discarded.
AbilityRoll roll_of(int score)
{
    const int first = std::min(6, score - 2);
    const int second = std::min(6, score - first - 1);
    return {{first, second, score - first - second, 1}, 3};
}

// A human soldier fighter with 15 in every ability but Charisma.
Character fighter(int charisma = 10, std::string name = "Fighter")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{5, 5, 5, 1}, 3};
    }
    d.rolls[static_cast<std::size_t>(Ability::charisma)] = roll_of(charisma);
    return Character(*srd5::character_rules(), d, {});
}

// A plain item of `type`, named by its own word, at `price`.
por::Equipment item(std::uint8_t type, std::uint16_t price)
{
    por::Equipment equipment;
    equipment.stored.type = type;
    equipment.stored.name_components = {0, 0, type};
    equipment.stored.value = price;
    equipment.stored.stack_size = 1;
    return equipment;
}

// The item with other name words and an enchantment.
por::Equipment named(por::Equipment equipment, std::array<std::uint8_t, 3> words, int bonus = 0)
{
    equipment.stored.name_components = words;
    equipment.stored.magic_bonus = static_cast<std::int8_t>(bonus);
    return equipment;
}

// New Phlan's shops as the tests need them: an armory, a general store, a
// silversmith and a shop selling enchanted gear.
struct Town
{
    std::vector<std::vector<por::Equipment>> stocks{
        {item(short_sword, 8), item(long_sword, 15),
         named(item(leather, 5), {0, leather_word, leather})},
        {item(flask_of_oil, 40)},
        {named(item(short_sword, 80), {0, short_sword, silver})},
        {named(item(leather, 4000), {0, leather_word, leather}, 1)},
    };

    [[nodiscard]] SaleMarket at(std::size_t shop) const
    {
        return {.shop = stocks[shop], .town = stocks};
    }
};

constexpr std::size_t armory = 0, general_store = 1, silversmith = 2, magic_shop = 3;

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

// A worn Long Sword sells for half the shop's price and comes off first.
void sell()
{
    const Town town;
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_wealth({0, 0, 0, 100, 0, 0, 0});
    const auto sword = buy(party, id, item(long_sword, 15));
    (void)party.equip(id, sword);
    check(gold(party) == 85 && party.sale_offer(id, sword, town.at(armory)) == 7,
          "A shop offers half a Long Sword's price, rounded down");
    check(party.sell(id, sword, town.at(armory)) == 7 && gold(party) == 92 &&
              !carries(party, id, sword) && party.member(id).equipped.empty(),
          "Selling a worn sword takes it off, and the party gets the offer");
}

// The offer for one item a member with `charisma` carries, at `shop`.
unsigned offer(const por::Equipment &carried, int charisma, std::size_t shop)
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter(charisma));
    party.take_item(id, carried);
    const auto taken = party.member(id).character.inventory().items().back().id;
    return party.sale_offer(id, taken, Town{}.at(shop));
}

// A monster's Short Sword, whose record is worth 3 gp, fetches half what the
// armory asks for one, 10% more or less of that per point of Charisma
// modifier (the user, 2026-11-07).
void shop_price_and_charisma()
{
    const auto monster_sword = item(short_sword, 3);
    check(offer(monster_sword, 10, armory) == 4, "A monster's Short Sword fetches half of 8 gp");
    check(offer(monster_sword, 8, armory) == 3, "Charisma 8 (-1) takes 10% off: 3.6 gp");
    check(offer(monster_sword, 16, armory) == 5, "Charisma 16 (+3) adds 30%: 5.2 gp");
    check(offer(monster_sword, 3, armory) == 2, "Charisma 3 (-4) pays 60%: 2.4 gp");
    check(offer(monster_sword, 18, armory) == 5, "Charisma 18 (+4) pays 140%: 5.6 gp");
}

// A shop that sells nothing of an item's base type pays 25% less.
void other_shops()
{
    check(offer(item(short_sword, 3), 10, general_store) == 3,
          "The general store pays 25% less for a Short Sword: 3 gp");
    check(offer(item(flask_of_oil, 40), 10, general_store) == 20 &&
              offer(item(flask_of_oil, 40), 10, armory) == 15,
          "The general store pays the table for its own oil, the armory 25% less");
}

// A Silver Short Sword is priced as the silversmith's, not as a plain one.
void silver_items()
{
    const auto silver_sword = named(item(short_sword, 80), {0, short_sword, silver});
    check(offer(silver_sword, 10, silversmith) == 40, "The silversmith pays half of 80 gp");
    check(offer(silver_sword, 10, armory) == 40,
          "The armory sells Short Swords, so it pays the table for a silver one too");
    check(offer(item(short_sword, 3), 10, silversmith) == 4,
          "A plain Short Sword is not priced as a silver one");
}

// Magic gear no shop sells goes by its own value; gear a shop sells
// enchanted goes by that shop's price, whatever its name words.
void magic()
{
    const auto long_sword_plus_two = named(item(long_sword, 4000), {0, plus_two, long_sword}, 2);
    check(offer(long_sword_plus_two, 10, armory) == 2000,
          "The armory pays half a Long Sword +2's own 4,000 gp");
    check(offer(long_sword_plus_two, 10, general_store) == 1500,
          "The general store pays 25% less for it");
    const auto found_leather = named(item(leather, 2000), {plus_one, leather_word, leather}, 1);
    check(offer(found_leather, 10, magic_shop) == 2000,
          "A found Leather Armor +1 fetches half the magic shop's 4,000 gp");
}

// An item not yet identified could be cursed: half what the shop would pay
// for its unenchanted version (the user, 2026-11-07).
void unidentified()
{
    auto hidden_sword = named(item(long_sword, 2000), {0, plus_one, long_sword}, 1);
    hidden_sword.stored.hidden_name_words = 6;
    check(!hidden_sword.identified() && offer(hidden_sword, 10, armory) == 3,
          "An unidentified Long Sword +1 fetches half of a Long Sword's 7 gp");
    auto hidden_ring = named(item(ring, 5000), {205, 167, 66}, 3);
    hidden_ring.stored.hidden_name_words = 6;
    check(offer(hidden_ring, 10, armory) == 0,
          "An unidentified ring, which no shop sells unenchanted, fetches nothing");
}

// The higher Charisma of the seller and the leader bargains.
void leader_bargains()
{
    const Town town;
    CampaignParty party(module());
    const auto seller = party.add_pc(fighter(8, "Seller"));
    const auto leader = party.add_pc(fighter(16, "Leader"));
    party.make_leader(leader);
    party.take_item(seller, item(short_sword, 3));
    const auto sword = party.member(seller).character.inventory().items().back().id;
    check(party.sale_offer(seller, sword, town.at(armory)) == 5,
          "A leader with Charisma 16 bargains for a seller with 8");
    party.make_leader(seller);
    check(party.sale_offer(seller, sword, town.at(armory)) == 3,
          "Leading herself, the seller gets her own price");
}

// The share of a stack still carried, and the table's ends at Charisma 30
// (+10) and 3 (-4), at a shop that sells the item.
void shares()
{
    const Town town;
    auto oil = item(flask_of_oil, 40);
    oil.stored.stack_size = 4;
    const std::vector<std::vector<por::Equipment>> oil_seller{{oil}};
    check(shop_offer(item(flask_of_oil, 1), 2, 0, {.shop = oil_seller[0], .town = oil_seller}) ==
              10,
          "Two flasks fetch half their share of the shop's 40 gp for four");
    check(shop_offer(item(short_sword, 3), 1, 10, town.at(armory)) == 8,
          "Charisma 30 sells a Short Sword for what the shop asks");
    check(shop_offer(item(short_sword, 3), 1, -4, town.at(armory)) == 2,
          "Charisma 3 sells a Short Sword for 2 gp");
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
            (void)party.sell(id, sword, Town{}.at(armory));
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
        shop_price_and_charisma();
        other_shops();
        silver_items();
        magic();
        unidentified();
        leader_bargains();
        shares();
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
