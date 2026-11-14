// The event kinds of an Unlimited Adventures design that offer a town's
// services, test or bill a member, guide the party or bring an NPC along
// (docs/games/ua.md, Events). Their menus are the kit's own words, which the
// snapshot carries as host messages for the view to translate; the
// designer's texts are shown as written.
#include "opengold/ua_session.h"

#include "opengold/coin_purse.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

using por::TourPhase;
using rules::Message;

[[nodiscard]] unsigned word_at(const EventRecord &event, std::size_t at)
{
    return event[at] | event[at + 1] << 8;
}

[[nodiscard]] std::uint32_t long_at(const EventRecord &event, std::size_t at)
{
    return static_cast<std::uint32_t>(word_at(event, at)) | word_at(event, at + 2) << 16;
}

[[nodiscard]] std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        const auto mark = "{" + argument.name + "}";
        for (auto at = text.find(mark); at != std::string::npos; at = text.find(mark))
        {
            text.replace(at, mark.size(), argument.value);
        }
    }
    return text;
}

[[nodiscard]] std::string upper(std::string_view text)
{
    std::string result;
    for (const char c : text)
    {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    const auto first = result.find_first_not_of(' ');
    const auto last = result.find_last_not_of(' ');
    return first == std::string::npos ? std::string{} : result.substr(first, last - first + 1);
}

// The menu's options of a menu text, as menu_event reads them.
[[nodiscard]] std::vector<std::string> menu_words(const std::string &menu)
{
    std::vector<std::string> options;
    for (const char c : menu)
    {
        if (c == '~' || c == '^')
        {
            options.emplace_back();
        }
        else if (!options.empty())
        {
            options.back() += c;
        }
    }
    for (auto &option : options)
    {
        option = upper(option);
    }
    return options;
}

// ITEM.DAT numbers a shop's stock masks pick from: a mask's high four bits
// choose the row, its twenty low bits the items (shop.c, item_classes).
[[nodiscard]] unsigned stock_item(unsigned row, unsigned bit)
{
    struct Row
    {
        unsigned first, count;
    };

    // Most rows are a run of numbers; three join two runs, one ends out of order.
    static constexpr std::array<Row, 16> runs{{{0x01, 20},
                                               {0x15, 11},
                                               {0x20, 13},
                                               {0x32, 20},
                                               {0x50, 20},
                                               {0x6E, 20},
                                               {0x8C, 20},
                                               {0xAA, 20},
                                               {0x46, 10},
                                               {0x82, 10},
                                               {0xBE, 10},
                                               {0xCE, 16},
                                               {0xDF, 10},
                                               {0xEE, 5},
                                               {0xF3, 6},
                                               {0xF9, 7}}};
    if (row == 2 && bit == 13)
    {
        return 0x31;
    }
    if (row == 8 && bit >= 10)
    {
        return 0x64 + bit - 10;
    }
    if (row == 9 && bit >= 10)
    {
        return 0xA0 + bit - 10;
    }
    if (row == 10 && bit >= 10)
    {
        return bit < 14 ? 0x2D + bit - 10 : 0xC8 + bit - 14;
    }
    if (row == 11 && bit >= 16)
    {
        return bit == 16 ? 0xE9 : bit == 17 ? 0xDE : 0;
    }
    if (row == 12 && bit >= 10)
    {
        return bit < 14 ? 0xEA + bit - 10 : 0;
    }
    return bit < runs[row].count ? runs[row].first + bit : 0;
}

// A temple's services in its Heal menu's order: the temple level that offers
// each and its list price in platinum (temple.c). Restoration (level 7,
// 1,500) is left out: the original rules have no such service yet.
struct TempleOffer
{
    rules::TempleService service;
    std::string_view name;
    unsigned level, price;
};

constexpr std::array<TempleOffer, 11> temple_offers{{
    {rules::TempleService::cure_light_wounds, "Cure Light Wounds", 1, 20},
    {rules::TempleService::cure_blindness, "Cure Blindness", 3, 200},
    {rules::TempleService::cure_disease, "Cure Disease", 3, 200},
    {rules::TempleService::remove_curse, "Remove Curse", 3, 700},
    {rules::TempleService::neutralize_poison, "Neutralize Poison", 4, 200},
    {rules::TempleService::cure_serious_wounds, "Cure Serious Wounds", 4, 70},
    {rules::TempleService::cure_critical_wounds, "Cure Critical Wounds", 5, 120},
    {rules::TempleService::raise_dead, "Raise Dead", 5, 1100},
    {rules::TempleService::heal, "Heal", 6, 1000},
    {rules::TempleService::stone_to_flesh, "Stone to Flesh", 6, 400},
    {rules::TempleService::resurrection, "Resurrection", 7, 2000},
}};

// The kit's class bits of a training hall (combtab.c), by the original
// rules' class IDs.
constexpr std::array<std::pair<unsigned, std::string_view>, 6> hall_class_bits{{
    {0x01, "magic_user"},
    {0x02, "cleric"},
    {0x04, "thief"},
    {0x08, "fighter"},
    {0x10, "paladin"},
    {0x20, "ranger"},
}};

// What the shop asks for an item: its value, at least 1, for the whole
// bundle, at the shop's rate.
[[nodiscard]] unsigned shop_price(const por::Equipment &item, unsigned rate)
{
    const unsigned count = std::max<unsigned>(item.stored.stack_size, 1);
    return rated_price(std::max<unsigned>(item.stored.value, 1) * count, rate);
}

} // namespace

// The kit's price of `price` at a rate (temple.c, rate_price): 0 free, 1 to
// 8 a hundredth to a half, 9 two thirds, 10 the price, 11 half again, 12 to
// 19 two to a hundred times.
unsigned rated_price(unsigned price, unsigned rate)
{
    static constexpr std::array<unsigned, 8> divisors{100, 50, 20, 10, 5, 4, 3, 2};
    static constexpr std::array<unsigned, 8> multipliers{2, 3, 4, 5, 10, 20, 50, 100};
    // The kit reads past its table above rate 19; the highest rate stands in
    // (docs/ORIGINAL-RULES.md, Bugs).
    rate = std::min(rate, 19U);
    price = std::max(price, 1U);
    if (rate == 0)
    {
        return 0;
    }
    if (rate <= 8)
    {
        return std::max(price / divisors[rate - 1], 1U);
    }
    switch (rate)
    {
    case 9:
        return std::max(price * 2 / 3, 1U);
    case 10:
        return price;
    case 11:
        return price * 3 / 2;
    default:
        return price * multipliers[rate - 12];
    }
}

// What the kit says, as a host message the view translates; the designer's
// words already said join it as they are.
void UaSession::say_kit(Message message)
{
    if (snapshot_.host_dialogue.empty() && !snapshot_.dialogue.empty())
    {
        snapshot_.host_dialogue.push_back({"{text}", {{"text", snapshot_.dialogue}}});
    }
    if (!snapshot_.dialogue.empty())
    {
        snapshot_.dialogue += '\n';
    }
    snapshot_.dialogue += english(message);
    snapshot_.host_dialogue.push_back(std::move(message));
}

void UaSession::ask_kit(std::vector<Message> choices, std::function<void(std::size_t)> then)
{
    std::vector<std::string> words;
    for (const auto &choice : choices)
    {
        words.push_back(english(choice));
    }
    ask(std::move(words), std::move(then));
    snapshot_.host_choices = std::move(choices);
}

void UaSession::type_answer(bool number, std::function<void(std::string_view)> then)
{
    pending_.wait = Wait::typing;
    pending_.resume = [then = std::move(then)](std::size_t, std::string_view typed)
    {
        then(typed);
    };
    snapshot_.phase = TourPhase::awaiting_input;
    snapshot_.number_input = number;
    snapshot_.choices.clear();
    snapshot_.host_choices.clear();
    snapshot_.continue_ticket = next_ticket_++;
    ++snapshot_.prompts;
}

// The party's members by name and the kit's Exit; `then` takes the member
// picked, or none for Exit.
void UaSession::pick_member(std::function<void(MemberId)> then)
{
    const auto members = party_members();
    std::vector<Message> choices;
    for (const auto id : members)
    {
        choices.push_back({"{name}", {{"name", party_->member(id).character.sheet().name}}});
    }
    choices.push_back({"Exit", {}});
    ask_kit(std::move(choices),
            [members, then = std::move(then)](std::size_t choice)
            {
                then(choice < members.size() ? members[choice] : MemberId{});
            });
}

std::string UaSession::member_name(MemberId id) const
{
    return party_->member(id).character.sheet().name;
}

bool UaSession::member_active(MemberId id) const
{
    const auto &vitals = party_->member(id).vitals;
    return !vitals.dead && vitals.hit_points > 0;
}

// The kit's prices are platinum; the party's purse pays them at its rates.
bool UaSession::pay_platinum(unsigned amount)
{
    const auto rates = party_->coin_rates();
    const auto gold = std::uint64_t{amount} * rates.worth[4] / rates.worth[3];
    if (gold_value(party_->state().wealth, rates) < gold)
    {
        return false;
    }
    party_->set_wealth(pay_gold(party_->state().wealth, gold, rates).purse);
    return true;
}

std::uint32_t UaSession::platinum_in_gold(unsigned amount) const
{
    const auto rates = party_->coin_rates();
    return static_cast<std::uint32_t>(std::uint64_t{amount} * rates.worth[4] / rates.worth[3]);
}

// How a service event ends on its own: its step back (byte 7, bit 2) and
// the event it chose. Inside a small town the town's menu comes back instead.
UaSession::ServiceEnd UaSession::service_end(const EventRecord &event)
{
    return [this, step_back = (event[7] & 4) != 0](unsigned next)
    {
        step_back_due_ = step_back_due_ || step_back;
        finish_kind(next);
    };
}

// Kind 6: a hall training the classes of byte 8's bits for a fee in
// platinum, 1,000 at byte 9's rate (evcmds.c, event_train; train.c).
void UaSession::training_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    const auto open = [this, event, end]
    {
        say_kit({"Does the party want to train?", {}});
        ask_yes_no(
            [this, event, end](bool yes)
            {
                if (!yes)
                {
                    end(0);
                    return;
                }
                hall_menu(event, end);
            });
    };
    if (in_town)
    {
        open();
        return;
    }
    show_event_text(event, open);
}

void UaSession::hall_menu(const EventRecord &event, ServiceEnd end)
{
    const auto fee = rated_price(1000, event[9]);
    say_kit({"Training costs {fee} platinum. Who will train?", {{"fee", std::to_string(fee)}}});
    pick_member(
        [this, event, end, fee](MemberId id)
        {
            if (!id)
            {
                end(0);
                return;
            }
            train_member(event, end, fee, id);
        });
}

void UaSession::train_member(const EventRecord &event, ServiceEnd end, unsigned fee, MemberId id)
{
    const auto again = [this, event, end]
    {
        wait_for_return(
            [this, event, end]
            {
                hall_menu(event, end);
            });
    };
    const auto &sheet = party_->member(id).character.sheet();
    std::vector<std::string> taught;
    for (const auto &[bit, klass] : hall_class_bits)
    {
        if (event[8] & bit)
        {
            taught.emplace_back(klass);
        }
    }
    const auto here = [&taught](const std::string &klass)
    {
        return std::ranges::find(taught, klass) != taught.end();
    };
    std::vector<std::string> rising;
    for (auto &klass : party_->rule_module().classes_to_train(sheet, party_->member(id).experience))
    {
        if (here(klass))
        {
            rising.push_back(std::move(klass));
        }
    }
    if (!member_active(id))
    {
        say_kit({"We only train conscious people.", {}});
    }
    else if (std::ranges::none_of(sheet.level_classes, here))
    {
        say_kit({"We don't train that class here.", {}});
    }
    else if (rising.empty())
    {
        say_kit({"Not enough experience.", {}});
    }
    else if (!pay_platinum(fee))
    {
        say_kit({"Training costs {fee} platinum.", {{"fee", std::to_string(fee)}}});
    }
    else
    {
        auto choice = party_->default_advancement(id);
        choice.training["hall"] = taught;
        const auto spells = std::ranges::find(rising, "magic_user") != rising.end()
                                ? party_->advancement_options(id, choice).spells
                                : std::vector<rules::AdvancementOption>{};
        if (!spells.empty())
        {
            std::vector<Message> labels;
            for (const auto &spell : spells)
            {
                labels.push_back({"{spell}", {{"spell", spell.label, true}}});
            }
            say_kit({"Choose a spell for {name} to learn.", {{"name", member_name(id)}}});
            ask_kit(std::move(labels),
                    [this, id, choice, spells, again](std::size_t picked) mutable
                    {
                        choice.spells = {spells[picked].id};
                        party_->advance(id, choice);
                        say_kit({"Congratulations... {name} has trained.",
                                 {{"name", member_name(id)}}});
                        again();
                    });
            return;
        }
        party_->advance(id, choice);
        say_kit({"Congratulations... {name} has trained.", {{"name", member_name(id)}}});
    }
    again();
}

// Kind 7: a tavern's drinks, tales and brawl (mapevent.c, event_tavern).
void UaSession::tavern_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    drunkenness_ = 0;
    const auto open = [this, event, in_town, end]
    {
        tavern_menu(event, in_town, end);
    };
    if (word_at(event, 4) == 0 || in_town)
    {
        say_kit({"The party enters a tavern.", {}});
        open();
        return;
    }
    say(text(word_at(event, 4)));
    open();
}

void UaSession::tavern_menu(const EventRecord &event, bool in_town, ServiceEnd end)
{
    std::vector<std::string> tales;
    for (std::size_t at = 8; at < 16; at += 2)
    {
        if (const auto slot = word_at(event, at); slot != 0)
        {
            tales.push_back(text(slot));
        }
    }
    for (const auto &tale : tavern_tales_)
    {
        if (!tale.empty())
        {
            tales.push_back(tale);
        }
    }
    enum class Pick
    {
        drink,
        talk,
        fight,
        leave
    };
    std::vector<Message> choices;
    std::vector<Pick> picks;
    const auto offer = [&](bool shown, std::string_view label, Pick pick)
    {
        if (shown)
        {
            choices.push_back({std::string(label), {}});
            picks.push_back(pick);
        }
    };
    offer((event[7] & 0x20) != 0, "Drink", Pick::drink);
    offer(!tales.empty(), "Talk", Pick::talk);
    offer((event[7] & 0x10) != 0, "Fight", Pick::fight);
    offer(true, "Leave", Pick::leave);
    const auto again = [this, event, in_town, end]
    {
        wait_for_return(
            [this, event, in_town, end]
            {
                tavern_menu(event, in_town, end);
            });
    };
    ask_kit(std::move(choices),
            [this, event, in_town, end, tales, picks, again](std::size_t choice)
            {
                switch (picks[choice])
                {
                case Pick::talk:
                {
                    const auto count = static_cast<unsigned>(tales.size());
                    const auto n = (event[7] & 8) ? roll(count) - 1 : tavern_tale_next_++ % count;
                    say(tales[n]);
                    again();
                    return;
                }
                case Pick::fight:
                    if (event[0x12] != 0 && !in_town)
                    {
                        end(event[0x12]);
                        return;
                    }
                    say_kit({"Everyone runs away. There's no one to fight!", {}});
                    again();
                    return;
                case Pick::drink:
                    drink(event, in_town, end);
                    return;
                case Pick::leave:
                    end(0);
                    return;
                }
            });
}

void UaSession::drink(const EventRecord &event, bool in_town, ServiceEnd end)
{
    const auto again = [this, event, in_town, end]
    {
        wait_for_return(
            [this, event, in_town, end]
            {
                tavern_menu(event, in_town, end);
            });
    };
    const auto menu = word_at(event, 0x10);
    const auto drinks =
        menu == 0 || in_town ? std::vector<std::string>{} : menu_words(text(menu, false));
    if (drinks.empty())
    {
        say_kit({"The party orders a local drink.", {}});
        again();
        return;
    }
    say_kit({"What's your pleasure?", {}});
    ask(drinks,
        [this, event, end, again](std::size_t choice)
        {
            // Each drink is stronger than the one before it (mapevent.c).
            static constexpr std::array<unsigned, 5> strength{0, 6, 10, 15, 30};
            drunkenness_ += strength[std::min<std::size_t>(choice, 4)];
            if (drunkenness_ >= 60 && event[0x13] != 0)
            {
                end(event[0x13]);
                return;
            }
            say_kit({"The party has a drink.", {}});
            again();
        });
}

// Kind 8: a shop whose stock four masks pick from ITEM.DAT, at byte 5's rate
// (mapevent.c, event_shop; shop.c).
void UaSession::shop_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    shop_stock_.clear();
    for (std::size_t mask = 8; mask < 20; mask += 3)
    {
        const unsigned row = event[mask] >> 4;
        for (unsigned bit = 0; bit < 20; ++bit)
        {
            const std::size_t byte = mask + 2 - bit / 8;
            if (event[byte] & (1U << (bit % 8)))
            {
                if (const auto number = stock_item(row, bit); number != 0)
                {
                    shop_stock_.push_back(number);
                }
            }
        }
    }
    if (in_town)
    {
        say_kit({"The party enters a local shop.", {}});
    }
    else
    {
        say(text(word_at(event, 4)));
        say_kit({"May I help you?", {}});
    }
    shop_menu(event, end);
}

void UaSession::shop_menu(const EventRecord &event, ServiceEnd end)
{
    std::vector<Message> choices;
    if (!shop_stock_.empty())
    {
        choices.push_back({"Buy", {}});
    }
    choices.push_back({"Sell", {}});
    choices.push_back({"Identify", {}});
    choices.push_back({"Exit", {}});
    const bool buys = !shop_stock_.empty();
    ask_kit(std::move(choices),
            [this, event, end, buys](std::size_t choice)
            {
                const auto action = choice + (buys ? 0 : 1);
                if (action == 0)
                {
                    shop_buy(event, end);
                }
                else if (action == 1)
                {
                    shop_sell(event, end, false);
                }
                else if (action == 2)
                {
                    shop_sell(event, end, true);
                }
                else
                {
                    end(0);
                }
            });
}

void UaSession::shop_buy(const EventRecord &event, ServiceEnd end)
{
    std::vector<Message> choices;
    std::vector<por::Equipment> wares;
    for (const auto number : shop_stock_)
    {
        auto item = number == 0xFF ? random_scroll() : design_item(items_, number, true);
        if (!item)
        {
            continue;
        }
        const auto price = shop_price(*item, event[5]);
        choices.push_back({"{item} / {price} platinum",
                           {{"item", item->label()}, {"price", std::to_string(price)}}});
        wares.push_back(std::move(*item));
    }
    choices.push_back({"Exit", {}});
    ask_kit(std::move(choices),
            [this, event, end, wares](std::size_t choice)
            {
                const auto back = [this, event, end]
                {
                    wait_for_return(
                        [this, event, end]
                        {
                            shop_menu(event, end);
                        });
                };
                if (choice >= wares.size())
                {
                    shop_menu(event, end);
                    return;
                }
                const auto &item = wares[choice];
                const auto buyer = party_->selected();
                const auto price = shop_price(item, event[5]);
                if (gold_value(party_->state().wealth, party_->coin_rates()) <
                    platinum_in_gold(price))
                {
                    say_kit({"Not enough money.", {}});
                    back();
                    return;
                }
                try
                {
                    party_->take_item(buyer, item);
                }
                catch (const std::runtime_error &)
                {
                    say_kit({"{name} is overloaded.", {{"name", member_name(buyer)}}});
                    back();
                    return;
                }
                (void)pay_platinum(price);
                say_kit({"{name} buys {item}.",
                         {{"name", member_name(buyer)}, {"item", item.label()}}});
                back();
            });
}

// Sells, or identifies for 20 platinum, an item the selected member carries.
void UaSession::shop_sell(const EventRecord &event, ServiceEnd end, bool identify)
{
    const auto seller = party_->selected();
    const auto &items = party_->member(seller).character.inventory().items();
    std::vector<Message> choices;
    std::vector<std::uint64_t> ids;
    for (const auto &item : items)
    {
        choices.push_back({"{item}", {{"item", item.name}}});
        ids.push_back(item.id);
    }
    choices.push_back({"Exit", {}});
    ask_kit(std::move(choices),
            [this, event, end, identify, seller, ids](std::size_t choice)
            {
                const auto back = [this, event, end]
                {
                    wait_for_return(
                        [this, event, end]
                        {
                            shop_menu(event, end);
                        });
                };
                if (choice >= ids.size())
                {
                    shop_menu(event, end);
                    return;
                }
                if (identify)
                {
                    constexpr unsigned identify_price = 20;
                    if (gold_value(party_->state().wealth, party_->coin_rates()) <
                            platinum_in_gold(identify_price) ||
                        !party_->identify_at_shop(seller, ids[choice]))
                    {
                        say_kit({"Not enough money.", {}});
                    }
                    else
                    {
                        say_kit({"The item is identified.", {}});
                    }
                    back();
                    return;
                }
                // Half the shop's price; nothing for a cursed item or one of
                // no value, which the shop only takes away (shop.c).
                const auto &sources = party_->member(seller).item_sources;
                const auto found = sources.find(ids[choice]);
                unsigned price = 0;
                if (found != sources.end() && found->second.stored.value != 0 &&
                    found->second.stored.cursed_raw == 0)
                {
                    price = shop_price(found->second, event[5]);
                    price = price > 1 ? price / 2 : price;
                }
                party_->drop(seller, ids[choice]);
                if (price != 0)
                {
                    party_->add_coins(Coin::platinum, price);
                }
                say_kit({"Sold for {price} platinum.", {{"price", std::to_string(price)}}});
                back();
            });
}

// Kind 9: a temple's cures at byte 4's level and byte 5's rate, and the
// donations that may run event 0Ch (evcmds.c, event_temple; temple.c).
void UaSession::temple_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    const auto welcome = in_town ? 0 : word_at(event, 0xD);
    if (welcome != 0)
    {
        say(text(welcome));
    }
    else
    {
        say_kit({"\"Welcome to our temple.\"", {}});
    }
    temple_menu(event, in_town, end);
}

void UaSession::temple_menu(const EventRecord &event, bool in_town, ServiceEnd end)
{
    const auto greeting = in_town ? 0 : word_at(event, 0xF);
    if (greeting != 0)
    {
        say(text(greeting));
    }
    else
    {
        say_kit({"What is your wish?", {}});
    }
    const bool donations = (event[7] & 8) != 0;
    std::vector<Message> choices{{"Heal", {}}};
    if (donations)
    {
        choices.push_back({"Donate", {}});
    }
    choices.push_back({"Leave", {}});
    ask_kit(std::move(choices),
            [this, event, in_town, end, donations](std::size_t choice)
            {
                if (choice == 0)
                {
                    temple_heal(event, in_town, end);
                }
                else if (donations && choice == 1)
                {
                    donate(event, in_town, end);
                }
                else
                {
                    end(0);
                }
            });
}

void UaSession::temple_heal(const EventRecord &event, bool in_town, ServiceEnd end)
{
    std::vector<Message> choices;
    std::vector<TempleOffer> offered;
    for (const auto &offer : temple_offers)
    {
        if (offer.level <= event[4])
        {
            const auto price = rated_price(offer.price, event[5]);
            choices.push_back(
                {"{service} / {price} platinum",
                 {{"service", std::string(offer.name), true}, {"price", std::to_string(price)}}});
            offered.push_back(offer);
        }
    }
    choices.push_back({"Exit", {}});
    ask_kit(std::move(choices),
            [this, event, in_town, end, offered](std::size_t choice)
            {
                if (choice >= offered.size())
                {
                    temple_menu(event, in_town, end);
                    return;
                }
                const auto offer = offered[choice];
                say_kit({"Who is to be cured?", {}});
                pick_member(
                    [this, event, in_town, end, offer](MemberId id)
                    {
                        if (id)
                        {
                            cure(offer.service, rated_price(offer.price, event[5]), id);
                        }
                        wait_for_return(
                            [this, event, in_town, end]
                            {
                                temple_menu(event, in_town, end);
                            });
                    });
            });
}

void UaSession::cure(rules::TempleService service, unsigned price, MemberId id)
{
    if (!party_->temple_service_helps(id, service))
    {
        say_kit({"{name} has no need of it.", {{"name", member_name(id)}}});
        return;
    }
    if (gold_value(party_->state().wealth, party_->coin_rates()) < platinum_in_gold(price))
    {
        say_kit({"Not enough money.", {}});
        return;
    }
    party_->temple_service(id, service, platinum_in_gold(price));
    say_kit({"{name} is cured.", {{"name", member_name(id)}}});
}

void UaSession::donate(const EventRecord &event, bool in_town, ServiceEnd end)
{
    say_kit({"How much platinum do you donate?", {}});
    type_answer(true,
                [this, event, in_town, end](std::string_view typed)
                {
                    unsigned amount = 0;
                    for (const char c : typed)
                    {
                        if (c >= '0' && c <= '9' && amount < 1000000)
                        {
                            amount = amount * 10 + static_cast<unsigned>(c - '0');
                        }
                    }
                    if (amount != 0 && pay_platinum(amount))
                    {
                        donated_ += amount;
                        say_kit({"Thank you for your generous donation.", {}});
                    }
                    else if (amount != 0)
                    {
                        say_kit({"Not enough money.", {}});
                    }
                    // A tithe reaching the goal (bytes 8 to 11) runs event 0Ch.
                    if (!in_town && event[0xC] != 0 && donated_ >= long_at(event, 8))
                    {
                        donated_ = 0;
                        end(event[0xC]);
                        return;
                    }
                    wait_for_return(
                        [this, event, in_town, end]
                        {
                            temple_menu(event, in_town, end);
                        });
                });
}

// Kind 24: a vault the party's money is left in and taken from.
void UaSession::vault_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    if (!in_town && word_at(event, 4) != 0)
    {
        say(text(word_at(event, 4)));
    }
    else
    {
        say_kit({"The party enters the vault.", {}});
    }
    vault_menu(end);
}

void UaSession::vault_menu(ServiceEnd end)
{
    constexpr std::array<Coin, 3> kept{Coin::platinum, Coin::gems, Coin::jewelry};
    say_kit({"The vault holds {platinum} platinum, {gems} gems and {jewelry} jewelry.",
             {{"platinum", std::to_string(vault_[0])},
              {"gems", std::to_string(vault_[1])},
              {"jewelry", std::to_string(vault_[2])}}});
    ask_kit({{"Money", {}}, {"Take", {}}, {"Exit", {}}},
            [this, end, kept](std::size_t choice)
            {
                if (choice == 2)
                {
                    end(0);
                    return;
                }
                auto wealth = party_->state().wealth;
                for (std::size_t n = 0; n < kept.size(); ++n)
                {
                    auto &carried = wealth[static_cast<std::size_t>(kept[n])];
                    auto &stored = vault_[n];
                    if (choice == 0)
                    {
                        stored += carried;
                        carried = 0;
                    }
                    else
                    {
                        carried += stored;
                        stored = 0;
                    }
                }
                party_->set_wealth(wealth);
                vault_menu(end);
            });
}

// Kind 29: an inn, where the party rests with no encounter.
void UaSession::inn_event(const EventRecord &event, bool in_town, ServiceEnd end)
{
    if (!in_town && word_at(event, 4) != 0)
    {
        say(text(word_at(event, 4)));
    }
    else
    {
        say_kit({"The party enters a local Inn.", {}});
    }
    ask_kit({{"Rest", {}}, {"Exit", {}}},
            [this, end](std::size_t choice)
            {
                if (choice == 0 && party_)
                {
                    (void)party_->rest();
                    // The kit's rest memorizes the spells each caster had.
                    while (party_->state().spell_rest)
                    {
                        party_->keep_rest_spells(party_->state().spell_rest->members.front());
                    }
                    say_kit({"The party rests.", {}});
                    wait_for_return(
                        [end]
                        {
                            end(0);
                        });
                    return;
                }
                end(0);
            });
}

// Kind 22: a small town's services, each built from the town's own bytes
// (mapevent.c, event_town).
void UaSession::town_event(const EventRecord &event)
{
    say(text(word_at(event, 4)));

    struct Service
    {
        std::string_view label;
        unsigned bit;
    };

    static constexpr std::array<Service, 6> services{{{"Temple", 0x01},
                                                      {"Hall", 0x02},
                                                      {"Shop", 0x04},
                                                      {"Inn", 0x08},
                                                      {"Pub", 0x10},
                                                      {"Vault", 0x20}}};
    std::vector<Message> choices;
    std::vector<unsigned> bits;
    for (const auto &service : services)
    {
        if (event[7] & service.bit)
        {
            choices.push_back({std::string(service.label), {}});
            bits.push_back(service.bit);
        }
    }
    choices.push_back({"Leave", {}});
    ask_kit(std::move(choices),
            [this, event, bits](std::size_t choice)
            {
                if (choice >= bits.size())
                {
                    return;
                }
                const ServiceEnd back = [this, event](unsigned)
                {
                    town_event(event);
                };
                EventRecord service{};
                service[0] = static_cast<std::uint8_t>(EventKind::small_town);
                switch (bits[choice])
                {
                case 0x01:
                    service[4] = static_cast<std::uint8_t>((event[9] & 0xE0) >> 5);
                    service[5] = event[0xC] & 0x1F;
                    temple_event(service, true, back);
                    return;
                case 0x02:
                    service[8] = event[8];
                    service[9] = event[0xC] & 0x1F;
                    training_event(service, true, back);
                    return;
                case 0x04:
                    service[5] = event[9] & 0x1F;
                    std::copy(event.begin() + 0xE, event.end(), service.begin() + 0xE);
                    shop_event(service, true, back);
                    return;
                case 0x08:
                    inn_event(service, true, back);
                    return;
                case 0x10:
                    service[7] = 0x20;
                    service[8] = event[0xA];
                    service[9] = event[0xB];
                    tavern_event(service, true, back);
                    return;
                default:
                    vault_event(service, true, back);
                    return;
                }
            });
}

// Kind 12: the party led along a path of turns and steps, through walls,
// with two texts on the way (evcmds.c 1637-1708).
void UaSession::tour_event(const EventRecord &event)
{
    if (area_.overland())
    {
        return;
    }
    rerun_square_ = (event[7] & 0x20) != 0;
    if (event[7] & 0x10)
    {
        move_party(std::min<unsigned>(event[5], area_.area.columns - 1U),
                   std::min<unsigned>(event[4], area_.area.rows - 1U),
                   por::MapDirection{((event[7] & 0xCU) >> 1) / 2 % 4});
    }
    party_moved_ = true;
    tour_step(event, 0);
}

void UaSession::tour_step(const EventRecord &event, unsigned step)
{
    for (; step <= event[6]; ++step)
    {
        for (const auto [at, words] : {std::pair{0xE, 0x10}, std::pair{0xF, 0x12}})
        {
            if (event[at] == step && word_at(event, words) != 0 && !tour_text_shown_[at - 0xE])
            {
                tour_text_shown_[at - 0xE] = true;
                say(text(word_at(event, words)));
                wait_for_return(
                    [this, event, step]
                    {
                        tour_step(event, step);
                    });
                return;
            }
        }
        tour_text_shown_ = {};
        if (step == event[6])
        {
            break;
        }
        auto &pose = snapshot_.pose;
        step_back_pose_ = pose;
        switch ((event[8 + step / 4] >> (step % 4 * 2)) & 3)
        {
        case 1:
            pose.facing = por::turned_left(pose.facing);
            break;
        case 2:
            pose.facing = por::turned_right(pose.facing);
            break;
        case 3:
        {
            const int columns = area_.area.columns, rows = area_.area.rows;
            const auto x =
                (static_cast<int>(pose.x) + por::step_x(pose.facing) + columns) % columns;
            const auto y = (static_cast<int>(pose.y) + por::step_y(pose.facing) + rows) % rows;
            move_party(static_cast<unsigned>(x), static_cast<unsigned>(y), pose.facing);
            advance_minutes(1);
            break;
        }
        default:
            break;
        }
    }
    tour_text_shown_ = {};
}

// MONST record `number` as a member of the party under the kit's rules.
std::optional<Character> UaSession::npc_character(unsigned number) const
{
    const auto record = files_.data("MONST", number);
    const auto monster = record ? decode_monster(*record) : std::nullopt;
    const auto &creation = party_->character_rules();
    if (!monster || !creation)
    {
        return std::nullopt;
    }
    const auto &raw = monster->raw;
    // The kit's races (0 elf, 1 half-elf, 2 dwarf, 3 gnome, 4 halfling, 5
    // human, 6 a monster, taken as human) by the original rules' numbers.
    static constexpr std::array<original::Race, 7> races{
        original::Race::elf,   original::Race::half_elf, original::Race::dwarf,
        original::Race::gnome, original::Race::halfling, original::Race::human,
        original::Race::human};
    rules::CharacterSheet sheet;
    sheet.name = monster->name;
    sheet.gender = "Male";
    rules::CharacterSheet::Original numbers;
    for (std::size_t slot = 0; slot < 7; ++slot)
    {
        numbers.class_levels[slot] = monster->levels[slot];
    }
    // The kit's knight (Krynn's) is a fighter; its slot 1 is the druid's.
    numbers.class_levels[2] = std::max(numbers.class_levels[2], numbers.class_levels[1]);
    numbers.class_levels[1] = 0;
    numbers.race = static_cast<int>(races[std::min<std::size_t>(raw[0x58], 6)]);
    numbers.profession = raw[0x59] == 1 ? 2 : std::min<int>(raw[0x59], 16);
    numbers.alignment = std::min<int>(raw[0x5D], 8);
    for (std::size_t level = 0; level < 8; ++level)
    {
        if (numbers.class_levels[level] > 0)
        {
            sheet.level_classes.emplace_back(hall_class_id(level));
        }
    }
    if (sheet.level_classes.empty())
    {
        numbers.class_levels[2] = 1;
        numbers.profession = 2;
        sheet.level_classes.emplace_back("fighter");
    }
    // The six abilities, a base and a current score each from +70h:
    // Strength, Intelligence, Wisdom, Dexterity, Constitution, Charisma.
    static constexpr std::array<rules::Ability, 6> abilities{
        rules::Ability::strength,  rules::Ability::intelligence, rules::Ability::wisdom,
        rules::Ability::dexterity, rules::Ability::constitution, rules::Ability::charisma};
    for (std::size_t n = 0; n < abilities.size(); ++n)
    {
        sheet.scores[abilities[n]] = std::clamp<int>(raw[0x71 + 2 * n], 3, 25);
    }
    sheet.base = sheet.scores;
    sheet.hit_points = std::max<int>(raw[0x81], 1);
    sheet.original = numbers;
    const auto experience =
        static_cast<unsigned>(raw[0x44] | raw[0x45] << 8 | raw[0x46] << 16 | raw[0x47] << 24);
    const auto draft = creation->imported_draft(sheet, experience);
    if (!draft)
    {
        return std::nullopt;
    }
    return Character(*creation, *draft, {});
}

std::string_view UaSession::hall_class_id(std::size_t slot)
{
    static constexpr std::array<std::string_view, 8> ids{
        "cleric", "druid", "fighter", "paladin", "ranger", "magic_user", "thief", "monk"};
    return ids[slot];
}

// The member a design's NPC (MONST `number`, 0 any of 70h to 7Fh) joined as.
MemberId UaSession::npc_member(unsigned number) const
{
    for (const auto id : party_members())
    {
        const auto &source = party_->member(id).npc_source;
        constexpr std::string_view prefix = "UA:MONST:";
        if (!source.starts_with(prefix))
        {
            continue;
        }
        const auto joined = static_cast<unsigned>(std::stoul(source.substr(prefix.size())));
        if (number == 0 ? joined >= 0x70 && joined <= 0x7F : joined == number)
        {
            return id;
        }
    }
    return {};
}

// Kind 13: an NPC (MONST byte 8) joins with byte 9's percent of its Hit
// Points, unless it is with the party already or the party is full.
void UaSession::add_npc_event(const EventRecord &event)
{
    if (!party_ || npc_member(event[8]))
    {
        return;
    }
    show_event_text(event,
                    [this, event]
                    {
                        const auto &slots = party_->state().slots;
                        if (std::ranges::count_if(slots,
                                                  [](MemberId id)
                                                  {
                                                      return static_cast<bool>(id);
                                                  }) >= 8)
                        {
                            return;
                        }
                        auto character = npc_character(event[8]);
                        if (!character)
                        {
                            diagnostics_.push_back("NPC " + std::to_string(event[8]) +
                                                   " cannot join under the kit's rules");
                            return;
                        }
                        const auto id =
                            party_->recruit(monster_key(event[8]), std::move(*character));
                        const int most = party_->hit_point_maximum(id);
                        const int wanted = std::clamp(most * event[9] / 100, 1, 250);
                        if (wanted < party_->member(id).vitals.hit_points)
                        {
                            rules::HazardAttack wound;
                            wound.automatic = true;
                            wound.damage_bonus = party_->member(id).vitals.hit_points - wanted;
                            (void)party_->hazard_attack(id, wound);
                        }
                    });
}

// Kind 32: an NPC leaves the party.
void UaSession::remove_npc_event(const EventRecord &event)
{
    if (!party_)
    {
        return;
    }
    if (const auto id = npc_member(event[8]))
    {
        party_->remove(id);
        show_event_text(event,
                        []
                        {
                        });
    }
}

// Kind 15: monsters met at a distance, approached, fled or fought, as the
// event's five buttons say (evcmds.c 1106-1196).
void UaSession::encounter_event(const EventRecord &event)
{
    say(text(word_at(event, 4)));
    encounter_range_ = (event[7] & 3) + 1;
    encounter_turn(event);
}

void UaSession::encounter_turn(const EventRecord &event)
{
    if (encounter_range_ == 0)
    {
        encounter_action(event, event[0xE] & 0xF);
        return;
    }
    static constexpr std::array<std::string_view, 5> labels{"Approach", "Retreat", "Fight", "Wait",
                                                            "Talk"};
    std::vector<Message> choices;
    std::vector<unsigned> actions;
    for (std::size_t k = 0; k < labels.size(); ++k)
    {
        const auto button = event[9 + k];
        const bool restricted = (button & 0x10) != 0;
        const bool shown = (button & 8) && !(restricted && k == 0 && encounter_range_ <= 1) &&
                           !(restricted && (k == 2 || k == 4) && encounter_range_ >= 2);
        if (shown)
        {
            choices.push_back({std::string(labels[k]), {}});
            actions.push_back(button & 7U);
        }
    }
    if (choices.empty())
    {
        return;
    }
    ask_kit(std::move(choices),
            [this, event, actions](std::size_t choice)
            {
                encounter_action(event, actions[choice]);
            });
}

void UaSession::encounter_action(const EventRecord &event, unsigned action)
{
    // The party's slowest member, at half its movement, against the
    // monsters' speed (byte 8).
    unsigned slowest = 255;
    for (const auto id : party_members())
    {
        slowest = std::min<unsigned>(
            slowest, static_cast<unsigned>(std::max(party_->profile(id).movement_feet, 0)) / 5 / 2);
    }
    const bool monsters_faster = event[8] > slowest;
    switch (action)
    {
    case 0:
        if (encounter_range_ > 0)
        {
            --encounter_range_;
        }
        encounter_turn(event);
        return;
    case 1:
        encounter_surprise_ = monsters_faster ? Surprise::party : Surprise::none;
        finish_kind(event[0x11]);
        return;
    case 2:
        encounter_surprise_ = monsters_faster ? Surprise::none : Surprise::monsters;
        finish_kind(event[0x11]);
        return;
    case 3:
        encounter_surprise_ = Surprise::none;
        finish_kind(event[0x11]);
        return;
    case 4:
        finish_kind(event[0x12]);
        return;
    case 5:
        finish_kind(monsters_faster ? event[0x11] : event[0x13]);
        return;
    default:
        // At range 0 an action that does nothing would ask forever.
        if (encounter_range_ != 0)
        {
            encounter_turn(event);
        }
        return;
    }
}

// What a test's success or failure leads to (mapevent.c, event_outcome).
void UaSession::outcome(const EventRecord &event, bool passed)
{
    rerun_square_ = (event[0xA] & 0x40) != 0;
    const auto words = word_at(event, passed ? 0xD : 0xB);
    const unsigned action = passed ? (event[0xA] >> 2) & 3 : event[0xA] & 3;
    const auto act = [this, event, passed, action]
    {
        switch (action)
        {
        case 1:
            finish_kind(event[passed ? 0x10 : 0xF]);
            break;
        case 2:
            move_party(std::min<unsigned>(event[0x11], area_.area.columns - 1U),
                       std::min<unsigned>(event[0x12], area_.area.rows - 1U),
                       por::MapDirection{((event[0xA] & 0x30U) >> 3) / 2 % 4});
            party_moved_ = true;
            break;
        case 3:
            step_back_due_ = true;
            break;
        default:
            break;
        }
    };
    if (words == 0)
    {
        act();
        return;
    }
    say(text(words));
    wait_for_return(act);
}

// Kind 18: a member picked tries a test of an ability or a thief's skill.
void UaSession::who_tries_event(const EventRecord &event)
{
    tries_left_ = std::max<unsigned>(event[9], 1);
    try_once(event);
}

void UaSession::try_once(const EventRecord &event)
{
    say(text(word_at(event, 4)));
    pick_member(
        [this, event](MemberId id)
        {
            if (!id)
            {
                outcome(event, false);
                return;
            }
            if (!member_active(id))
            {
                say_kit({"{name} is in no condition to try.", {{"name", member_name(id)}}});
                wait_for_return(
                    [this, event]
                    {
                        try_once(event);
                    });
                return;
            }
            if (member_passes(event, id))
            {
                outcome(event, true);
                return;
            }
            if (--tries_left_ == 0)
            {
                outcome(event, false);
                return;
            }
            say_kit({"That attempt did not succeed.", {}});
            wait_for_return(
                [this, event]
                {
                    try_once(event);
                });
        });
}

bool UaSession::member_passes(const EventRecord &event, MemberId id)
{
    const unsigned test = (event[7] >> 2) & 0xF;
    if (test <= 1)
    {
        return test == 0;
    }
    const auto &sheet = party_->member(id).character.sheet();
    static constexpr std::array<rules::Ability, 6> abilities{
        rules::Ability::strength,  rules::Ability::intelligence, rules::Ability::wisdom,
        rules::Ability::dexterity, rules::Ability::constitution, rules::Ability::charisma};
    const bool skill = test >= 8;
    const int value = skill ? (sheet.original ? sheet.original->thief_skills[test - 8] : 0)
                            : sheet.scores[abilities[test - 2]];
    if (event[7] & 0x40)
    {
        // Strength 18 against 18 asks for exceptional Strength as well.
        if (test == 2 && value == 18 && event[8] == 18 && sheet.original)
        {
            return sheet.original->exceptional_strength >= 18;
        }
        return value >= event[8];
    }
    return static_cast<int>(roll(skill ? 100 : 20)) <= value;
}

// Kind 19: a member picked pays an amount of gems, jewelry or platinum.
void UaSession::who_pays_event(const EventRecord &event)
{
    say(text(word_at(event, 4)));
    pick_member(
        [this, event](MemberId id)
        {
            const unsigned currency = (event[7] >> 2) & 3;
            if (!id || currency == 0)
            {
                outcome(event, false);
                return;
            }
            const auto amount = word_at(event, 8);
            auto wealth = party_->state().wealth;
            bool paid = false;
            if (currency == 3)
            {
                paid = pay_platinum(amount);
            }
            else
            {
                auto &count =
                    wealth[static_cast<std::size_t>(currency == 1 ? Coin::gems : Coin::jewelry)];
                if (count >= amount)
                {
                    count -= amount;
                    party_->set_wealth(wealth);
                    paid = true;
                }
            }
            if (paid)
            {
                outcome(event, true);
                return;
            }
            say_kit({"{name} doesn't have enough.", {{"name", member_name(id)}}});
            wait_for_return(
                [this, event]
                {
                    who_pays_event(event);
                });
        });
}

// Kind 20: a password typed, with byte 13h's tries.
void UaSession::password_event(const EventRecord &event)
{
    tries_left_ = event[0x13];
    ask_password(event);
}

void UaSession::ask_password(const EventRecord &event)
{
    if (tries_left_ == 0)
    {
        outcome(event, false);
        return;
    }
    say(text(word_at(event, 4)));
    type_answer(false,
                [this, event](std::string_view typed)
                {
                    if (upper(typed) == upper(text(word_at(event, 8), false)))
                    {
                        outcome(event, true);
                        return;
                    }
                    if (--tries_left_ == 0)
                    {
                        outcome(event, false);
                        return;
                    }
                    say_kit({"Wrong, try again.", {}});
                    wait_for_return(
                        [this, event]
                        {
                            ask_password(event);
                        });
                });
}

} // namespace opengold::ua
