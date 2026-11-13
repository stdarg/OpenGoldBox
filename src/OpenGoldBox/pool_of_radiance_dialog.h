#ifndef OPENGOLDBOX_POOL_OF_RADIANCE_DIALOG_H
#define OPENGOLDBOX_POOL_OF_RADIANCE_DIALOG_H
#include "opengold/campaign_party.h"
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <vector>

// Curse's Add Character to Party, From Where: Pool ("Pool adds a character
// whose last adventure was in Pool of Radiance"): the Pool of Radiance saves
// made under the original rules, the player characters of the one chosen, and
// each brought into the campaign's party as its rules take it in
// (CampaignParty::bring_across). The controls come from the creation scene.
class PoolOfRadianceDialog : public godot::Window
{
    GDCLASS(PoolOfRadianceDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;

    void _ready() override;
    // The host's campaign, read at each use because loading a save replaces
    // it, and what follows a member brought in.
    void connect_host(CampaignAccess campaign, std::function<void()> member_added);
    // Lists the Pool of Radiance saves and shows the dialog.
    void open();
    // Reads the chosen save and lists its player characters.
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
    // Turns Add on or off for the chosen character and says why.
    void show_member_state();
    CampaignAccess campaign_;
    std::function<void()> member_added_;
    std::vector<std::filesystem::path> saves_;
    // The chosen save's player characters, and which of them have been
    // brought in since the save was chosen.
    std::vector<opengold::PartyMember> members_;
    std::vector<bool> brought_;
    std::size_t member_index_{};
};
#endif
