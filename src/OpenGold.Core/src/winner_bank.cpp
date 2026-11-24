#include "opengold/winner_bank.h"

#include "opengold/darkness_import.h"
#include "opengold/dark_queen_import.h"
#include "opengold/death_knights_import.h"
#include "opengold/matrix_import.h"
#include "opengold/original_content.h"
#include "opengold/secret_import.h"
#include "opengold/treasures_import.h"

#include <algorithm>
#include <stdexcept>

namespace opengold
{

bool is_winner(const PartyState &party, const PartyMember &member)
{
    const auto position = std::ranges::find(party.slots, member.id);
    if (!member.id || position == party.slots.end())
    {
        return false;
    }
    const bool npc_position = position - party.slots.begin() >= 6;
    if (member.npc_source.empty())
    {
        return !npc_position;
    }
    return npc_position && !member.vitals.dead;
}

std::vector<MemberId> winners(const PartyState &party)
{
    std::vector<MemberId> found;
    for (const auto id : party.slots)
    {
        const auto member = std::ranges::find(party.roster, id, &PartyMember::id);
        if (id && member != party.roster.end() && is_winner(party, *member))
        {
            found.push_back(id);
        }
    }
    return found;
}

namespace
{

// A player character's even share of the save's platinum, gems and jewelry:
// OpenGoldBox keeps a party's money in one purse, where the original gives
// each character its own (decided 2026-11-15).
CampaignParty::BroughtMoney precious_share(const SequelArrival &arrival)
{
    const auto share = [&](Coin coin)
    {
        return static_cast<std::uint32_t>(arrival.source_wealth[static_cast<std::size_t>(coin)] /
                                          std::max<std::size_t>(arrival.player_characters, 1));
    };
    return {share(Coin::platinum), share(Coin::gems), share(Coin::jewelry)};
}

} // namespace

MemberId bring_into_sequel(CampaignParty &party, const PartyMember &member,
                           const SequelArrival &arrival)
{
    using ConvertedItems = CampaignParty::ConvertedItems;
    const bool npc = !member.npc_source.empty();
    const auto from_game = game_folder(arrival.earlier);
    const auto take = [&](const CampaignParty::ItemConversion &convert,
                          std::optional<CampaignParty::BroughtMoney> money,
                          ConvertedItems converted)
    {
        return npc ? party.bring_npc_across(member, from_game, convert, converted)
                   : party.bring_across(member, convert, money, converted);
    };
    // A share of every coin of the save, as the Krynn games and Secret's
    // Curse take it in.
    const auto all_money = [&]() -> std::optional<CampaignParty::BroughtMoney>
    {
        if (npc)
        {
            return std::nullopt;
        }
        return champions_money_for_death_knights(arrival.source_wealth, arrival.player_character,
                                                 arrival.player_characters);
    };
    switch (arrival.earlier)
    {
    case GoldBoxGame::pool_of_radiance:
        return take({}, std::nullopt, ConvertedItems::unreadied);
    case GoldBoxGame::curse_of_the_azure_bonds:
        return take(curse_items_for_secret(arrival.earlier_folder, arrival.sequel_folder),
                    all_money(), ConvertedItems::readied_as_they_were);
    case GoldBoxGame::secret_of_the_silver_blades:
        // Pools of Darkness's LoadChar keeps a Secret character's own
        // platinum, gems and jewelry, and its spells.
        return take(secret_items_for_darkness(arrival.sequel_folder), precious_share(arrival),
                    ConvertedItems::readied_as_they_were);
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return take(gateway_items_for_treasures(arrival.sequel_folder),
                    arrival.gateway_money_kept ? std::optional(precious_share(arrival))
                                               : std::nullopt,
                    ConvertedItems::unreadied);
    case GoldBoxGame::champions_of_krynn:
        return take(champions_items_for_death_knights(arrival.sequel_folder), all_money(),
                    ConvertedItems::readied_as_they_were);
    case GoldBoxGame::death_knights_of_krynn:
        return take(death_knights_items_for_dark_queen(arrival.sequel_folder), all_money(),
                    ConvertedItems::readied_as_they_were);
    case GoldBoxGame::countdown_to_doomsday:
        return bring_countdown_member_into_matrix(
            party, member, load_buck_rogers_stock(arrival.sequel_folder, GoldBoxGame::matrix_cubed),
            from_game);
    default:
        throw std::invalid_argument("No game takes in this game's characters");
    }
}

std::optional<std::string> WinnerCapture::observe(bool game_won, bool idle, const PartyState &,
                                                  const Saves &saves)
{
    const bool newly_won = game_won && !won_;
    if (newly_won)
    {
        // A game won while idle (Matrix Cubed's) is saved with its world at once.
        if (idle)
        {
            auto bank = saves.with_world();
            waiting_for_world_.reset();
            won_ = game_won;
            return bank;
        }
        auto bank = saves.party_only();
        waiting_for_world_ = bank;
        won_ = game_won;
        return bank;
    }
    if (!waiting_for_world_ || !idle)
    {
        won_ = game_won;
        return std::nullopt;
    }
    // Complete encodings, rather than a subset of visible stats, protect
    // same-count item changes and the rules' opaque per-character resources.
    std::optional<std::string> bank;
    if (saves.party_only() == *waiting_for_world_)
    {
        bank = saves.with_world();
    }
    waiting_for_world_.reset();
    won_ = game_won;
    // Winners changed after the win (an epilogue's departure) leave the
    // party alone, banked at the win, as the bank.
    return bank;
}

void WinnerCapture::bank(bool game_won, bool idle, const PartyState &party, const Saves &saves,
                         const std::function<void(std::string_view)> &write)
{
    if (pending_bank_)
    {
        write(*pending_bank_);
        pending_bank_.reset();
    }
    pending_bank_ = observe(game_won, idle, party, saves);
    if (pending_bank_)
    {
        write(*pending_bank_);
        pending_bank_.reset();
    }
}

} // namespace opengold
