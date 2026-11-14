// The treasure menu of an Unlimited Adventures design, after a fight won or
// a treasure event (docs/games/ua.md, The treasure menu; the reconstruction's
// postcmbt.c, loot_menu, loot_take and loot_items). The party has one purse
// (DEVIATIONS.md), so the kit's Pool and Share have nothing to do and Take's
// money all goes into it; the items go to the selected member, whom the
// player may change while the menu waits.
#include "opengold/original_spells.h"
#include "opengold/ua_session.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

using rules::Message;

// The Detect Magic the menu casts: the cleric's and the magic-user's (the
// kit's spells 5 and 0Bh; its 4Dh is no spell the original rules have).
constexpr std::array<int, 2> detect_magic_spells{5, 11};

[[nodiscard]] bool magical(const por::Equipment &item) noexcept
{
    return item.stored.magic_bonus > 0 || item.stored.save_bonus > 0 || item.stored.cursed_raw != 0;
}

} // namespace

bool UaSession::treasure_left() const
{
    return !pile_items_.empty() || std::ranges::any_of(pile_money_,
                                                       [](unsigned count)
                                                       {
                                                           return count != 0;
                                                       });
}

// An item as the menu names it: the kit marks a magic one with a star once
// Detect Magic has shown it (hero.c, item_name).
std::string UaSession::treasure_label(const por::Equipment &item) const
{
    return pile_magic_shown_ && magical(item) ? "* " + item.label() : item.label();
}

// The Detect Magic a member who can act has memorized, the first found.
std::optional<std::string> UaSession::detect_magic_spell() const
{
    if (pile_items_.empty())
    {
        return std::nullopt;
    }
    const auto &rules = party_->rule_module();
    for (const auto id : active_members())
    {
        const auto &m = party_->member(id);
        for (const auto number : detect_magic_spells)
        {
            const auto spell = original::spell_id(number);
            if (rules.can_cast_exploration_spell(m.character.sheet(), m.vitals, spell))
            {
                return spell;
            }
        }
    }
    return std::nullopt;
}

void UaSession::treasure_menu()
{
    if (!treasure_left())
    {
        treasure_open_ = false;
        return;
    }
    treasure_open_ = true;
    enum class Way : std::uint8_t
    {
        take,
        detect,
        exit,
    };
    std::vector<Way> ways{Way::take};
    std::vector<Message> words{{"Take", {}}};
    if (detect_magic_spell())
    {
        ways.push_back(Way::detect);
        words.push_back({"Detect", {}});
    }
    ways.push_back(Way::exit);
    words.push_back({"Exit", {}});
    ask_kit(std::move(words),
            [this, ways](std::size_t choice)
            {
                switch (ways[choice])
                {
                case Way::take:
                    take_treasure();
                    return;
                case Way::detect:
                    detect_treasure_magic();
                    return;
                case Way::exit:
                    leave_treasure();
                    return;
                }
            });
}

// Take asks which, Money or Items, while the treasure holds both.
void UaSession::take_treasure()
{
    const bool money = std::ranges::any_of(pile_money_,
                                           [](unsigned count)
                                           {
                                               return count != 0;
                                           });
    if (!money)
    {
        take_treasure_items();
        return;
    }
    if (pile_items_.empty())
    {
        take_treasure_money();
        return;
    }
    say_kit({"Take:", {}});
    ask_kit({{"Money", {}}, {"Items", {}}, {"Exit", {}}},
            [this](std::size_t choice)
            {
                if (choice == 0)
                {
                    take_treasure_money();
                }
                else if (choice == 1)
                {
                    take_treasure_items();
                }
                else
                {
                    treasure_menu();
                }
            });
}

// The party's one purse takes all the money.
void UaSession::take_treasure_money()
{
    constexpr std::array<Coin, 3> money{Coin::platinum, Coin::gems, Coin::jewelry};
    for (std::size_t n = 0; n < money.size(); ++n)
    {
        if (pile_money_[n] != 0)
        {
            party_->add_coins(money[n], pile_money_[n]);
        }
    }
    pile_money_ = {};
    say_kit({"The party takes the money.", {}});
    wait_for_return(
        [this]
        {
            take_treasure();
        });
}

// The items, one taken at a time by the selected member, until none is left
// or the player is done.
void UaSession::take_treasure_items()
{
    if (pile_items_.empty())
    {
        treasure_menu();
        return;
    }
    std::vector<Message> choices;
    for (const auto &item : pile_items_)
    {
        choices.push_back({"{item}", {{"item", treasure_label(item)}}});
    }
    choices.push_back({"Exit", {}});
    say_kit({"Items:", {}});
    ask_kit(std::move(choices),
            [this](std::size_t choice)
            {
                if (choice >= pile_items_.size())
                {
                    treasure_menu();
                    return;
                }
                const auto taker = party_->selected();
                try
                {
                    party_->take_item(taker, pile_items_[choice]);
                }
                catch (const std::runtime_error &)
                {
                    say_kit({"{name} is overloaded.", {{"name", member_name(taker)}}});
                    wait_for_return(
                        [this]
                        {
                            take_treasure_items();
                        });
                    return;
                }
                pile_items_.erase(pile_items_.begin() + static_cast<std::ptrdiff_t>(choice));
                take_treasure_items();
            });
}

// The member casts Detect Magic, spending it, and the menu marks the magic
// items from then on.
void UaSession::detect_treasure_magic()
{
    if (const auto spell = detect_magic_spell())
    {
        (void)party_->cast_exploration_spell(*spell);
        pile_magic_shown_ = true;
    }
    treasure_menu();
}

// Leaving with treasure still there asks first; what is left behind is lost.
void UaSession::leave_treasure()
{
    say_kit({"Treasure is still left here. Go back for it?", {}});
    ask_yes_no(
        [this](bool back)
        {
            if (back)
            {
                treasure_menu();
                return;
            }
            pile_money_ = {};
            pile_items_.clear();
            treasure_open_ = false;
        });
}

} // namespace opengold::ua
