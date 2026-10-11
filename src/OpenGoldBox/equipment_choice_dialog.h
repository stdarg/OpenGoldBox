#ifndef OPENGOLDBOX_EQUIPMENT_CHOICE_DIALOG_H
#define OPENGOLDBOX_EQUIPMENT_CHOICE_DIALOG_H
#include "godot_nodes.h"
#include "opengold/campaign_party.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/window.hpp>
#include <cstdint>
#include <exception>
#include <functional>
#include <vector>

// The weapon-hand choice for an item that can be held more than one way. The
// rules module supplies legality and outcomes; this window only selects a
// returned operation and commits it. Its state is its own, and the campaign
// view hears when it closes, so neither depends on the other's internals
// (Effective C++ Item 31).
class EquipmentChoiceDialog : public godot::Window
{
    GDCLASS(EquipmentChoiceDialog, godot::Window)
  public:
    using CampaignAccess = std::function<opengold::CampaignParty &()>;
    using FailureReport = std::function<void(const std::exception &)>;

    // The hidden window, named EquipmentChoice, with all of its controls.
    [[nodiscard]] static presentation::NodeOwner<EquipmentChoiceDialog> create();
    // The host's campaign, read at each use because loading a save replaces
    // it; what follows the window closing, and what follows a choice being
    // equipped; and where a failed handler is shown. All four are required.
    void connect_host(CampaignAccess campaign, std::function<void()> closed,
                      std::function<void()> equipped, FailureReport report);
    // Offers the member's choices for one item; there must be at least one.
    void open(opengold::MemberId member, std::uint64_t item,
              std::vector<opengold::rules::EquipmentChoice> choices);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods()
    {
    }

  private:
    CampaignAccess campaign_;
    std::function<void()> closed_, equipped_;
    FailureReport report_;
    opengold::MemberId member_{};
    std::uint64_t item_{};
    std::vector<opengold::rules::EquipmentChoice> choices_;

    void add_controls();
    void choice_selected(std::int64_t index);
    void apply();
    void close();
    void window_input(const godot::Ref<godot::InputEvent> &event);
};
#endif
