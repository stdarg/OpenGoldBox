#ifndef OPENGOLDBOX_PORTRAIT_DIALOG_H
#define OPENGOLDBOX_PORTRAIT_DIALOG_H

#include "opengold/campaign_party.h"
#include "portrait_catalog.h"
#include <godot_cpp/classes/window.hpp>
#include <cstddef>
#include <functional>
#include <vector>

// Changes a party member's portrait at any time, from its sheet. A member that
// shows its new portrait picks another new one; otherwise it builds its
// original head-and-body portrait. Each change applies at once.
class PortraitDialog : public godot::Window
{
    GDCLASS(PortraitDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;

    void _ready() override;
    // Shows `member`'s portrait. `portraits` must outlive the dialog's use;
    // `changed` follows every change.
    void open(CampaignAccess campaign, opengold::MemberId member, PortraitCatalog &portraits,
              std::function<void()> changed);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    void connect_controls();
    void show_portrait();
    void step_new(std::int64_t direction);
    void new_selected(std::int64_t index);
    void step_head(std::int64_t direction);
    void step_body(std::int64_t direction);
    void change(const opengold::por::CharacterAppearance &appearance);
    void close();
    [[nodiscard]] const opengold::PartyMember &member() const;

    CampaignAccess campaign_;
    opengold::MemberId member_{};
    PortraitCatalog *portraits_{};
    std::function<void()> changed_;
    // The catalog entries offered: the member's race and gender, else all.
    std::vector<std::size_t> offered_;
};

#endif
