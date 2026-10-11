#include "equipment_choice_dialog.h"
#include "guarded_handlers.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <stdexcept>
#include <string>
#include <utility>

using namespace godot;
using presentation::required_node;

namespace
{
// Fills a rules message's arguments into its translated source text.
String equipment_message(const opengold::rules::Message &message)
{
    auto text = i18n::text(message.source);
    for (const auto &argument : message.arguments)
        text = text.replace(String::utf8(("{" + argument.name + "}").c_str()),
                            argument.translate ? i18n::text(argument.value)
                            : String::utf8(argument.value.c_str()));
    return text;
}
} // namespace

presentation::NodeOwner<EquipmentChoiceDialog> EquipmentChoiceDialog::create()
{
    auto window = presentation::make_node<EquipmentChoiceDialog>();
    window->set_name("EquipmentChoice");
    window->set_title(i18n::text(N_("Choose weapon hand")));
    presentation::attach_dialog_layout(*window);
    window->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    window->set_transient(true);
    window->set_exclusive(true);
    window->hide();
    window->add_controls();
    return window;
}

void EquipmentChoiceDialog::connect_host(CampaignAccess campaign, std::function<void()> closed,
        std::function<void()> equipped, FailureReport report)
{
    if (!campaign || !closed || !equipped || !report)
        throw std::logic_error("Equipment choice needs a campaign, two follow-ups and a report");
    campaign_ = std::move(campaign);
    closed_ = std::move(closed);
    equipped_ = std::move(equipped);
    report_ = std::move(report);
}

void EquipmentChoiceDialog::report_failure(const std::exception &failure)
{
    report_(failure);
}

void EquipmentChoiceDialog::add_controls()
{
    connect("close_requested", presentation::guarded(this, &EquipmentChoiceDialog::close));
    connect("window_input", presentation::guarded(this, &EquipmentChoiceDialog::window_input));
    auto *name = presentation::dialog_control<Label>(*this, "Item");
    name->set("autowrap_mode", 3);
    auto *label = presentation::dialog_control<Label>(*this, "HandLabel");
    label->set_text(i18n::text(N_("Weapon hand")));
    auto *selection = presentation::dialog_control<OptionButton>(*this, "Hand");
    selection->connect("item_selected",
                       presentation::guarded(this, &EquipmentChoiceDialog::choice_selected));
    auto *note = presentation::dialog_control<Label>(*this, "Explanation");
    note->set("autowrap_mode", 3);
    auto *cancel = presentation::dialog_control<Button>(*this, "Cancel");
    cancel->set_text(i18n::text(N_("Cancel")));
    cancel->connect("pressed", presentation::guarded(this, &EquipmentChoiceDialog::close));
    auto *equip = presentation::dialog_control<Button>(*this, "Equip");
    equip->set_text(i18n::text(N_("Equip")));
    equip->connect("pressed", presentation::guarded(this, &EquipmentChoiceDialog::apply));
}

void EquipmentChoiceDialog::open(opengold::MemberId member, std::uint64_t item,
                                 std::vector<opengold::rules::EquipmentChoice> choices)
{
    member_ = member;
    item_ = item;
    choices_ = std::move(choices);
    required_node<Label>(*this, "Item").set_text(
        i18n::text(campaign_().member(member).character.inventory().find(item)->get().name));
    auto *hand = &required_node<OptionButton>(*this, "Hand");
    hand->clear();
    int first = -1;
    for (unsigned i = 0; i < choices_.size(); ++i)
    {
        hand->add_item(equipment_message(choices_[i].label));
        hand->set_item_disabled(i, !choices_[i].available);
        hand->set_item_tooltip(i, equipment_message(choices_[i].explanation));
        if (first < 0 && choices_[i].available)
            first = i;
    }
    hand->select(first < 0 ? 0 : first);
    choice_selected(first < 0 ? 0 : first);
    popup_centered();
    hand->grab_focus();
}

void EquipmentChoiceDialog::choice_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= choices_.size())
        return;
    const auto &choice = choices_[index];
    required_node<Label>(*this, "Explanation").set_text(equipment_message(choice.explanation));
    required_node<Button>(*this, "Equip").set_disabled(!choice.available ||
            campaign_().in_combat());
}

void EquipmentChoiceDialog::apply()
{
    const auto selected = required_node<OptionButton>(*this, "Hand").get_selected();
    if (selected < 0 || static_cast<std::size_t>(selected) >= choices_.size() ||
            !choices_[selected].available)
        return;
    try
    {
        campaign_().equip(member_, item_, choices_[selected].operation);
        close();
        equipped_();
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "Explanation").set_text(i18n::text(e.what()));
    }
}

void EquipmentChoiceDialog::close()
{
    hide();
    choices_.clear();
    member_ = 0;
    item_ = 0;
    closed_();
}

void EquipmentChoiceDialog::window_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == Key::KEY_ESCAPE)
    {
        close();
        set_input_as_handled();
    }
}
