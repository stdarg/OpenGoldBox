#include "town_sheet_dialog.h"
#include "character_sheet_text.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>
using namespace godot;
using presentation::required_node;

void TownSheetDialog::_ready()
{
    required_node<Button>(*this, "MakeLeader")
    .connect("pressed", presentation::guarded(this, &TownSheetDialog::make_leader));
    required_node<Button>(*this, "Close")
    .connect("pressed", presentation::guarded(this, &TownSheetDialog::close));
    connect("close_requested", presentation::guarded(this, &TownSheetDialog::close));
}

void TownSheetDialog::connect_host(CampaignAccess campaign, std::function<void()> leader_changed,
                                   FailureReport report)
{
    if (!campaign || !leader_changed || !report)
        throw std::logic_error("The town sheet needs its campaign, a follow-up and a report");
    campaign_ = std::move(campaign);
    leader_changed_ = std::move(leader_changed);
    report_ = std::move(report);
}

void TownSheetDialog::report_failure(const std::exception &failure)
{
    report_(failure);
}

void TownSheetDialog::fit(Vector2 view_size)
{
    required_node<Node>(*this, "WindowLayout").call("fit", Vector2i(view_size));
}

void TownSheetDialog::show_member(std::int64_t slot)
{
    const auto &campaign = campaign_();
    if (slot < 0 || slot >= 8 || !campaign.state().slots[slot])
        return;
    const auto &m = campaign.member(campaign.state().slots[slot]);
    member_ = m.id;
    required_node<RichTextLabel>(*this, "Text").set_text(presentation::sheet_text(campaign, m));
    required_node<Button>(*this, "MakeLeader").set_disabled(m.id == campaign.leader());
    popup_centered();
}

void TownSheetDialog::make_leader()
{
    if (!member_)
        return;
    campaign_().make_leader(member_);
    required_node<Button>(*this, "MakeLeader").set_disabled(true);
    leader_changed_();
}

void TownSheetDialog::close()
{
    hide();
}
