#ifndef OPENGOLDBOX_TOWN_SHEET_DIALOG_H
#define OPENGOLDBOX_TOWN_SHEET_DIALOG_H
#include "opengold/campaign_party.h"
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <functional>

// A party member's sheet opened from the exploration screen's party list,
// where the member can also be made leader. The window's controls come from
// the creation scene; the campaign view hears when the leader changes, so
// neither depends on the other's internals (Effective C++ Item 31).
class TownSheetDialog : public godot::Window
{
    GDCLASS(TownSheetDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;
    using FailureReport = std::function<void(const std::exception &)>;

    void _ready() override;
    // The host's campaign, read at each use because loading a save replaces
    // it; what follows a change of leader; and where a failed handler is
    // shown. All three are required.
    void connect_host(CampaignAccess campaign, std::function<void()> leader_changed,
                      FailureReport report);
    // Shows the member in a party slot; an empty or invalid slot is ignored.
    void show_member(std::int64_t slot);
    // Sizes the window and its controls to the campaign view.
    void fit(godot::Vector2 view_size);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    CampaignAccess campaign_;
    std::function<void()> leader_changed_;
    FailureReport report_;
    opengold::MemberId member_{};

    void make_leader();
    void close();
};
#endif
