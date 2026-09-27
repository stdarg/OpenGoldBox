// Shared implementation for the game and research demo; each adapter supplies
// its CharacterCreationView declaration and localization function.
#include "training_control.h"
#include "opengold/srd5.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
using namespace godot;

void CharacterCreationView::setup_training_review()
{
    auto *button =
        presentation::add_control<Button>(*get_node<Control>("PartyPanel"), "ReviewTraining", {});
    button->set_text(review_text(N_("Review Training")));
    button->hide();
    button->connect("pressed", callable_mp(this, &CharacterCreationView::open_training_review));
    auto owned = presentation::make_node<Window>();
    owned->set_name("TrainingReview");
    owned->set_title(review_text(N_("Review Training")));
    owned->set_size(Vector2i(700, 700));
    owned->set_min_size(Vector2i(700, 700));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *window = presentation::attach_child(*this, std::move(owned));
    window->connect("close_requested",
                    callable_mp(this, &CharacterCreationView::close_training_review));
    window->connect("window_input",
                    callable_mp(this, &CharacterCreationView::training_review_input));
    auto *note = presentation::add_control<Label>(*window, "Note", Rect2(24, 18, 652, 54));
    note->set_text(review_text(N_(
        "Complete missing training. Existing selections are locked.\nCancel discards your changes.")));
    note->add_theme_font_size_override("font_size", 16);
    presentation::setup_training_controls(*window);
    auto *fixed = window->get_node<RichTextLabel>("TrainingFixed");
    fixed->set_position(Vector2(24, 82));
    fixed->set_size(Vector2(652, 100));
    auto *scroll = window->get_node<ScrollContainer>("Training");
    scroll->set_position(Vector2(24, 192));
    scroll->set_size(Vector2(652, 392));
    auto *error = presentation::add_control<Label>(*window, "Error", Rect2(24, 594, 652, 40));
    error->set("autowrap_mode", 3);
    error->add_theme_font_size_override("font_size", 14);
    auto *cancel = presentation::add_control<Button>(*window, "Cancel", Rect2(280, 644, 150, 40));
    cancel->set_text(review_text(N_("Cancel")));
    cancel->connect("pressed", callable_mp(this, &CharacterCreationView::close_training_review));
    auto *apply = presentation::add_control<Button>(*window, "Apply", Rect2(442, 644, 234, 40));
    apply->set_text(review_text(N_("Apply Training")));
    apply->connect("pressed", callable_mp(this, &CharacterCreationView::apply_training_review));
}

void CharacterCreationView::open_training_review()
{
    if (!campaign_ || campaign_->in_combat() || roster_index_ >= campaign_->state().roster.size())
        return;
    const auto &member = campaign_->state().roster[roster_index_];
    if (member.character.sheet().training.complete)
        return;
    try
    {
        auto editor = std::make_unique<opengold::CharacterCreator>(
            opengold::srd5::character_rules(), member.character, campaign_->rule_module());
        locked_training_ = member.character.training_choices();
        training_member_ = member.id;
        training_review_ = std::move(editor);
        auto *window = get_node<Window>("TrainingReview");
        window->get_node<Label>("Error")->set_text({});
        refresh_training_review();
        window->popup_centered();
        window->get_node<Button>("Cancel")->grab_focus();
    }
    catch (const std::exception &e)
    {
        error_ = review_text(e.what());
        refresh_party();
    }
}

void CharacterCreationView::refresh_training_review()
{
    if (!training_review_)
        return;
    auto *window = get_node<Window>("TrainingReview");
    presentation::refresh_training_controls(
        *window, *training_review_,
        callable_mp(this, &CharacterCreationView::review_training_toggled),
        callable_mp(this, &CharacterCreationView::review_training_selected), review_text,
        locked_training_);
    bool valid = !campaign_->in_combat() && training_review_->training_complete();
    if (valid)
    {
        try
        {
            (void)campaign_->preview_training(training_member_, training_review_->rules(),
                                              training_review_->draft().training);
        }
        catch (const std::exception &e)
        {
            valid = false;
            window->get_node<Label>("Error")->set_text(review_text(e.what()));
        }
    }
    window->get_node<Button>("Apply")->set_disabled(!valid);
}

void CharacterCreationView::review_training_toggled(bool selected, String group, String option)
{
    if (!training_review_ || campaign_->in_combat())
        return;
    const std::string id = group.utf8().get_data(), value = option.utf8().get_data();
    const auto locked = locked_training_.find(id);
    if (locked != locked_training_.end() &&
        std::find(locked->second.begin(), locked->second.end(), value) != locked->second.end())
        return;
    try
    {
        training_review_->training_choice(id, value, selected);
        get_node<Label>("TrainingReview/Error")->set_text({});
    }
    catch (const std::exception &e)
    {
        get_node<Label>("TrainingReview/Error")->set_text(review_text(e.what()));
    }
    refresh_training_review();
}

void CharacterCreationView::review_training_selected(std::int64_t index, String group)
{
    if (!training_review_ || campaign_->in_combat() || index <= 0)
        return;
    const std::string id = group.utf8().get_data();
    const auto locked = locked_training_.find(id);
    if (locked != locked_training_.end() && !locked->second.empty())
        return;
    const auto groups = training_review_->training_options();
    const auto found = std::find_if(groups.begin(), groups.end(),
                                    [&](const auto &g)
                                    {
                                        return g.id == id;
                                    });
    if (found == groups.end() || static_cast<std::size_t>(index) > found->options.size())
        return;
    review_training_toggled(true, group,
                            presentation::training_string(found->options[index - 1].id));
}

void CharacterCreationView::apply_training_review()
{
    if (!training_review_ || campaign_->in_combat() || !training_review_->training_complete())
        return;
    try
    {
        campaign_->complete_training(training_member_, training_review_->rules(),
                                     training_review_->draft().training);
        error_ = String();
        close_training_review();
        refresh_party();
        get_node<Control>("PartyPanel/Roster")->grab_focus();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("TrainingReview/Error")->set_text(review_text(e.what()));
    }
}

void CharacterCreationView::close_training_review()
{
    get_node<Window>("TrainingReview")->hide();
    training_review_.reset();
    locked_training_.clear();
    training_member_ = 0;
    get_node<Button>("PartyPanel/ReviewTraining")->grab_focus();
}

void CharacterCreationView::training_review_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE)
    {
        get_node<Window>("TrainingReview")->set_input_as_handled();
        close_training_review();
    }
}
