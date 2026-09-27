#include "spell_choice_controls.h"
#include "godot_nodes.h"
#include "localization.h"
#include "game_resources.h"
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
#include <algorithm>

using namespace godot;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}

} // namespace

void CharacterCreationView::setup_advancement()
{
    get_node<ItemList>("PartyPanel/Roster")->add_theme_constant_override("v_separation", 8);
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    advancement_check_ = args.has("--advancement-check") || args.has("--champion-creator");
    advancement_review_ = args.has("--level-up-review");
    auto owned = presentation::make_node<Window>();
    owned->set_name("LevelUp");
    owned->set_title(i18n::text(N_("Level up")));
    owned->set_size(Vector2i(700, 670));
    owned->set_min_size(Vector2i(700, 670));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *window = owned.get();
    presentation::attach_child(*this, std::move(owned));
    window->connect("close_requested",
                    callable_mp(this, &CharacterCreationView::close_advancement));
    auto *spell_page = presentation::add_control<ScrollContainer>(*window, "SpellChoicesPage",
                                                                  Rect2(24, 70, 652, 475));
    spell_page->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    spell_page->set_follow_focus(true);
    spell_page->hide();
    presentation::spell_rows(*spell_page, "Rows");
    auto *back = presentation::add_control<Button>(*window, "Back", Rect2(236, 610, 136, 40));
    back->set_text(i18n::text(N_("Back")));
    back->hide();
    back->connect("pressed", callable_mp(this, &CharacterCreationView::advancement_back));
    auto *title = presentation::add_control<Label>(*window, "Title", Rect2(24, 20, 652, 40));
    title->add_theme_font_size_override("font_size", 24);
    title->set_clip_text(true);
    presentation::add_control<Label>(*window, "HP", Rect2(24, 70, 652, 42));
    presentation::add_control<Label>(*window, "FeatLabel", Rect2(24, 122, 652, 28))
        ->set_text(i18n::text(N_("Feat or ability points")));
    auto *feat = presentation::add_control<OptionButton>(*window, "Feat", Rect2(24, 155, 652, 38));
    feat->connect("item_selected", callable_mp(this, &CharacterCreationView::advancement_changed));
    auto *training_label = presentation::add_control<Label>(*window, "AdvancementTrainingLabel",
                                                            Rect2(24, 205, 652, 25));
    training_label->hide();
    auto *training = presentation::add_control<OptionButton>(*window, "AdvancementTraining",
                                                             Rect2(24, 236, 652, 36));
    training->hide();
    training->connect("item_selected",
                      callable_mp(this, &CharacterCreationView::advancement_changed));
    const std::array<const char *, 6> abilities{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    for (unsigned i = 0; i < 6; ++i)
    {
        presentation::add_control<Label>(*window, String("AbilityLabel") + String::num_uint64(i),
                                         Rect2(24 + i * 110, 205, 100, 25))
            ->set_text(i18n::text(abilities[i]));
        auto *points = presentation::add_control<OptionButton>(
            *window, String("Ability") + String::num_uint64(i), Rect2(24 + i * 110, 236, 100, 36));
        for (int n = 0; n <= 2; ++n)
            points->add_item(String("+") + String::num_int64(n));
        points->connect("item_selected",
                        callable_mp(this, &CharacterCreationView::advancement_changed));
    }
    presentation::add_control<Label>(*window, "SpellLabel", Rect2(24, 292, 652, 28))
        ->set_text(i18n::text(N_("Prepared spells")));
    auto *style =
        presentation::add_control<OptionButton>(*window, "FightingStyle", Rect2(24, 325, 652, 38));
    style->hide();
    style->connect("item_selected", callable_mp(this, &CharacterCreationView::advancement_changed));
    for (int i = 0; i < 4; ++i)
    {
        auto *spell = presentation::add_control<CheckBox>(
            *window, String("Spell") + String::num_int64(i), Rect2(24, 325 + i * 38, 652, 36));
        spell->connect(
            "toggled",
            callable_mp(this, &CharacterCreationView::advancement_spell_changed).bind(i));
    }
    auto *note = presentation::add_control<Label>(*window, "Note", Rect2(24, 489, 652, 66));
    note->set_text(i18n::text(N_(
        "Fixed-average HP growth. Existing resource expenditure is preserved.\nAdditional class and subclass features are unavailable in this version.")));
    note->add_theme_font_size_override("font_size", 14);
    note->set("autowrap_mode", 3);
    auto *error = presentation::add_control<Label>(*window, "Error", Rect2(24, 560, 652, 34));
    error->add_theme_font_size_override("font_size", 15);
    auto *cancel = presentation::add_control<Button>(*window, "Cancel", Rect2(386, 610, 136, 40));
    cancel->set_text(i18n::text(N_("Cancel")));
    cancel->connect("pressed", callable_mp(this, &CharacterCreationView::close_advancement));
    auto *confirm = presentation::add_control<Button>(*window, "Confirm", Rect2(536, 610, 140, 40));
    confirm->set_text(i18n::text(N_("Confirm")));
    confirm->connect("pressed", callable_mp(this, &CharacterCreationView::confirm_advancement));
}

void CharacterCreationView::advancement_spell_page()
{
    if (!advancing_)
        return;
    auto *w = get_node<Window>("LevelUp");
    const bool wizard = advancement_choice_.spell_learning.has_value();
    w->get_node<Control>("SpellChoicesPage")->set_visible(wizard && advancement_spell_page_);
    w->get_node<Button>("Back")->set_visible(wizard && advancement_spell_page_);
    w->get_node<Button>("Confirm")->set_text(i18n::text(N_("Confirm")));
    if (!wizard)
        return;
    w->get_node<Button>("Confirm")->set_text(advancement_spell_page_ ? i18n::text(N_("Confirm"))
                                                                     : i18n::text(N_("Next")));
    for (const char *name : {"HP", "FeatLabel", "Feat", "Note"})
        w->get_node<Control>(name)->set_visible(!advancement_spell_page_);
    const bool training = !advancement_options_.training.empty();
    for (const char *name : {"AdvancementTrainingLabel", "AdvancementTraining"})
        w->get_node<Control>(name)->set_visible(!advancement_spell_page_ && training);
    for (unsigned i = 0; i < 6; ++i)
    {
        w->get_node<Control>(String("Ability") + String::num_uint64(i))
            ->set_visible(!advancement_spell_page_ && !training);
        w->get_node<Control>(String("AbilityLabel") + String::num_uint64(i))
            ->set_visible(!advancement_spell_page_ && !training);
    }
    w->get_node<Control>("SpellLabel")->hide();
    for (unsigned i = 0; i < 4; ++i)
        w->get_node<Control>(String("Spell") + String::num_uint64(i))->hide();
    if (!advancement_spell_page_)
        return;
    auto sheet = campaign_->member(advancing_).character.sheet();
    sheet.level = advancement_options_.level;
    auto options = campaign_->rule_module().spell_choice_options(
        sheet, opengold::rules::SpellChoiceContext::advancement);
    opengold::rules::SpellChoices choices{
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    auto learning = choices;
    learning.prepared.reset();
    try
    {
        campaign_->rule_module().apply_spell_choices(
            sheet, learning, opengold::rules::SpellChoiceContext::advancement, false);
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
            return i18n::text(source);
        });
}

void CharacterCreationView::advancement_back()
{
    advancement_spell_page_ = false;
    advancement_spell_page();
    advancement_changed();
}

void CharacterCreationView::advancement_learning_toggled(bool selected, String group, String option)
{
    opengold::rules::SpellChoices choices{
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    presentation::toggle_spell(choices, selected, group.utf8().get_data(),
                               option.utf8().get_data());
    advancement_choice_.spell_learning = choices.learning;
    advancement_choice_.spells = *choices.prepared;
    if (group != "prepared" && !selected)
    {
        const std::string id = option.utf8().get_data();
        std::erase(advancement_choice_.spells, id);
    }
    advancement_spell_page();
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
            arrow = presentation::add_control<Button>(*list, name, Rect2(0, 0, 30, 26));
            arrow->set_text(String::utf8("↑"));
            arrow->set_tooltip_text(
                i18n::format("Level up {name}", {{"name", gs(member.character.sheet().name)}}));
            arrow->add_theme_font_size_override("font_size", 14);
            arrow->connect(
                "pressed",
                callable_mp(this, &CharacterCreationView::open_advancement).bind(member.id));
        }
        arrow->set_tooltip_text(
            i18n::format("Level up {name}", {{"name", gs(member.character.sheet().name)}}));
        const auto rect = list->get_item_rect(i);
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
                             [&](const auto &m)
                             {
                                 return m.id == id;
                             }))
                arrow->hide();
        }
}

void CharacterCreationView::open_advancement(std::int64_t id)
{
    if (campaign_defeated_ || !campaign_->can_advance(id))
        return;
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
        town && town->is_visible() && !town->can_leave())
        return;
    advancement_spell_page_ = false;
    advancing_ = id;
    advancement_options_ = campaign_->advancement_options(id);
    advancement_choice_ = campaign_->default_advancement(id);
    advancement_refreshing_ = true;
    auto *window = get_node<Window>("LevelUp");
    for (const char *name : {"HP", "FeatLabel", "Feat", "Note", "SpellLabel"})
        window->get_node<Control>(name)->show();
    window->get_node<Label>("Title")->set_text(i18n::format(
        "{name} / Level {level}", {{"name", gs(campaign_->member(id).character.sheet().name)},
                                   {"level", advancement_options_.level}}));
    window->get_node<Label>("Note")->set_text(i18n::text(advancement_options_.description));
    auto *feat = window->get_node<OptionButton>("Feat");
    feat->clear();
    if (advancement_options_.feats.empty())
        feat->add_item(i18n::text(N_("No feat or ability increase at this level")));
    for (unsigned i = 0; i < advancement_options_.feats.size(); ++i)
    {
        const auto &option = advancement_options_.feats[i];
        feat->add_item(i18n::text(option.label) +
                       (option.available ? String() : i18n::text(" (Unavailable)")));
        feat->set_item_disabled(i, !option.available);
        feat->set_item_tooltip(i, i18n::text(option.description));
        if (option.id == advancement_choice_.feat)
            feat->select(i);
    }
    feat->set_disabled(advancement_options_.feats.empty());
    const bool has_training = !advancement_options_.training.empty();
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
        const auto &group = advancement_options_.training.front();
        window->get_node<Label>("AdvancementTrainingLabel")->set_text(i18n::text(group.label));
        training->add_item(i18n::text("Choose an option"));
        for (const auto &option : group.options)
            training->add_item(i18n::text(option.label) +
                               (option.description.empty()
                                    ? String()
                                    : String(" / ") + i18n::text(option.description)));
        training->select(0);
        advancement_choice_.training.clear();
    }
    for (unsigned i = 0; i < 6; ++i)
        window->get_node<OptionButton>(String("Ability") + String::num_uint64(i))
            ->select(advancement_choice_.abilities[i]);
    window->get_node<Label>("SpellLabel")
        ->set_text(i18n::text(advancement_options_.spells.empty()
                                  ? N_("No spell choices for this class")
                                  : N_("Prepared spells: select at least one")));
    for (unsigned i = 0; i < 4; ++i)
    {
        auto *spell = window->get_node<CheckBox>(String("Spell") + String::num_uint64(i));
        spell->set_visible(i < advancement_options_.spells.size());
        if (i >= advancement_options_.spells.size())
            continue;
        const auto &option = advancement_options_.spells[i];
        spell->set_text(i18n::text(option.label) +
                        (option.available ? String() : i18n::text(" (Unavailable)")));
        spell->set_tooltip_text(i18n::text(option.description));
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
        window->get_node<Label>("SpellLabel")->set_text(i18n::text("Fighting Style"));
        for (const auto &option : advancement_options_.fighting_styles)
        {
            const int i = style->get_item_count();
            style->add_item(i18n::text(option.label));
            style->set_item_disabled(i, !option.available);
            style->set_item_tooltip(i, i18n::text(option.description));
            if ((advancement_choice_.fighting_style &&
                 option.id == *advancement_choice_.fighting_style) ||
                (!advancement_choice_.fighting_style && option.id == "keep"))
                style->select(i);
        }
    }
    advancement_refreshing_ = false;
    advancement_spell_page();
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
            feats->set_item_text(i,
                                 i18n::text(option.label) +
                                     (option.available ? String() : i18n::text(" (Unavailable)")));
        }
    }
    if (!advancement_options_.feats.empty())
        advancement_choice_.feat =
            advancement_options_.feats.at(window->get_node<OptionButton>("Feat")->get_selected())
                .id;
    advancement_choice_.training.clear();
    if (!advancement_options_.training.empty())
    {
        const auto &group = advancement_options_.training.front();
        const auto index = window->get_node<OptionButton>("AdvancementTraining")->get_selected();
        if (index > 0 && static_cast<std::size_t>(index) <= group.options.size())
            advancement_choice_.training[group.id] = {group.options[index - 1].id};
    }
    const bool ability = advancement_choice_.feat == "ability_score_improvement";
    for (unsigned i = 0; i < 6; ++i)
    {
        auto *points = window->get_node<OptionButton>(String("Ability") + String::num_uint64(i));
        points->set_disabled(!ability);
        if (!ability)
            points->select(0);
        advancement_choice_.abilities[i] = ability ? points->get_selected() : 0;
        const auto value = campaign_->member(advancing_).character.sheet().scores[i];
        const std::array<const char *, 6> labels{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
        window->get_node<Label>(String("AbilityLabel") + String::num_uint64(i))
            ->set_text(i18n::text(labels[i]) + " " + String::num_int64(value) +
                       String::utf8(" → ") +
                       String::num_int64(value + advancement_choice_.abilities[i]));
    }
    if (!advancement_choice_.spell_learning)
    {
        advancement_choice_.spells.clear();
        for (unsigned i = 0; i < advancement_options_.spells.size(); ++i)
            if (window->get_node<CheckBox>(String("Spell") + String::num_uint64(i))->is_pressed())
                advancement_choice_.spells.push_back(advancement_options_.spells[i].id);
    }
    auto preview_choice = advancement_choice_;
    if (preview_choice.spell_learning && !advancement_spell_page_)
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
            i18n::format("Maximum HP: {old} -> {new} / Current HP: {current}",
                         {{"old", old.character.sheet().hit_points},
                          {"new", preview.character.sheet().hit_points},
                          {"current", preview.vitals.hit_points}}));
        window->get_node<Label>("Error")->set_text("");
        window->get_node<Button>("Confirm")->set_disabled(false);
    }
    catch (const std::exception &e)
    {
        window->get_node<Label>("HP")->set_text(
            i18n::text(N_("Choose valid options to preview your new HP.")));
        window->get_node<Label>("Error")->set_text(i18n::text(e.what()));
        window->get_node<Button>("Confirm")->set_disabled(true);
    }
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
    if (advancement_choice_.spell_learning && !advancement_spell_page_)
    {
        advancement_spell_page_ = true;
        advancement_spell_page();
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
        get_node<Label>("LevelUp/Error")->set_text(i18n::text(e.what()));
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
                                   String("user://checks/") + name)) != OK)
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
        if (OS::get_singleton()->get_cmdline_user_args().has("--champion-creator"))
        {
            opengold::rules::CharacterDraft draft;
            draft.race = "human";
            draft.gender = "female";
            draft.character_class = "fighter";
            draft.background = "sage";
            draft.name = "Champion review";
            draft.alignment = "neutral_good";
            draft.rolled = true;
            for (auto &roll : draft.rolls)
                roll = {{6, 5, 4, 1}, 3};
            const auto member = campaign_->add_pc(
                opengold::Character(*opengold::srd5::character_rules(), draft, {}));
            campaign_->award_experience(2700, "fixture:champion-review");
            campaign_->advance(member, campaign_->default_advancement(member));
            party_action(0);
            refresh_party();
            refresh_advancement_arrows();
            open_advancement(member);
            advancement_check_ = false;
            advancement_review_ = false;
            return;
        }
        for (const char *klass : {"wizard", "fighter", "cleric", "fighter"})
        {
            opengold::rules::CharacterDraft draft;
            draft.race =
                advancement_check_ && std::string_view(klass) == "wizard" ? "dwarf" : "human";
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
        for (unsigned i = 0; i < 4; ++i)
        {
            auto *button = get_node<Button>(arrow(campaign_->state().slots[i]));
            if (list->get_item_at_position(button->get_position() + button->get_size() / 2, true) !=
                i)
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
        if (campaign_->member(id).character.sheet().level != 4 ||
            campaign_->member(id).character.sheet().prepared_spells.size() != 2 ||
            get_node<Button>(arrow(id))->is_visible())
            throw std::runtime_error("Wizard confirmation did not apply choices and hide arrow");
        show_modifiers();
        {
            const auto text = get_node<RichTextLabel>("ModifiersModal/Text")->get_text();
            if (!text.contains(
                    i18n::format("{background} background", {{"background", i18n::text("Sage")}}) +
                    " (+2)\n" +
                    i18n::format("Level {level} Ability Score Improvement", {{"level", 4}}) +
                    " (+2)\n" + i18n::format("Final score: {score}", {{"score", 19}})) ||
                text.contains(
                    i18n::format("{background} background", {{"background", i18n::text("Sage")}}) +
                    " (+4)"))
                throw std::runtime_error(
                    "Modifier dialog must separate background and level-four feat sources");
            if (!text.contains(i18n::format("Dwarven Toughness: +{hp} maximum HP.", {{"hp", 4}})))
                throw std::runtime_error(
                    "Racial section must show the attained Dwarven Toughness contribution");
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "bonus-ui-check");
            const auto module =
                opengold::srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data()));
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
        break;
    }
    case 5:
    {
        capture_dialog("level-up-fighter.png");
        press("LevelUp/Confirm");
        const auto &fighter = campaign_->member(campaign_->state().slots[1]).character;
        const auto has = [&](const opengold::rules::FeatureGrant &grant)
        {
            return std::find(fighter.sheet().grants.begin(), fighter.sheet().grants.end(), grant) !=
                   fighter.sheet().grants.end();
        };
        if (!has({"feat:defense", "class:fighter:ability_score_improvement", 4, {}}) ||
            !has({"feat:savage_attacker", "background:soldier", 1, {}}))
            throw std::runtime_error(
                "Fighter must retain separate creation and advancement grants");
        const auto text = sheet_text(fighter);
        if (!text.contains(i18n::text("savage attacker")) || !text.contains(i18n::text("defense")))
            throw std::runtime_error("Sheet must display both acquired feats");
    }
        get_node<RolfTourView>("CampaignTown")
            ->get_node<Button>("PartyList/Rows/Member2/Advance")
            ->emit_signal("pressed");
        select("LevelUp/Feat", 2);
        get_node<CheckBox>("LevelUp/Spell1")->set_pressed(true);
        break;
    case 6:
    {
        capture_dialog("level-up-cleric.png");
        press("LevelUp/Confirm");
        const auto cleric_id = campaign_->state().slots[2];
        const auto has_feat = [&]
        {
            const auto &grants = campaign_->member(cleric_id).character.sheet().grants;
            return std::find(grants.begin(), grants.end(),
                             opengold::rules::FeatureGrant{"feat:savage_attacker",
                                                           "class:cleric:ability_score_improvement",
                                                           4,
                                                           {}}) != grants.end();
        };
        if (!has_feat() ||
            campaign_->member(cleric_id).character.sheet().prepared_spells.size() != 2)
            throw std::runtime_error(
                "Cleric selection must grant Savage Attacker with its source and chosen spells");
        const auto saved = opengold::encode_campaign(*campaign_, nullptr, "feat-ui-check");
        const auto module =
            opengold::srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data()));
        auto restored = opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
                                                  *module, "feat-ui-check", nullptr);
        campaign_->restore(std::move(restored.party));
        if (!has_feat() || opengold::encode_campaign(*campaign_, nullptr, "feat-ui-check") != saved)
            throw std::runtime_error("UI-acquired feat must survive campaign reload");
        get_node<RolfTourView>("CampaignTown")
            ->get_node<Button>("PartyList/Rows/Member3/Advance")
            ->emit_signal("pressed");
        auto *feats = get_node<OptionButton>("LevelUp/Feat");
        const auto archery =
            std::find_if(advancement_options_.feats.begin(), advancement_options_.feats.end(),
                         [](const auto &f)
                         {
                             return f.id == "archery";
                         });
        if (archery == advancement_options_.feats.end() || !archery->available)
            throw std::runtime_error("Fighter Archery must be selectable");
        select("LevelUp/Feat", static_cast<int>(archery - advancement_options_.feats.begin()));
        if (feats->get_item_text(feats->get_selected()) != i18n::text("Archery"))
            throw std::runtime_error("Archery choice must be translated");
        break;
    }
    case 7:
    {
        capture_dialog("level-up-archery.png");
        press("LevelUp/Confirm");
        const auto archer = campaign_->state().slots[3];
        const auto &sheet = campaign_->member(archer).character.sheet();
        if (std::find(sheet.grants.begin(), sheet.grants.end(),
                      opengold::rules::FeatureGrant{
                          "feat:archery", "class:fighter:ability_score_improvement", 4, {}}) ==
            sheet.grants.end())
            throw std::runtime_error("Confirmed Archery lacks its entitlement grant");
        if (!sheet_text(campaign_->member(archer).character).contains(i18n::text("archery")))
            throw std::runtime_error("Sheet must display translated Archery");
        const auto bytes = opengold::encode_campaign(*campaign_, nullptr, "archery-ui-check");
        const auto module =
            opengold::srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data()));
        auto restored = opengold::decode_campaign(bytes, *opengold::srd5::character_rules(),
                                                  *module, "archery-ui-check", nullptr);
        campaign_->restore(std::move(restored.party));
        if (opengold::encode_campaign(*campaign_, nullptr, "archery-ui-check") != bytes)
            throw std::runtime_error("UI-acquired Archery must survive reload");
        party_action(9);
        capture("level-up-complete.png");
        UtilityFunctions::print(
            "Godot advancement passed: roster and town arrows, HP preview, Cancel rollback, invalid-point prevention, level-four feat and spell confirmations.");
        advancement_check_ = false;
        get_tree()->quit(0);
        break;
    }
    }
    ++advancement_stage_;
}
