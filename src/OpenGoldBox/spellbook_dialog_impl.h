#include "spell_choice_controls.h"
#include <godot_cpp/classes/input_event_key.hpp>
using namespace godot;

void CharacterCreationView::setup_spellbook()
{
    auto *button =
        presentation::add_control<Button>(*get_node<Control>("PartyPanel"), "Spellbook", {});
    button->set_text(review_text(N_("Spellbook")));
    button->hide();
    button->connect("pressed", callable_mp(this, &CharacterCreationView::open_spellbook));
    auto *w = presentation::setup_spell_dialog(
        *this, "SpellbookDialog", callable_mp(this, &CharacterCreationView::close_spellbook),
        callable_mp(this, &CharacterCreationView::apply_spellbook), review_text);
    w->connect("window_input", callable_mp(this, &CharacterCreationView::spellbook_input));
    for (const char *name : {"ReplaceLabel", "WithLabel", "Replace", "With"})
        w->get_node<Control>(name)->hide();
    w->get_node<ScrollContainer>("Choices")->set_size(Vector2(652, 390));
}

void CharacterCreationView::open_spellbook()
{
    if (!campaign_ || campaign_->in_combat() || roster_index_ >= campaign_->state().roster.size())
        return;
    spellbook_member_ = campaign_->state().roster[roster_index_].id;
    spellbook_choice_ = {};
    refresh_spellbook();
    auto *w = get_node<Window>("SpellbookDialog");
    w->popup_centered();
    w->get_node<Button>("Cancel")->grab_focus();
}

void CharacterCreationView::refresh_spellbook()
{
    if (!spellbook_member_)
        return;
    auto *w = get_node<Window>("SpellbookDialog");
    const auto &sheet = campaign_->member(spellbook_member_).character.sheet();
    w->get_node<Label>("Title")->set_text(presentation::training_string(sheet.name));
    presentation::spell_known(*w, campaign_->rule_module().spell_access(sheet), review_text);
    presentation::refresh_spell_groups(
        *w->get_node<VBoxContainer>("Choices/Rows"),
        campaign_->spell_choice_options(spellbook_member_), spellbook_choice_,
        callable_mp(this, &CharacterCreationView::spellbook_toggled), review_text);
    try
    {
        (void)campaign_->preview_spell_choices(spellbook_member_, spellbook_choice_);
        w->get_node<Button>("Apply")->set_disabled(false);
        w->get_node<Label>("Error")->set_text({});
    }
    catch (const std::exception &e)
    {
        w->get_node<Button>("Apply")->set_disabled(true);
        w->get_node<Label>("Error")->set_text(review_text(e.what()));
    }
}

void CharacterCreationView::spellbook_toggled(bool selected, String group, String option)
{
    presentation::toggle_spell(spellbook_choice_, selected, group.utf8().get_data(),
                               option.utf8().get_data());
    refresh_spellbook();
}

void CharacterCreationView::apply_spellbook()
{
    try
    {
        campaign_->choose_spells(spellbook_member_, spellbook_choice_);
        close_spellbook();
        refresh_party();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("SpellbookDialog/Error")->set_text(review_text(e.what()));
    }
}

void CharacterCreationView::close_spellbook()
{
    get_node<Window>("SpellbookDialog")->hide();
    spellbook_member_ = 0;
    spellbook_choice_ = {};
    get_node<Button>("PartyPanel/Spellbook")->grab_focus();
}

void CharacterCreationView::spellbook_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE)
    {
        get_node<Window>("SpellbookDialog")->set_input_as_handled();
        close_spellbook();
    }
}
