#include "town_sheet_dialog.h"
#include "application_settings.h"
#include "character_sheet_text.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "portrait_dialog.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>
using namespace godot;
using presentation::required_node;

void TownSheetDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void TownSheetDialog::connect_controls()
{
    required_node<Button>(*this, "MakeLeader")
        .connect("pressed", presentation::guarded(this, &TownSheetDialog::make_leader));
    required_node<Button>(*this, "ChangePortrait")
        .connect("pressed", presentation::guarded(this, &TownSheetDialog::change_portrait));
    presentation::attach_child(*this, presentation::instantiate_control<PortraitDialog>(
                                          "res://scenes/portrait_dialog.tscn"));
    required_node<Button>(*this, "Close")
        .connect("pressed", presentation::guarded(this, &TownSheetDialog::close));
    // In the order of opengold::GearPreference.
    auto &preference = required_node<OptionButton>(*this, "GearPreference");
    preference.add_item(i18n::text(N_("Maximize defense")));
    preference.add_item(i18n::text(N_("Maximize damage")));
    preference.connect("item_selected",
                       presentation::guarded(this, &TownSheetDialog::gear_preference_selected));
    connect("close_requested", presentation::guarded(this, &TownSheetDialog::close));
}

void TownSheetDialog::connect_host(CampaignAccess campaign, std::function<void()> party_changed,
                                   FailureReport report)
{
    if (!campaign || !party_changed || !report)
    {
        throw std::logic_error("The town sheet needs its campaign, a follow-up and a report");
    }
    campaign_ = std::move(campaign);
    party_changed_ = std::move(party_changed);
    report_ = std::move(report);
}

void TownSheetDialog::forget_portraits()
{
    portraits_.reset();
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
    {
        return;
    }
    member_ = campaign.state().slots[slot];
    show_sheet();
    popup_centered();
}

void TownSheetDialog::show_sheet()
{
    const auto &campaign = campaign_();
    const auto &m = campaign.member(member_);
    if (!portraits_)
    {
        portraits_ = PortraitCatalog::load();
    }
    const auto sections = presentation::sheet_sections(campaign, m);
    required_node<RichTextLabel>(*this, "Text")
        .call("render_with_portrait",
              portraits_->texture(m.character.appearance(), m.character.creation_data()),
              sections.beside_portrait, sections.below_portrait);
    required_node<Button>(*this, "MakeLeader").set_disabled(m.id == campaign.leader());
    const bool auto_equip = settings::auto_equip();
    required_node<Label>(*this, "GearPreferenceLabel").set_visible(auto_equip);
    auto &preference = required_node<OptionButton>(*this, "GearPreference");
    preference.set_visible(auto_equip);
    preference.select(static_cast<int>(m.gear_preference));
}

void TownSheetDialog::make_leader()
{
    if (!member_)
    {
        return;
    }
    campaign_().make_leader(member_);
    required_node<Button>(*this, "MakeLeader").set_disabled(true);
    party_changed_();
}

// Opens the member's portrait to change; each change redraws the sheet and
// the party.
void TownSheetDialog::change_portrait()
{
    if (!member_ || !portraits_)
    {
        return;
    }
    required_node<PortraitDialog>(*this, "PortraitDialog")
        .open(campaign_, member_, *portraits_,
              [this]
              {
                  show_sheet();
                  party_changed_();
              });
}

void TownSheetDialog::gear_preference_selected(std::int64_t index)
{
    if (member_)
    {
        campaign_().set_gear_preference(member_, static_cast<opengold::GearPreference>(index));
    }
}

void TownSheetDialog::close()
{
    hide();
}
