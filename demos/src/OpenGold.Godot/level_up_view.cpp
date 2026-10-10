#include "../../../src/OpenGoldBox/spell_choice_controls.h"
#include "character_creation_view.h"
#include "rolf_tour_view.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "../../../src/OpenGoldBox/godot_path.h"
#include <algorithm>
#include <utility>

using namespace godot;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}

struct DeleteNode
{
    void operator()(Node *n) const
    {
        memdelete(n);
    }
};

template <class T> T *control(Node *parent, const String &name, Rect2 rect)
{
    std::unique_ptr<T, DeleteNode> owned(memnew(T));
    owned->set_name(name);
    owned->set_position(rect.position);
    owned->set_size(rect.size);
    auto *borrowed = owned.get();
    parent->add_child(owned.get());
    owned.release();
    return borrowed;
}
} // namespace

void CharacterCreationView::setup_advancement()
{
    get_node<ItemList>("PartyPanel/Roster")->add_theme_constant_override("v_separation", 8);
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    advancement_check_ = args.has("--advancement-check");
    advancement_review_ = args.has("--level-up-review");
    std::unique_ptr<Window, DeleteNode> owned(memnew(Window));
    owned->set_name("LevelUp");
    owned->set_title("Level up");
    owned->set_size(Vector2i(700, 670));
    owned->set_min_size(Vector2i(700, 670));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *window = owned.get();
    add_child(owned.get());
    owned.release();
    window->connect("close_requested",
                    callable_mp(this, &CharacterCreationView::close_advancement));
    auto *spell_page = presentation::add_control<ScrollContainer>(*window, "SpellChoicesPage",
        Rect2(24, 70, 652, 475));
    spell_page->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    spell_page->set_follow_focus(true);
    spell_page->hide();
    presentation::spell_rows(*spell_page, "Rows");
    // Skilled reuses the same page area and scrolling behaviour as the spell page.
    auto *skilled_count = presentation::add_control<Label>(*window, "SkilledCount",
        Rect2(24, 44, 652, 24));
    skilled_count->hide();
    auto *skilled_page = presentation::add_control<ScrollContainer>(*window, "SkilledPage",
        Rect2(24, 70, 652, 475));
    skilled_page->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    skilled_page->set_follow_focus(true);
    skilled_page->hide();
    presentation::spell_rows(*skilled_page, "Rows");
    auto *back = presentation::add_control<Button>(*window, "Back", Rect2(236, 610, 136, 40));
    back->set_text(gs("Back"));
    back->hide();
    back->connect("pressed", callable_mp(this, &CharacterCreationView::advancement_back));
    auto *title = control<Label>(window, "Title", Rect2(24, 20, 652, 40));
    title->add_theme_font_size_override("font_size", 24);
    title->set_clip_text(true);
    control<Label>(window, "HP", Rect2(24, 70, 652, 42));
    control<Label>(window, "FeatLabel", Rect2(24, 122, 652, 28))
    ->set_text("Feat or ability points");
    auto *feat = control<OptionButton>(window, "Feat", Rect2(24, 155, 652, 38));
    feat->connect("item_selected", callable_mp(this, &CharacterCreationView::advancement_changed));
    auto *training_label =
        control<Label>(window, "AdvancementTrainingLabel", Rect2(24, 205, 652, 25));
    training_label->hide();
    auto *training = control<OptionButton>(window, "AdvancementTraining", Rect2(24, 236, 652, 36));
    training->hide();
    training->connect("item_selected",
                      callable_mp(this, &CharacterCreationView::advancement_changed));
    const std::array<const char *, 6> abilities{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    for (unsigned i = 0; i < 6; ++i)
    {
        control<Label>(window, String("AbilityLabel") + String::num_uint64(i),
                       Rect2(24 + i * 110, 205, 100, 25))
        ->set_text(abilities[i]);
        auto *points = control<OptionButton>(window, String("Ability") + String::num_uint64(i),
                                             Rect2(24 + i * 110, 236, 100, 36));
        for (int n = 0; n <= 2; ++n)
            points->add_item(String("+") + String::num_int64(n));
        points->connect("item_selected",
                        callable_mp(this, &CharacterCreationView::advancement_changed));
    }
    control<Label>(window, "SpellLabel", Rect2(24, 292, 652, 28))->set_text("Prepared spells");
    auto *style = control<OptionButton>(window, "FightingStyle", Rect2(24, 325, 652, 38));
    style->hide();
    style->connect("item_selected", callable_mp(this, &CharacterCreationView::advancement_changed));
    for (int i = 0; i < 4; ++i)
    {
        auto *spell = control<CheckBox>(window, String("Spell") + String::num_int64(i),
                                        Rect2(24, 325 + i * 38, 652, 36));
        spell->connect(
            "toggled",
            callable_mp(this, &CharacterCreationView::advancement_spell_changed).bind(i));
    }
    auto *note = control<Label>(window, "Note", Rect2(24, 489, 652, 66));
    note->set_text(
        "Fixed-average HP growth. Existing resource expenditure is preserved.\nAdditional class and subclass features are unavailable in this version.");
    note->add_theme_font_size_override("font_size", 14);
    note->set("autowrap_mode", 3);
    auto *error = control<Label>(window, "Error", Rect2(24, 560, 652, 34));
    error->add_theme_font_size_override("font_size", 15);
    auto *cancel = control<Button>(window, "Cancel", Rect2(386, 610, 136, 40));
    cancel->set_text("Cancel");
    cancel->connect("pressed", callable_mp(this, &CharacterCreationView::close_advancement));
    auto *confirm = control<Button>(window, "Confirm", Rect2(536, 610, 140, 40));
    confirm->set_text("Confirm");
    confirm->connect("pressed", callable_mp(this, &CharacterCreationView::confirm_advancement));
}

const opengold::rules::TrainingChoiceGroup *
CharacterCreationView::advancement_skilled_group() const
{
    const auto found = std::find_if(advancement_options_.training.begin(),
                                    advancement_options_.training.end(),
                                    [](const auto & g)
    {
        return g.id == "feat:skilled";
    });
    return found == advancement_options_.training.end() ? nullptr : &*found;
}

const opengold::rules::TrainingChoiceGroup *
CharacterCreationView::advancement_dropdown_group() const
{
    const auto found = std::find_if(advancement_options_.training.begin(),
                                    advancement_options_.training.end(),
                                    [](const auto & g)
    {
        return g.id != "feat:skilled";
    });
    return found == advancement_options_.training.end() ? nullptr : &*found;
}

CharacterCreationView::AdvancementPage
CharacterCreationView::advancement_next_page(AdvancementPage from) const
{
    if (from == AdvancementPage::choices && advancement_skilled_group())
        return AdvancementPage::skilled;
    if (from != AdvancementPage::spells && advancement_choice_.spell_learning)
        return AdvancementPage::spells;
    return from;
}

CharacterCreationView::AdvancementPage
CharacterCreationView::advancement_previous_page(AdvancementPage from) const
{
    if (from == AdvancementPage::spells && advancement_skilled_group())
        return AdvancementPage::skilled;
    return AdvancementPage::choices;
}

void CharacterCreationView::advancement_pages()
{
    if (!advancing_)
        return;
    auto *w = get_node<Window>("LevelUp");
    const bool wizard = advancement_choice_.spell_learning.has_value();
    const auto *skilled = advancement_skilled_group();
    const bool on_first = advancement_page_ == AdvancementPage::choices;
    w->get_node<Control>("SpellChoicesPage")
    ->set_visible(wizard && advancement_page_ == AdvancementPage::spells);
    w->get_node<Control>("SkilledPage")
    ->set_visible(skilled && advancement_page_ == AdvancementPage::skilled);
    w->get_node<Control>("SkilledCount")
    ->set_visible(skilled && advancement_page_ == AdvancementPage::skilled);
    // Back appears on any page after the first rather than only a Wizard's
    // spell page, so a non-Wizard taking Skilled can still step back.
    w->get_node<Button>("Back")->set_visible(!on_first);
    w->get_node<Button>("Confirm")
    ->set_text(advancement_next_page(advancement_page_) == advancement_page_ ? gs("Confirm")
               : gs("Next"));
    for (const char *name :
            {"HP", "FeatLabel", "Feat", "Note"
            })
        w->get_node<Control>(name)->set_visible(on_first);
    // Match open_advancement exactly: a dropdown alongside a feat choice is
    // supplemental, and then the ability controls stay visible beside it.
    const bool has_training = advancement_dropdown_group() != nullptr;
    const bool supplemental = has_training && !advancement_options_.feats.empty();
    for (const char *name :
            {"AdvancementTrainingLabel", "AdvancementTraining"
            })
        w->get_node<Control>(name)->set_visible(on_first && has_training);
    for (unsigned i = 0; i < 6; ++i)
    {
        w->get_node<Control>(String("Ability") + String::num_uint64(i))
        ->set_visible(on_first && (!has_training || supplemental));
        w->get_node<Control>(String("AbilityLabel") + String::num_uint64(i))
        ->set_visible(on_first && (!has_training || supplemental));
    }
    if (advancement_page_ == AdvancementPage::skilled)
        refresh_advancement_skilled();
    if (!wizard)
    {
        w->get_node<Control>("SpellLabel")->set_visible(on_first);
        for (unsigned i = 0; i < 4; ++i)
            w->get_node<Control>(String("Spell") + String::num_uint64(i))
            ->set_visible(on_first && i < advancement_options_.spells.size());
        return;
    }
    w->get_node<Control>("SpellLabel")->hide();
    for (unsigned i = 0; i < 4; ++i)
        w->get_node<Control>(String("Spell") + String::num_uint64(i))->hide();
    if (advancement_page_ != AdvancementPage::spells)
        return;
    auto sheet = campaign_->member(advancing_).character.sheet();
    sheet.level = advancement_options_.level;
    auto options = campaign_->rule_module().spell_choice_options(
                       sheet, opengold::rules::SpellChoiceContext::advancement);
    opengold::rules::SpellChoices choices
    {
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    auto learning = choices;
    learning.prepared.reset();
    try
    {
        campaign_->rule_module().apply_spell_choices(
            sheet, learning, opengold::rules::SpellChoiceContext::advancement,
            opengold::rules::ChoiceCompleteness::partial);
        options.preparation =
            campaign_->rule_module()
            .spell_choice_options(sheet, opengold::rules::SpellChoiceContext::advancement)
            .preparation;
    }
    catch (const std::exception &)
    {
    } // Invalid edits remain visible; final preview reports the error.
    presentation::refresh_spell_groups(
        *w->get_node<VBoxContainer>("SpellChoicesPage/Rows"), options, choices,
        callable_mp(this, &CharacterCreationView::advancement_learning_toggled),
        [](std::string_view source)
    {
        return gs(source);
    });
}

// One checkbox per offered proficiency, prefixed so the two catalogs stay
// distinguishable in a single list. Controls are reused across refreshes so
// toggling never destroys the focused checkbox.
void CharacterCreationView::refresh_advancement_skilled()
{
    const auto *group = advancement_skilled_group();
    if (!group)
        return;
    auto *w = get_node<Window>("LevelUp");
    auto &rows = *w->get_node<VBoxContainer>("SkilledPage/Rows");
    const auto &picked = advancement_choice_.training["feat:skilled"];
    w->get_node<Label>("SkilledCount")
    ->set_text(gs("Selected: ") + String::num_uint64(picked.size()) + gs("/") +
               String::num_uint64(group->count));
    // Hide every row first; the loop below shows only what is still offered, so
    // no reverse name mapping is needed (ids contain underscores of their own).
    for (int n = 0; n < rows.get_child_count(); ++n)
        if (auto *box = Object::cast_to<CheckBox>(rows.get_child(n)))
            box->hide();
    for (const auto &option : group->options)
    {
        const auto name = gs(option.id).replace(":", "_");
        auto *box = Object::cast_to<CheckBox>(rows.get_node_or_null(name));
        if (!box)
        {
            auto owned = presentation::make_node<CheckBox>();
            owned->set_name(name);
            box = owned.get();
            box->connect("toggled", callable_mp(this,
                                                &CharacterCreationView::advancement_skilled_toggled)
                         .bind(gs(option.id)));
            presentation::attach_child(rows, std::move(owned));
        }
        box->set_text(gs(option.id.starts_with("skill:") ? "Skill: " : "Tool: ") +
                      gs(option.label));
        const bool on = std::find(picked.begin(), picked.end(), option.id) != picked.end();
        box->set_pressed_no_signal(on);
        // A full selection leaves only the chosen three toggleable, so the
        // count cannot be exceeded by clicking.
        box->set_disabled(!on && picked.size() >= group->count);
        box->show();
    }
}

void CharacterCreationView::advancement_skilled_toggled(bool selected, String option)
{
    const std::string id = option.utf8().get_data();
    auto &picked = advancement_choice_.training["feat:skilled"];
    const auto *group = advancement_skilled_group();
    if (selected)
    {
        if (std::find(picked.begin(), picked.end(), id) == picked.end() &&
                (!group || picked.size() < group->count))
            picked.push_back(id);
    }
    else
        std::erase(picked, id);
    refresh_advancement_skilled();
    advancement_changed();
}

void CharacterCreationView::advancement_back()
{
    advancement_page_ = advancement_previous_page(advancement_page_);
    advancement_pages();
    advancement_changed();
}

void CharacterCreationView::advancement_learning_toggled(bool selected, String group, String option)
{
    opengold::rules::SpellChoices choices
    {
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    presentation::toggle_spell(choices, {.group = group.utf8().get_data(),
                                         .option = option.utf8().get_data(),
                                         .selected = selected
                                        });
    advancement_choice_.spell_learning = choices.learning;
    advancement_choice_.spells = *choices.prepared;
    if (group != "prepared" && !selected)
    {
        const std::string id = option.utf8().get_data();
        std::erase(advancement_choice_.spells, id);
    }
    advancement_pages();
    advancement_changed();
}

void CharacterCreationView::refresh_advancement_arrows()
{
    auto *list = get_node<ItemList>("PartyPanel/Roster");
    if (!list->is_visible_in_tree())
        return;
    list->force_update_list_size();
    const auto &roster = campaign_->state().roster;
    for (std::size_t i = 0; i < roster.size(); ++i)
    {
        const auto &member = roster[i];
        const auto name = String("Advance") + String::num_uint64(member.id);
        auto *arrow = Object::cast_to<Button>(list->get_node_or_null(name));
        if (!arrow)
        {
            arrow = control<Button>(list, name, Rect2(0, 0, 30, 26));
            arrow->set_text(String::utf8("↑"));
            arrow->set_tooltip_text("Level up " + gs(member.character.sheet().name));
            arrow->add_theme_font_size_override("font_size", 14);
            arrow->connect(
                "pressed",
                callable_mp(this, &CharacterCreationView::open_advancement).bind(member.id));
        }
        arrow->set_tooltip_text("Level up " + gs(member.character.sheet().name));
        const auto rect = list->get_item_rect(static_cast<std::int32_t>(i));
        const float y = rect.position.y - list->get_v_scroll_bar()->get_value();
        const auto font = list->get_theme_font("font");
        const float width = Vector2(font->call("get_string_size", gs(member.character.sheet().name),
                                               0, -1, list->get_theme_font_size("font_size")))
                            .x;
        arrow->set_position(Vector2(std::min(width + 16, list->get_size().x - 52), y));
        arrow->set_visible(campaign_->can_advance(member.id) && y >= 0 &&
                           y + 26 <= list->get_size().y);
    }
    for (int n = 0; n < list->get_child_count(); ++n)
        if (auto *arrow = Object::cast_to<Button>(list->get_child(n));
                arrow && String(arrow->get_name()).begins_with("Advance"))
        {
            const auto id = String(arrow->get_name()).substr(7).to_int();
            if (std::none_of(roster.begin(), roster.end(),
                             [&](const auto & m)
        {
            return m.id == id;
        }))
            arrow->hide();
        }
}

void CharacterCreationView::open_advancement(std::int64_t member)
{
    const auto id = static_cast<opengold::MemberId>(member);
    if (campaign_defeated_ || !campaign_->can_advance(id))
        return;
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
            town && town->is_visible() && !town->can_leave())
        return;
    advancement_page_ = AdvancementPage::choices;
    advancing_ = id;
    advancement_options_ = campaign_->advancement_options(id);
    advancement_choice_ = campaign_->default_advancement(id);
    advancement_refreshing_ = true;
    auto *window = get_node<Window>("LevelUp");
    for (const char *name :
            {"HP", "FeatLabel", "Feat", "Note", "SpellLabel"
            })
        window->get_node<Control>(name)->show();
    window->get_node<Label>("Title")->set_text(gs(campaign_->member(id).character.sheet().name) +
            " / Level " +
            String::num_uint64(advancement_options_.level));
    window->get_node<Label>("Note")->set_text(gs(advancement_options_.description));
    auto *feat = window->get_node<OptionButton>("Feat");
    feat->clear();
    if (advancement_options_.feats.empty())
        feat->add_item("No feat or ability increase at this level");
    for (unsigned i = 0; i < advancement_options_.feats.size(); ++i)
    {
        const auto &option = advancement_options_.feats[i];
        feat->add_item(gs(option.label) + (option.available ? "" : " (Unavailable)"));
        feat->set_item_disabled(i, !option.available);
        feat->set_item_tooltip(i, gs(option.description));
        if (option.id == advancement_choice_.feat)
            feat->select(i);
    }
    feat->set_disabled(advancement_options_.feats.empty());
    const auto *dropdown = advancement_dropdown_group();
    const bool has_training = dropdown != nullptr;
    const bool supplemental_training = has_training && !advancement_options_.feats.empty();
    auto *training = window->get_node<OptionButton>("AdvancementTraining");
    training->clear();
    training->set_visible(has_training);
    window->get_node<Label>("AdvancementTrainingLabel")->set_visible(has_training);
    window->get_node<Label>("AdvancementTrainingLabel")
    ->set_position(Vector2(24, supplemental_training ? 374 : 205));
    training->set_position(Vector2(24, supplemental_training ? 406 : 236));
    for (unsigned i = 0; i < 6; ++i)
    {
        window->get_node<Control>(String("Ability") + String::num_uint64(i))
        ->set_visible(!has_training || supplemental_training);
        window->get_node<Control>(String("AbilityLabel") + String::num_uint64(i))
        ->set_visible(!has_training || supplemental_training);
    }
    if (has_training)
    {
        const auto &group = *dropdown;
        window->get_node<Label>("AdvancementTrainingLabel")->set_text(gs(group.label));
        training->add_item(gs("Choose an option"));
        for (const auto &option : group.options)
            training->add_item(gs(option.label) + (option.description.empty()
                                                   ? String()
                                                   : String(" / ") + gs(option.description)));
        training->select(0);
        advancement_choice_.training.clear();
    }
    for (unsigned i = 0; i < 6; ++i)
        window->get_node<OptionButton>(String("Ability") + String::num_uint64(i))
        ->select(advancement_choice_.abilities[opengold::rules::all_abilities[i]]);
    window->get_node<Label>("SpellLabel")
    ->set_text(advancement_options_.spells.empty() ? "No spell choices for this class"
               : "Prepared spells: select at least one");
    for (unsigned i = 0; i < 4; ++i)
    {
        auto *spell = window->get_node<CheckBox>(String("Spell") + String::num_uint64(i));
        spell->set_visible(i < advancement_options_.spells.size());
        if (i >= advancement_options_.spells.size())
            continue;
        const auto &option = advancement_options_.spells[i];
        spell->set_text(gs(option.label) + (option.available ? "" : " (Unavailable)"));
        spell->set_tooltip_text(gs(option.description));
        spell->set_disabled(!option.available);
        spell->set_pressed_no_signal(std::find(advancement_choice_.spells.begin(),
                                               advancement_choice_.spells.end(),
                                               option.id) != advancement_choice_.spells.end());
    }
    auto *style = window->get_node<OptionButton>("FightingStyle");
    style->clear();
    style->set_visible(!advancement_options_.fighting_styles.empty());
    if (!advancement_options_.fighting_styles.empty())
    {
        window->get_node<Label>("SpellLabel")->set_text("Fighting Style");
        for (const auto &option : advancement_options_.fighting_styles)
        {
            const int i = style->get_item_count();
            style->add_item(gs(option.label));
            style->set_item_disabled(i, !option.available);
            style->set_item_tooltip(i, gs(option.description));
            if ((advancement_choice_.fighting_style &&
                    option.id == *advancement_choice_.fighting_style) ||
                    (!advancement_choice_.fighting_style && option.id == "keep"))
                style->select(i);
        }
    }
    advancement_refreshing_ = false;
    advancement_pages();
    advancement_changed();
    window->popup_centered();
    window->get_node<Button>("Cancel")->grab_focus();
}

void CharacterCreationView::advancement_spell_changed(bool, int)
{
    advancement_changed();
}

void CharacterCreationView::advancement_changed(std::int64_t)
{
    if (advancement_refreshing_ || !advancing_)
        return;
    auto *window = get_node<Window>("LevelUp");
    if (!advancement_options_.fighting_styles.empty())
    {
        const auto index = window->get_node<OptionButton>("FightingStyle")->get_selected();
        if (index >= 0)
        {
            const auto &option = advancement_options_.fighting_styles.at(index);
            if (option.id == "keep")
                advancement_choice_.fighting_style.reset();
            else
                advancement_choice_.fighting_style = option.id;
        }
        advancement_options_ = campaign_->advancement_options(advancing_, advancement_choice_);
        auto *feats = window->get_node<OptionButton>("Feat");
        for (unsigned i = 0; i < advancement_options_.feats.size(); ++i)
        {
            const auto &option = advancement_options_.feats[i];
            feats->set_item_disabled(i, !option.available);
            feats->set_item_text(i, gs(option.label) +
                                 (option.available ? String() : String(" (Unavailable)")));
        }
    }
    if (!advancement_options_.feats.empty())
    {
        advancement_choice_.feat =
            advancement_options_.feats.at(window->get_node<OptionButton>("Feat")->get_selected())
            .id;
        // The Skilled page is offered by the rules only while Skilled is the
        // selection, so the options have to follow the feat for every class, not
        // just the ones with a Fighting Style.
        advancement_options_ = campaign_->advancement_options(advancing_, advancement_choice_);
    }
    // Keep the Skilled picks: they come from their own page, not this dropdown,
    // and switching away from Skilled drops them by dropping the group instead.
    auto skilled_picks = advancement_choice_.training["feat:skilled"];
    advancement_choice_.training.clear();
    if (const auto *group = advancement_dropdown_group())
    {
        const auto index = window->get_node<OptionButton>("AdvancementTraining")->get_selected();
        if (index > 0 && static_cast<std::size_t>(index) <= group->options.size())
            advancement_choice_.training[group->id] = {group->options[index - 1].id};
    }
    if (advancement_choice_.feat == "skilled")
        advancement_choice_.training["feat:skilled"] = std::move(skilled_picks);
    const bool ability = advancement_choice_.feat == "ability_score_improvement";
    for (unsigned i = 0; i < 6; ++i)
    {
        auto *points = window->get_node<OptionButton>(String("Ability") + String::num_uint64(i));
        points->set_disabled(!ability);
        if (!ability)
            points->select(0);
        const auto shown = opengold::rules::all_abilities[i];
        advancement_choice_.abilities[shown] = ability ? points->get_selected() : 0;
        const auto value = campaign_->member(advancing_).character.sheet().scores[shown];
        const std::array<const char *, 6> labels{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
        window->get_node<Label>(String("AbilityLabel") + String::num_uint64(i))
        ->set_text(String(labels[i]) + " " + String::num_int64(value) + String::utf8(" → ") +
                   String::num_int64(value + advancement_choice_.abilities[shown]));
    }
    if (!advancement_choice_.spell_learning)
    {
        advancement_choice_.spells.clear();
        // The window has four spell checkboxes while a catalog may list more -- the
        // Cleric's fifth entry is its permanently unavailable Bless row, which the
        // display loop above already declines to show. Reading past the fourth
        // control asked for a node that does not exist and aborted every Cleric
        // level-up, so this bound matches the controls that were actually built.
        for (unsigned i = 0; i < std::min<std::size_t>(4, advancement_options_.spells.size()); ++i)
            if (window->get_node<CheckBox>(String("Spell") + String::num_uint64(i))->is_pressed())
                advancement_choice_.spells.push_back(advancement_options_.spells[i].id);
    }
    auto preview_choice = advancement_choice_;
    // The HP preview and the Next button both come from previewing the whole
    // transaction, so a page the player has not reached yet must preview with a
    // valid stand-in rather than blocking the way to it. Once the Skilled page is
    // showing, the real picks apply and an incomplete three gates Confirm.
    if (advancement_page_ == AdvancementPage::choices)
        if (const auto *group = advancement_skilled_group())
        {
            auto &fill = preview_choice.training["feat:skilled"];
            if (fill.size() != group->count)
            {
                fill.clear();
                for (const auto &option : group->options)
                {
                    if (fill.size() == group->count)
                        break;
                    fill.push_back(option.id);
                }
            }
        }
    if (preview_choice.spell_learning && advancement_page_ != AdvancementPage::spells)
    {
        const auto defaults = campaign_->default_advancement(advancing_);
        preview_choice.spell_learning = defaults.spell_learning;
        preview_choice.spells = defaults.spells;
    }
    try
    {
        const auto preview = campaign_->preview_advancement(advancing_, preview_choice);
        const auto &old = campaign_->member(advancing_);
        window->get_node<Label>("HP")->set_text(
            "Maximum HP: " + String::num_int64(old.character.sheet().hit_points) +
            String::utf8(" → ") + String::num_int64(preview.character.sheet().hit_points) +
            "   /   Current HP: " + String::num_int64(preview.vitals.hit_points));
        window->get_node<Label>("Error")->set_text("");
        window->get_node<Button>("Confirm")->set_disabled(false);
    }
    catch (const std::exception &e)
    {
        window->get_node<Label>("HP")->set_text("Choose valid options to preview your new HP.");
        window->get_node<Label>("Error")->set_text(gs(e.what()));
        window->get_node<Button>("Confirm")->set_disabled(true);
    }
    // Selecting or leaving Skilled changes which pages exist, so the page state
    // has to follow every choice change, not only Back/Next. Nothing reached
    // from here calls back into this function.
    advancement_pages();
}

void CharacterCreationView::close_advancement()
{
    get_node<Window>("LevelUp")->hide();
    advancing_ = 0;
}

void CharacterCreationView::confirm_advancement()
{
    if (!advancing_)
        return;
    // Confirm doubles as Next until the last page; only then does it apply the
    // whole advancement in one transaction.
    if (const auto next = advancement_next_page(advancement_page_); next != advancement_page_)
    {
        advancement_page_ = next;
        advancement_pages();
        advancement_changed();
        return;
    }
    try
    {
        campaign_->advance(advancing_, advancement_choice_);
        close_advancement();
        refresh_party();
        refresh_advancement_arrows();
        if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
            town->resume_party();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("LevelUp/Error")->set_text(gs(e.what()));
    }
}

void CharacterCreationView::advancement_check()
{
    if (advancement_frames_ > 6000)
        throw std::runtime_error("Advancement check timed out");
    if (++advancement_frames_ % 4)
        return;
    refresh_advancement_arrows();
    const auto press = [&](const String &path)
    {
        auto *button = get_node<Button>(path);
        if (button->is_disabled())
            throw std::runtime_error("Disabled advancement check control");
        button->emit_signal("pressed");
    };
    const auto arrow = [](opengold::MemberId id)
    {
        return String("PartyPanel/Roster/Advance") + String::num_uint64(id);
    };
    const auto capture_dialog = [&](const char *name)
    {
        if (!capture_)
            return;
        const auto image = get_node<Window>("LevelUp")->get_texture()->get_image();
        if (image.is_null() || image->save_png(ProjectSettings::get_singleton()->globalize_path(
                String("res://../../user-data/") + name)) != OK)
            throw std::runtime_error("Level-up capture failed");
    };
    const auto select = [&](const String &path, int index)
    {
        auto *option = get_node<OptionButton>(path);
        option->select(index);
        option->emit_signal("item_selected", index);
    };
    const auto id = campaign_->state().roster.empty() ? 0 : campaign_->state().slots[0];
    switch (advancement_stage_)
    {
    case 0:
    {
        for (const char *klass :
                {"wizard", "fighter", "cleric"
                })
        {
            opengold::rules::CharacterDraft draft;
            draft.race = "human";
            draft.gender = "female";
            draft.character_class = klass;
            draft.alignment = "neutral_good";
            draft.background = klass == std::string_view("fighter") ? "soldier" : "sage";
            draft.name = std::string(klass == std::string_view("wizard")    ? "Mira"
                                     : klass == std::string_view("fighter") ? "Tessa"
                                     : "Lena") +
                                                               " / " + klass;
            draft.rolled = true;
            for (auto &roll : draft.rolls)
                roll = {{6, 5, 4, 1}, 3};
            campaign_->add_pc(opengold::Character(*opengold::srd5::character_rules(), draft, {}));
        }
        campaign_->award_experience(2700, "fixture:level-up-review");
        const auto slots = campaign_->state().slots;
        for (const auto member : slots)
            if (member)
                for (unsigned level = 2; level <= 3; ++level)
                    campaign_->advance(member, campaign_->default_advancement(member));
        party_action(0);
        error_ =
            "Review party: each character is ready for level 4. Click the arrow beside a name.";
        refresh_party();
        refresh_advancement_arrows();
        if (advancement_review_)
        {
            advancement_review_ = false;
            return;
        }
        break;
    }
    case 1:
    {
        auto *list = get_node<ItemList>("PartyPanel/Roster");
        for (unsigned i = 0; i < 3; ++i)
        {
            auto *button = get_node<Button>(arrow(campaign_->state().slots[i]));
            if (std::cmp_not_equal(
                        list->get_item_at_position(button->get_position() + button->get_size() / 2,
                                                   true),
                        i))
                throw std::runtime_error("Level-up arrow is not beside its own character row");
        }
        capture("level-up-arrows.png");
        press(arrow(id));
        break;
    }
    case 2:
    {
        capture_dialog("level-up-wizard.png");
        const auto before = opengold::encode_campaign(*campaign_, nullptr, "ui-check");
        press("LevelUp/Cancel");
        if (opengold::encode_campaign(*campaign_, nullptr, "ui-check") != before)
            throw std::runtime_error("Cancel mutated campaign");
        press(arrow(id));
        for (int i = 0; i < 6; ++i)
            select(String("LevelUp/Ability") + String::num_int64(i), 0);
        if (!get_node<Button>("LevelUp/Confirm")->is_disabled())
            throw std::runtime_error("Incomplete points can be confirmed");
        select("LevelUp/Ability2", 2);
        get_node<CheckBox>("LevelUp/Spell1")->set_pressed(true);
        break;
    }
    case 3:
        capture_dialog("level-up-choices.png");
        press("LevelUp/Confirm");
        if (!get_node<Control>("LevelUp/SpellChoicesPage")->is_visible())
            throw std::runtime_error("A Wizard's spell page follows the first page");
        press("LevelUp/Confirm");
        if (campaign_->member(id).character.sheet().level != 4 ||
                // The SRD Wizard prepares seven spells at level 4.
                campaign_->member(id).character.sheet().prepared_spells.size() != 7 ||
                get_node<Button>(arrow(id))->is_visible())
            throw std::runtime_error("Wizard confirmation did not apply choices and hide arrow");
        show_modifiers();
        {
            const auto text = get_node<RichTextLabel>("ModifiersModal/Text")->get_text();
            if (!text.contains(
                        "Sage background (+2)\nLevel 4 Ability Score Improvement (+2)\nFinal score: 19") ||
                    text.contains("Sage background (+4)"))
                throw std::runtime_error(
                    "Modifier dialog must separate background and level-four feat sources");
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "bonus-ui-check");
            const auto module = opengold::srd5::load(presentation::path_from_godot(
                    ProjectSettings::get_singleton()
                    ->globalize_path("res://../../data/rules/srd-5.2.1/combat.rules")));
            auto restored = opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
                *module, "bonus-ui-check", nullptr);
            campaign_->restore(std::move(restored.party));
            show_modifiers();
            if (get_node<RichTextLabel>("ModifiersModal/Text")->get_text() != text)
                throw std::runtime_error(
                    "Saved bonus sources must reconstruct the same modifier dialog");
        }
        close_modifiers();
        party_action(7);
        break;
    case 4:
    {
        auto *town = get_node<RolfTourView>("CampaignTown");
        if (!town->can_leave())
        {
            auto *next = town->get_node<Button>("Continue");
            if (next->is_visible() && !next->is_disabled())
                next->emit_signal("pressed");
            return;
        }
        town->resume_party();
        auto *button = town->get_node<Button>("PartyList/Rows/Member1/Advance");
        if (!button->is_visible())
            throw std::runtime_error("Town level-up arrow is missing");
        button->emit_signal("pressed");
        if (!get_node<Window>("LevelUp")->is_visible())
            throw std::runtime_error("Town arrow did not open advancement");
        select("LevelUp/Feat", 1);
        // A level-four Fighter also masters a fourth weapon.
        select("LevelUp/AdvancementTraining", 1);
        break;
    }
    case 5:
    {
        capture_dialog("level-up-fighter.png");
        press("LevelUp/Confirm");
        const auto &fighter = campaign_->member(campaign_->state().slots[1]).character;
        const auto has = [&](const opengold::rules::FeatureGrant & grant)
        {
            return std::find(fighter.sheet().grants.begin(), fighter.sheet().grants.end(), grant) !=
                   fighter.sheet().grants.end();
        };
        if (!has({"feat:defense", "class:fighter:ability_score_improvement", 4, {}}) ||
                !has({"feat:savage_attacker", "background:soldier", 1, {}}))
            throw std::runtime_error(
                "Fighter must retain separate creation and advancement grants");
        const auto text = sheet_text(fighter);
        if (!text.contains("savage attacker") || !text.contains("defense"))
            throw std::runtime_error("Sheet must display both acquired feats");
    }
    get_node<RolfTourView>("CampaignTown")
    ->get_node<Button>("PartyList/Rows/Member2/Advance")
    ->emit_signal("pressed");
    get_node<CheckBox>("LevelUp/Spell1")->set_pressed(true);
    break;
    case 6:
        capture_dialog("level-up-cleric.png");
        press("LevelUp/Confirm");
        party_action(9);
        capture("level-up-complete.png");
        UtilityFunctions::print(
            "Godot advancement passed: roster and town arrows, HP preview, Cancel rollback, invalid-point prevention, level-four feat and spell confirmations.");
        advancement_check_ = false;
        get_tree()->quit(0);
        break;
    }
    ++advancement_stage_;
}
