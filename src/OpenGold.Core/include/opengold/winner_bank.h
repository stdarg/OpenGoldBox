#ifndef OPENGOLD_WINNER_BANK_H
#define OPENGOLD_WINNER_BANK_H

#include "opengold/campaign_party.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opengold
{

// The winner bank (docs/SAVES.md, The winning party): a game's party as it
// stood when the game was won, saved for the series' next game to bring in.

// Whether `member` of a party at its win is one of its winners: a player
// character in one of the six PC positions, or a permanent NPC, a living
// NPC in one of the two NPC positions (docs/DEVIATIONS.md, Winning NPCs).
[[nodiscard]] bool is_winner(const PartyState &party, const PartyMember &member);
// The party's winners in its positions' order.
[[nodiscard]] std::vector<MemberId> winners(const PartyState &party);

// A member of `earlier`'s saved party, brought into the series' next game's
// `party` as that game takes it in (the party screen's Add from <earlier
// game>; docs/GAMES.md): Curse's Pool of Radiance, Secret's Curse, Pools of
// Darkness's Secret, Treasures' Gateway, Death Knights' Champions, Dark
// Queen's Death Knights and Matrix Cubed's Countdown, each with its item
// conversion read from `sequel_folder` (and Secret's from `earlier_folder`
// too). A player character brings the money
// its game lets it keep: its share of `source_wealth` as the save's
// `player_character` of `player_characters` (Treasures' only when
// `gateway_money_kept`); a permanent NPC joins in an NPC position with none.
struct SequelArrival
{
    GoldBoxGame earlier{};
    std::filesystem::path earlier_folder, sequel_folder;
    Purse source_wealth{};
    std::size_t player_character{}, player_characters{};
    bool gateway_money_kept{};
};

MemberId bring_into_sequel(CampaignParty &party, const PartyMember &member,
                           const SequelArrival &arrival);

// Watches a game being played for its win and says when its winning party
// is to be banked. At the first moment the game is won, the party alone is
// banked, before any Continue or epilogue can change it; once the game is
// idle again, and so can be saved with its world, the party is banked again
// with the world, as long as its winners are still there as they won.
class WinnerCapture
{
  public:
    WinnerCapture() = default;

    // A game loaded already won is not won again by being shown.
    explicit WinnerCapture(bool won_already) noexcept : won_(won_already)
    {
    }

    // The saves of the game as it is now: its party alone, and its party
    // with its world, which only an idle game has.
    struct Saves
    {
        std::function<std::string()> party_only;
        std::function<std::string()> with_world;
    };

    // The save to bank now, if any. `game_won` and `idle` are the game's
    // state now; `party` its party. Encodes at the win and when checking a
    // pending world upgrade; every party byte must match before that upgrade.
    [[nodiscard]] std::optional<std::string> observe(bool game_won, bool idle,
                                                     const PartyState &party, const Saves &saves);

    // Persist each capture through the ordinary durable save writer. A failed
    // write retains the exact encoded payload for the next observation, before
    // a later party can be encoded. Encoder failures leave observation retryable.
    void bank(bool game_won, bool idle, const PartyState &party, const Saves &saves,
              const std::function<void(std::string_view)> &write);

  private:
    bool won_{};
    // Compare every saved party byte, including native records, equipment,
    // spells and money, before replacing the at-win bank with its world.
    std::optional<std::string> waiting_for_world_;
    std::optional<std::string> pending_bank_;
};

} // namespace opengold

#endif
