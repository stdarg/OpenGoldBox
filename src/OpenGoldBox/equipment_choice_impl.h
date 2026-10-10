#ifndef OPENGOLDBOX_EQUIPMENT_CHOICE_IMPL_H
#define OPENGOLDBOX_EQUIPMENT_CHOICE_IMPL_H

// Shared approved hand-choice dialog. The rules module supplies legality and
// outcomes; this adapter only selects a returned operation and commits it.
// One implementation for the game and demo; the host supplies review_text().
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <string_view>

namespace presentation
{
// Fills a rules message's arguments into its source text, through the host's
// review_text for the parts that are translated.
inline godot::String equipment_message(const opengold::rules::Message &message,
                                       godot::String(*review_text)(std::string_view))
{
    auto text = review_text(message.source);
    for (const auto &argument : message.arguments)
        text = text.replace(godot::String::utf8(("{" + argument.name + "}").c_str()),
                            argument.translate ? review_text(argument.value)
                            : godot::String::utf8(argument.value.c_str()));
    return text;
}
} // namespace presentation

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
        auto owned = presentation::make_node<godot::Window>();
        owned->set_name("EquipmentChoice");
        owned->set_title(review_text(N_("Choose weapon hand")));
        owned->set_size(godot::Vector2i(660, 340));
        owned->set_min_size(godot::Vector2i(660, 340));
        owned->set_flag(godot::Window::FLAG_RESIZE_DISABLED, true);
        owned->set_transient(true);
        owned->set_exclusive(true);
        owned->hide();
        window = presentation::attach_child(*this, std::move(owned));
        window->connect("close_requested",
                        presentation::guarded(this, &CharacterCreationView::close_equipment_choice));
        window->connect("window_input",
                        presentation::guarded(this, &CharacterCreationView::equipment_choice_input));
        auto *name = presentation::add_control<godot::Label>(*window, "Item",
            godot::Rect2(24, 20, 612, 48));
        name->set("autowrap_mode", 3);
        auto *label = presentation::add_control<godot::Label>(*window, "HandLabel",
            godot::Rect2(24, 80, 160, 38));
        label->set_text(review_text(N_("Weapon hand")));
        auto *selection = presentation::add_control<godot::OptionButton>(*window, "Hand",
            godot::Rect2(190, 80, 446, 38));
        selection->connect("item_selected",
                           presentation::guarded(this, &CharacterCreationView::equipment_choice_selected));
        auto *note = presentation::add_control<godot::Label>(*window, "Explanation",
            godot::Rect2(24, 138, 612, 116));
        note->set("autowrap_mode", 3);
        auto *cancel = presentation::add_control<godot::Button>(*window, "Cancel",
            godot::Rect2(316, 278, 150, 40));
        cancel->set_text(review_text(N_("Cancel")));
        cancel->connect("pressed",
                        presentation::guarded(this, &CharacterCreationView::close_equipment_choice));
        auto *apply = presentation::add_control<godot::Button>(*window, "Equip",
            godot::Rect2(478, 278, 158, 40));
        apply->set_text(review_text(N_("Equip")));
        apply->connect("pressed",
                       presentation::guarded(this, &CharacterCreationView::apply_equipment_choice));
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
        hand->add_item(presentation::equipment_message(choices[i].label, review_text));
        hand->set_item_disabled(i, !choices[i].available);
        hand->set_item_tooltip(
            i, presentation::equipment_message(choices[i].explanation, review_text));
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
    .set_text(presentation::equipment_message(choice.explanation, review_text));
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
#endif
