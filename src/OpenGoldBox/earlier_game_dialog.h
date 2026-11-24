#ifndef OPENGOLDBOX_EARLIER_GAME_DIALOG_H
#define OPENGOLDBOX_EARLIER_GAME_DIALOG_H
#include "opengold/campaign_party.h"
#include "opengold/gold_box_game.h"
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

class GameFolderQuestions;

// The party screen's choice of the game before's saves: Curse's Add Character
// to Party, From Where: Pool ("Pool adds a character whose last adventure was
// in Pool of Radiance"), Secret's from Curse, Treasures' characters from
// Gateway to the Savage Frontier, Pools of Darkness's from Secret of the
// Silver Blades, Death Knights' from Champions, Dark Queen's from Death
// Knights and Matrix Cubed's from Countdown to Doomsday. It lists that game's
// winning party first (opengold::WinnerCapture), then its saves made under
// the original rules; the winners of the winning party, its permanent NPCs
// among them, or the player characters of a save; and brings each into the
// campaign's party as its rules take it in (CampaignParty::bring_across).
// The controls come from the creation scene.
class EarlierGameDialog : public godot::Window
{
    GDCLASS(EarlierGameDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;

    void _ready() override;
    // The host's campaign, read at each use because loading a save replaces
    // it, what follows a member brought in, and the host's questions for a
    // game's folder, which outlive this dialog's use of them.
    void connect_host(CampaignAccess campaign, std::function<void()> member_added,
                      GameFolderQuestions &folder_questions);
    // Lists the earlier game's saves and shows the dialog.
    void open();
    // Reads the chosen save and lists its player characters, once the
    // folders it reads from are the player's choice.
    void select_save(std::int64_t index);
    void select_member(std::int64_t index);
    // Brings the chosen character into the party.
    void add();
    void close();
    // Shows a failed handler's error, such as a save that cannot be read;
    // called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    void connect_controls();
    // The first folder the import reads that the player has not chosen, if any.
    [[nodiscard]] std::optional<opengold::GoldBoxGame> folder_to_ask() const;
    void ask_folder(opengold::GoldBoxGame game, std::int64_t save_index);
    void list_members(std::int64_t save_index);
    // Turns Add on or off for the chosen character and says why.
    void show_member_state();
    // The game whose characters come into the campaign's: Pool of Radiance
    // for Curse, Gateway for Treasures, Secret for Pools of Darkness.
    [[nodiscard]] opengold::GoldBoxGame earlier_game() const;
    // Brings the chosen member in as the campaign's game takes it.
    void bring_in(const opengold::PartyMember &member);

    [[nodiscard]] bool chosen_is_npc() const
    {
        return member_index_ >= player_characters_;
    }

    CampaignAccess campaign_;
    std::function<void()> member_added_;
    GameFolderQuestions *folder_questions_{};
    // The winning party's file, when there is one, first.
    std::vector<std::filesystem::path> saves_;
    // The chosen save's members to bring in, its player characters first and
    // then a winning party's permanent NPCs, and which of them have been
    // brought in since the save was chosen.
    std::vector<opengold::PartyMember> members_;
    std::size_t player_characters_{};
    std::vector<bool> brought_;
    std::size_t member_index_{};
    // The chosen save's money, which the characters brought in share.
    opengold::Purse source_wealth_{};
};

// The game whose characters a campaign of `game` brings across on its party
// screen under the original rules, if any.
[[nodiscard]] std::optional<opengold::GoldBoxGame> earlier_game_of(opengold::GoldBoxGame game);
#endif
