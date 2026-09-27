// Included in both native combat views; only presents rules-provided commands.
void CombatView::begin_nick()
{
    if (get_node<Button>("Nick")->is_disabled())
        return;
    get_node<Window>("NickAttack")->popup_centered();
    get_node<OptionButton>("NickAttack/Choices")->grab_focus();
}

void CombatView::nick_selected(std::int64_t index)
{
    auto *choices = get_node<OptionButton>("NickAttack/Choices");
    get_node<Button>("NickAttack/Target")
        ->set_disabled(index < 0 || index >= choices->get_item_count() ||
                       choices->is_item_disabled(index));
}

void CombatView::cancel_nick()
{
    get_node<Window>("NickAttack")->hide();
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
    auto *choices = get_node<OptionButton>("NickAttack/Choices");
    const auto index = choices->get_selected();
    if (index < 0 || get_node<Button>("NickAttack/Target")->is_disabled())
        return;
    const String key = choices->get_item_metadata(index);
    light_item_ = key.get_slice("#", 1).to_int();
    cancel_nick();
    get_node<Button>("Nick")->release_focus();
    select_mode(key.get_slice("#", 0));
}
