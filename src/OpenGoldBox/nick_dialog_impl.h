// Included in both native combat views; only presents rules-provided commands.
#include "godot_nodes.h"
void CombatView::begin_nick()
{
    if (presentation::required_node<Button>(*this, "Nick").is_disabled())
        return;
    presentation::required_node<Window>(*this, "NickAttack").popup_centered();
    presentation::required_node<OptionButton>(*this, "NickAttack/Choices").grab_focus();
}

void CombatView::nick_selected(std::int64_t index)
{
    auto *choices = &presentation::required_node<OptionButton>(*this, "NickAttack/Choices");
    presentation::required_node<Button>(*this, "NickAttack/Target")
    .set_disabled(index < 0 || index >= choices->get_item_count() ||
                  choices->is_item_disabled(static_cast<std::int32_t>(index)));
}

void CombatView::cancel_nick()
{
    presentation::required_node<Window>(*this, "NickAttack").hide();
}

void CombatView::nick_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::KEY_ESCAPE)
    {
        cancel_nick();
        get_viewport()->set_input_as_handled();
    }
}

void CombatView::confirm_nick()
{
    auto *choices = &presentation::required_node<OptionButton>(*this, "NickAttack/Choices");
    const auto index = choices->get_selected();
    if (index < 0 || presentation::required_node<Button>(*this, "NickAttack/Target").is_disabled())
        return;
    const String key = choices->get_item_metadata(index);
    light_item_ = static_cast<unsigned>(key.get_slice("#", 1).to_int());
    cancel_nick();
    presentation::required_node<Button>(*this, "Nick").release_focus();
    select_mode(key.get_slice("#", 0));
}
