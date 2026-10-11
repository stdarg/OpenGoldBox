#include "character_creation_view.h"

// The demo's own copy of the hand-choice dialog. The game replaced the header
// they shared with its EquipmentChoiceDialog class, which needs the game's
// localization; the demo keeps the dialog as view members instead.
#include "../../../src/OpenGoldBox/godot_nodes.h"
#include "../../../src/OpenGoldBox/guarded_handlers.h"
#include "../../../src/OpenGoldBox/localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <string_view>

using presentation::guarded;

namespace
{
godot::String review_text(std::string_view value)
{
    return godot::String::utf8(value.data(), value.size());
}

// Fills a rules message's arguments into its source text.
godot::String equipment_message(const opengold::rules::Message &message)
{
    auto text = review_text(message.source);
    for (const auto &argument : message.arguments)
        text = text.replace(godot::String::utf8(("{" + argument.name + "}").c_str()),
                            argument.translate ? review_text(argument.value)
                            : godot::String::utf8(argument.value.c_str()));
    return text;
}
} // namespace

void CharacterCreationView::report_failure(const std::exception &failure)
{
    error_ = review_text(failure.what());
    presentation::required_node<godot::Label>(*this, "Status").set_text(error_);
}

bool CharacterCreationView::open_equipment_choice(opengold::MemberId member, std::uint64_t item)
{
    if (campaign_->in_combat())
        throw std::runtime_error("Equipment cannot change during combat");
    const auto choices = campaign_->equipment_choices(member, item);
    if (choices.empty())
        return false;
    auto *window = godot::Object::cast_to<godot::Window>(get_node_or_null("EquipmentChoice"));
    if (!window)
    {
        auto owned = presentation::instantiate_scene("res://scenes/equipment_choice.tscn");
        window = godot::Object::cast_to<godot::Window>(owned.get());
        if (!window)
            throw std::runtime_error("Invalid equipment choice scene");
        presentation::attach_child(*this, std::move(owned));
        window->connect("close_requested",
                        guarded(this, &CharacterCreationView::close_equipment_choice));
        window->connect("window_input",
                        guarded(this, &CharacterCreationView::equipment_choice_input));
        auto *selection = &presentation::required_node<godot::OptionButton>(*window, "Hand");
        selection->connect("item_selected",
                           guarded(this, &CharacterCreationView::equipment_choice_selected));
        auto *cancel = &presentation::required_node<godot::Button>(*window, "Cancel");
        cancel->connect("pressed",
                        guarded(this, &CharacterCreationView::close_equipment_choice));
        auto *apply = &presentation::required_node<godot::Button>(*window, "Equip");
        apply->connect("pressed",
                       guarded(this, &CharacterCreationView::apply_equipment_choice));
    }
    equipment_member_ = member;
    equipment_item_ = item;
    equipment_choices_ = choices;
    presentation::required_node<godot::Label>(*window, "Item").set_text(
        review_text(campaign_->member(member).character.inventory().find(item)->get().name));
    auto *hand = &presentation::required_node<godot::OptionButton>(*window, "Hand");
    hand->clear();
    int first = -1;
    for (unsigned i = 0; i < choices.size(); ++i)
    {
        hand->add_item(equipment_message(choices[i].label));
        hand->set_item_disabled(i, !choices[i].available);
        hand->set_item_tooltip(
            i, equipment_message(choices[i].explanation));
        if (first < 0 && choices[i].available)
            first = i;
    }
    hand->select(first < 0 ? 0 : first);
    equipment_choice_selected(first < 0 ? 0 : first);
    window->popup_centered();
    hand->grab_focus();
    return true;
}

void CharacterCreationView::equipment_choice_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= equipment_choices_.size())
        return;
    const auto &choice = equipment_choices_[index];
    auto *window = &presentation::required_node<godot::Window>(*this, "EquipmentChoice");
    presentation::required_node<godot::Label>(*window, "Explanation")
    .set_text(equipment_message(choice.explanation));
    presentation::required_node<godot::Button>(*window, "Equip").set_disabled(!choice.available
            || campaign_->in_combat());
}

void CharacterCreationView::apply_equipment_choice()
{
    auto *window = &presentation::required_node<godot::Window>(*this, "EquipmentChoice");
    const auto selected =
        presentation::required_node<godot::OptionButton>(*window, "Hand").get_selected();
    if (selected < 0 || static_cast<std::size_t>(selected) >= equipment_choices_.size() ||
            !equipment_choices_[selected].available)
        return;
    try
    {
        campaign_->equip(equipment_member_, equipment_item_,
                         equipment_choices_[selected].operation);
        error_ = godot::String();
        close_equipment_choice();
        refresh_party();
    }
    catch (const std::exception &e)
    {
        presentation::required_node<godot::Label>(*window, "Explanation")
        .set_text(review_text(e.what()));
    }
}

void CharacterCreationView::close_equipment_choice()
{
    presentation::required_node<godot::Window>(*this, "EquipmentChoice").hide();
    equipment_choices_.clear();
    equipment_member_ = 0;
    equipment_item_ = 0;
    presentation::required_node<godot::Button>(*this, "PartyPanel/Equip").grab_focus();
}

void CharacterCreationView::equipment_choice_input(const godot::Ref<godot::InputEvent> &event)
{
    const godot::Ref<godot::InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == godot::Key::KEY_ESCAPE)
    {
        close_equipment_choice();
        presentation::required_node<godot::Window>(*this, "EquipmentChoice").set_input_as_handled();
    }
}
