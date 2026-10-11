#include "level_up_dialog.h"
#include "spell_choice_controls.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "scoped_flag.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

using namespace godot;
using presentation::required_node;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}

} // namespace

presentation::NodeOwner<> LevelUpDialog::create()
{
    auto scene = presentation::instantiate_scene("res://scenes/level_up_dialog.tscn");
    auto *dialog = Object::cast_to<LevelUpDialog>(scene.get());
    if (!dialog)
        throw std::runtime_error("Invalid level-up dialog scene");
    dialog->set_title(i18n::text(N_("Level up")));
    dialog->bind_controls();
    return scene;
}

void LevelUpDialog::connect_host(CampaignAccess campaign, std::function<void()> advanced,
                                 FailureReport report)
{
    if (!campaign || !advanced || !report)
        throw std::logic_error("Level-up dialog needs a campaign, a follow-up and a report");
    campaign_ = std::move(campaign);
    advanced_ = std::move(advanced);
    report_ = std::move(report);
}

void LevelUpDialog::report_failure(const std::exception &failure)
{
    report_(failure);
}

opengold::CampaignParty &LevelUpDialog::campaign()
{
    return campaign_();
}

void LevelUpDialog::bind_controls()
{
    connect("close_requested", presentation::guarded(this, &LevelUpDialog::close));
    auto *back = &required_node<Button>(*this, "Back");
    back->set_text(i18n::text(N_("Back")));
    back->connect("pressed", presentation::guarded(this, &LevelUpDialog::advancement_back));
    required_node<Label>(*this, "FeatLabel")
    .set_text(i18n::text(N_("Feat or ability points")));
    auto *feat = &required_node<OptionButton>(*this, "Feat");
    feat->connect("item_selected",
                  presentation::guarded(this, &LevelUpDialog::advancement_changed));
    auto *training = &required_node<OptionButton>(*this, "AdvancementTraining");
    training->connect("item_selected",
                      presentation::guarded(this, &LevelUpDialog::advancement_changed));
    const std::array<const char *, 6> abilities{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    for (unsigned i = 0; i < 6; ++i)
    {
        required_node<Label>(*this, String("AbilityLabel") + String::num_uint64(i))
        .set_text(i18n::text(abilities[i]));
        auto *points = &required_node<OptionButton>(
                           *this, String("Ability") + String::num_uint64(i));
        for (int n = 0; n <= 2; ++n)
            points->add_item(String("+") + String::num_int64(n));
        points->connect("item_selected",
                        presentation::guarded(this, &LevelUpDialog::advancement_changed));
    }
    required_node<Label>(*this, "SpellLabel").set_text(i18n::text(N_("Prepared spells")));
    auto *style = &required_node<OptionButton>(*this, "FightingStyle");
    style->connect("item_selected",
                   presentation::guarded(this, &LevelUpDialog::advancement_changed));
    for (int i = 0; i < 4; ++i)
    {
        auto *spell = &required_node<CheckBox>(
                          *this, String("Spell") + String::num_int64(i));
        spell->connect(
            "toggled",
            presentation::guarded(this, &LevelUpDialog::advancement_spell_changed).bind(i));
    }
    auto *note = &required_node<Label>(*this, "Note");
    note->set_text(i18n::text(N_(
                                  "Fixed-average HP growth. Existing resource expenditure is preserved.\nAdditional class and subclass features are unavailable in this version.")));
    auto *cancel = &required_node<Button>(*this, "Cancel");
    cancel->set_text(i18n::text(N_("Cancel")));
    cancel->connect("pressed", presentation::guarded(this, &LevelUpDialog::close));
    auto *confirm = &required_node<Button>(*this, "Confirm");
    confirm->set_text(i18n::text(N_("Confirm")));
    confirm->connect("pressed", presentation::guarded(this, &LevelUpDialog::confirm));
}

const opengold::rules::TrainingChoiceGroup *
LevelUpDialog::advancement_skilled_group() const
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
LevelUpDialog::advancement_dropdown_group() const
{
    const auto found = std::find_if(advancement_options_.training.begin(),
                                    advancement_options_.training.end(),
                                    [](const auto & g)
    {
        return g.id != "feat:skilled";
    });
    return found == advancement_options_.training.end() ? nullptr : &*found;
}

LevelUpDialog::AdvancementPage
LevelUpDialog::advancement_next_page(AdvancementPage from) const
{
    if (from == AdvancementPage::choices && advancement_skilled_group())
        return AdvancementPage::skilled;
    if (from != AdvancementPage::spells && advancement_choice_.spell_learning)
        return AdvancementPage::spells;
    return from;
}

LevelUpDialog::AdvancementPage
LevelUpDialog::advancement_previous_page(AdvancementPage from) const
{
    if (from == AdvancementPage::spells && advancement_skilled_group())
        return AdvancementPage::skilled;
    return AdvancementPage::choices;
}

void LevelUpDialog::advancement_pages()
{
    if (!advancing_)
        return;
    // Wizards and Clerics choose spells on a second page.
    const bool spell_page = advancement_choice_.spell_learning.has_value();
    const auto *skilled = advancement_skilled_group();
    const bool on_first = advancement_page_ == AdvancementPage::choices;
    required_node<Control>(*this, "SpellChoicesPage")
    .set_visible(spell_page && advancement_page_ == AdvancementPage::spells);
    required_node<Control>(*this, "SkilledPage")
    .set_visible(skilled && advancement_page_ == AdvancementPage::skilled);
    required_node<Control>(*this, "SkilledCount")
    .set_visible(skilled && advancement_page_ == AdvancementPage::skilled);
    // Back appears on any page after the first rather than only a Wizard's
    // spell page, so a non-Wizard taking Skilled can still step back.
    required_node<Button>(*this, "Back").set_visible(!on_first);
    required_node<Button>(*this, "Confirm")
    .set_text(advancement_next_page(advancement_page_) == advancement_page_
              ? i18n::text(N_("Confirm"))
              : i18n::text(N_("Next")));
    for (const char *name :
            {"HP", "FeatLabel", "Feat", "Note"
            })
        required_node<Control>(*this, name).set_visible(on_first);
    // Match open_advancement exactly: a dropdown alongside a feat choice is
    // supplemental, and then the ability controls stay visible beside it.
    const bool has_training = advancement_dropdown_group() != nullptr;
    const bool supplemental = has_training && !advancement_options_.feats.empty();
    for (const char *name :
            {"AdvancementTrainingLabel", "AdvancementTraining"
            })
        required_node<Control>(*this, name).set_visible(on_first && has_training);
    for (unsigned i = 0; i < 6; ++i)
    {
        required_node<Control>(*this, String("Ability") + String::num_uint64(i))
        .set_visible(on_first && (!has_training || supplemental));
        required_node<Control>(*this, String("AbilityLabel") + String::num_uint64(i))
        .set_visible(on_first && (!has_training || supplemental));
    }
    if (advancement_page_ == AdvancementPage::skilled)
        refresh_advancement_skilled();
    if (!spell_page)
    {
        required_node<Control>(*this, "SpellLabel").set_visible(on_first);
        for (unsigned i = 0; i < 4; ++i)
            required_node<Control>(*this, String("Spell") + String::num_uint64(i))
            .set_visible(on_first && i < advancement_options_.spells.size());
        return;
    }
    required_node<Control>(*this, "SpellLabel").hide();
    for (unsigned i = 0; i < 4; ++i)
        required_node<Control>(*this, String("Spell") + String::num_uint64(i)).hide();
    if (advancement_page_ != AdvancementPage::spells)
        return;
    auto sheet = campaign().rule_module().spell_choice_sheet(
                     campaign().member(advancing_).character.sheet(), advancement_choice_);
    auto options = campaign().rule_module().spell_choice_options(
                       sheet, opengold::rules::SpellChoiceContext::advancement);
    opengold::rules::SpellChoices choices
    {
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    auto learning = choices;
    learning.prepared.reset();
    try
    {
        campaign().rule_module().apply_spell_choices(
            sheet, learning, opengold::rules::SpellChoiceContext::advancement,
            opengold::rules::ChoiceCompleteness::partial);
        options.preparation =
            campaign().rule_module()
            .spell_choice_options(sheet, opengold::rules::SpellChoiceContext::advancement)
            .preparation;
    }
    catch (const std::exception &)
    {
    } // Invalid edits remain visible; final preview reports the error.
    presentation::refresh_spell_groups(
        required_node<VBoxContainer>(*this, "SpellChoicesPage/Rows"), options, choices,
        presentation::guarded(this, &LevelUpDialog::advancement_learning_toggled),
        i18n::text, true);
}

// One checkbox per offered proficiency, prefixed so the two catalogs stay
// distinguishable in a single list. Controls are reused across refreshes so
// toggling never destroys the focused checkbox.
void LevelUpDialog::refresh_advancement_skilled()
{
    const auto *group = advancement_skilled_group();
    if (!group)
        return;
    auto &rows = required_node<VBoxContainer>(*this, "SkilledPage/Rows");
    const auto &picked = advancement_choice_.training["feat:skilled"];
    required_node<Label>(*this, "SkilledCount")
    .set_text(i18n::format("Selected: {count}/{total}",
    {
        {"count", static_cast<unsigned>(picked.size())},
        {"total", group->count}
    }));
    // Hide every row first; the loop below shows only what is still offered, so
    // no reverse name mapping is needed (ids contain underscores of their own).
    for (int n = 0; n < rows.get_child_count(); ++n)
        if (auto *box = Object::cast_to<CheckBox>(rows.get_child(n)))
            box->hide();
    for (const auto &option : group->options)
    {
        const auto name = presentation::training_string(option.id).replace(":", "_");
        auto *box = Object::cast_to<CheckBox>(rows.get_node_or_null(name));
        if (!box)
        {
            auto owned = presentation::make_node<CheckBox>();
            owned->set_name(name);
            box = owned.get();
            box->set_theme_type_variation("LevelUpChoice");
            box->connect("toggled", presentation::guarded(this,
                    &LevelUpDialog::advancement_skilled_toggled)
                         .bind(presentation::training_string(option.id)));
            presentation::attach_child(rows, std::move(owned));
        }
        // Each template has to sit directly inside the i18n::format call: the
        // catalog extractor matches on that call, so a literal behind a ternary
        // is never collected and would ship untranslated.
        box->set_text(option.id.starts_with("skill:")
        ? i18n::format("Skill: {name}", {{"name", i18n::text(option.label)}})
        : i18n::format("Tool: {name}", {{"name", i18n::text(option.label)}}));
        const bool on = std::find(picked.begin(), picked.end(), option.id) != picked.end();
        box->set_pressed_no_signal(on);
        // A full selection leaves only the chosen three toggleable, so the
        // count cannot be exceeded by clicking.
        box->set_disabled(!on && picked.size() >= group->count);
        box->show();
    }
}

void LevelUpDialog::advancement_skilled_toggled(bool selected, String option)
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

void LevelUpDialog::advancement_back()
{
    advancement_page_ = advancement_previous_page(advancement_page_);
    advancement_pages();
    advancement_changed();
}

void LevelUpDialog::advancement_learning_toggled(bool selected, String group, String option)
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

void LevelUpDialog::open(opengold::MemberId id)
{
    advancement_page_ = AdvancementPage::choices;
    advancing_ = id;
    advancement_options_ = campaign().advancement_options(id);
    advancement_choice_ = campaign().default_advancement(id);
    {
        const presentation::ScopedFlag refreshing(advancement_refreshing_);
        for (const char *name :
                {"HP", "FeatLabel", "Feat", "Note", "SpellLabel"
                })
            required_node<Control>(*this, name).show();
        required_node<Label>(*this, "Title").set_text(i18n::format(
        "{name} / Level {level}", {{"name", gs(campaign().member(id).character.sheet().name)},
            {"level", advancement_options_.level}
        }));
        required_node<Label>(*this, "Note")
        .set_text(i18n::text(advancement_options_.description));
        auto *feat = &required_node<OptionButton>(*this, "Feat");
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
        const auto *dropdown = advancement_dropdown_group();
        const bool has_training = dropdown != nullptr;
        const bool supplemental_training = has_training && !advancement_options_.feats.empty();
        auto *training = &required_node<OptionButton>(*this, "AdvancementTraining");
        training->clear();
        training->set_visible(has_training);
        required_node<Label>(*this, "AdvancementTrainingLabel").set_visible(has_training);
        auto &training_layout = required_node<Node>(*this, "TrainingLayout");
        training_layout.call("play", supplemental_training ? "supplemental" : "primary");
        training_layout.call("advance", 0);
        for (unsigned i = 0; i < 6; ++i)
        {
            required_node<Control>(*this, String("Ability") + String::num_uint64(i))
            .set_visible(!has_training || supplemental_training);
            required_node<Control>(*this, String("AbilityLabel") + String::num_uint64(i))
            .set_visible(!has_training || supplemental_training);
        }
        if (has_training)
        {
            const auto &group = *dropdown;
            required_node<Label>(*this, "AdvancementTrainingLabel")
            .set_text(i18n::text(group.label));
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
            required_node<OptionButton>(*this, String("Ability") + String::num_uint64(i))
            .select(advancement_choice_.abilities[opengold::rules::all_abilities[i]]);
        required_node<Label>(*this, "SpellLabel")
        .set_text(i18n::text(advancement_options_.spells.empty()
                             ? N_("No spell choices for this class")
                             : N_("Prepared spells: select at least one")));
        for (unsigned i = 0; i < 4; ++i)
        {
            auto *spell =
                &required_node<CheckBox>(*this, String("Spell") + String::num_uint64(i));
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
        auto *style = &required_node<OptionButton>(*this, "FightingStyle");
        style->clear();
        style->set_visible(!advancement_options_.fighting_styles.empty());
        if (!advancement_options_.fighting_styles.empty())
        {
            required_node<Label>(*this, "SpellLabel").set_text(i18n::text("Fighting Style"));
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
    }
    advancement_pages();
    advancement_changed();
    popup_centered();
    required_node<Button>(*this, "Cancel").grab_focus();
}

void LevelUpDialog::advancement_spell_changed(bool, int)
{
    advancement_changed();
}

// Spells learned through a Fighting Style (Blessed Warrior's cantrips) leave
// with it, so a changed style drops picks its learning group no longer offers.
void LevelUpDialog::drop_unoffered_learning()
{
    if (!advancement_choice_.spell_learning)
        return;
    const auto &rules = campaign().rule_module();
    const auto offered =
        rules.spell_choice_options(rules.spell_choice_sheet(
                                       campaign().member(advancing_).character.sheet(),
                                       advancement_choice_),
                                   opengold::rules::SpellChoiceContext::advancement)
        .learning;
    std::erase_if(*advancement_choice_.spell_learning, [&](const auto & entry)
    {
        return std::none_of(offered.begin(), offered.end(), [&](const auto & group)
        {
            return group.id == entry.first;
        });
    });
}

void LevelUpDialog::advancement_changed(std::int64_t)
{
    if (advancement_refreshing_ || !advancing_)
        return;
    if (!advancement_options_.fighting_styles.empty())
    {
        const auto index = required_node<OptionButton>(*this, "FightingStyle").get_selected();
        if (index >= 0)
        {
            const auto &option = advancement_options_.fighting_styles.at(index);
            if (option.id == "keep")
                advancement_choice_.fighting_style.reset();
            else
                advancement_choice_.fighting_style = option.id;
        }
        advancement_options_ = campaign().advancement_options(advancing_, advancement_choice_);
        drop_unoffered_learning();
        auto *feats = &required_node<OptionButton>(*this, "Feat");
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
    {
        advancement_choice_.feat =
            advancement_options_.feats
            .at(required_node<OptionButton>(*this, "Feat").get_selected())
            .id;
        // The Skilled page is offered by the rules only while Skilled is the
        // selection, so the options have to follow the feat for every class, not
        // just the ones with a Fighting Style.
        advancement_options_ = campaign().advancement_options(advancing_, advancement_choice_);
    }
    // Keep the Skilled picks: they come from their own page, not this dropdown,
    // and switching away from Skilled drops them by dropping the group instead.
    auto skilled_picks = advancement_choice_.training["feat:skilled"];
    advancement_choice_.training.clear();
    if (const auto *group = advancement_dropdown_group())
    {
        const auto index =
            required_node<OptionButton>(*this, "AdvancementTraining").get_selected();
        if (index > 0 && static_cast<std::size_t>(index) <= group->options.size())
            advancement_choice_.training[group->id] = {group->options[index - 1].id};
    }
    if (advancement_choice_.feat == "skilled")
        advancement_choice_.training["feat:skilled"] = std::move(skilled_picks);
    const bool ability = advancement_choice_.feat == "ability_score_improvement";
    for (unsigned i = 0; i < 6; ++i)
    {
        auto *points =
            &required_node<OptionButton>(*this, String("Ability") + String::num_uint64(i));
        points->set_disabled(!ability);
        if (!ability)
            points->select(0);
        const auto shown = opengold::rules::all_abilities[i];
        advancement_choice_.abilities[shown] = ability ? points->get_selected() : 0;
        const auto value = campaign().member(advancing_).character.sheet().scores[shown];
        const std::array<const char *, 6> labels{"STR", "DEX", "CON", "INT", "WIS", "CHA"};
        required_node<Label>(*this, String("AbilityLabel") + String::num_uint64(i))
        .set_text(i18n::text(labels[i]) + " " + String::num_int64(value) +
                  String::utf8(" → ") +
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
            if (required_node<CheckBox>(*this, String("Spell") + String::num_uint64(i))
                    .is_pressed())
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
        const auto defaults = campaign().default_advancement(advancing_);
        preview_choice.spell_learning = defaults.spell_learning;
        preview_choice.spells = defaults.spells;
        // The defaults follow the default Fighting Style; a chosen style's own
        // learning (Blessed Warrior's cantrips) previews with its first options.
        const auto &rules = campaign().rule_module();
        const auto offered = rules.spell_choice_options(
                                 rules.spell_choice_sheet(campaign().member(advancing_).character.sheet(),
                                     preview_choice),
                                 opengold::rules::SpellChoiceContext::advancement);
        for (const auto &group : offered.learning)
        {
            auto &fill = (*preview_choice.spell_learning)[group.id];
            for (const auto &option : group.options)
                if (fill.size() < group.count &&
                        std::find(fill.begin(), fill.end(), option.id) == fill.end())
                    fill.push_back(option.id);
        }
    }
    try
    {
        const auto preview = campaign().preview_advancement(advancing_, preview_choice);
        const auto &old = campaign().member(advancing_);
        required_node<Label>(*this, "HP").set_text(
            i18n::format("Maximum HP: {old} -> {new} / Current HP: {current}",
        {
            {"old", old.character.sheet().hit_points},
            {"new", preview.character.sheet().hit_points},
            {"current", preview.vitals.hit_points}
        }));
        required_node<Label>(*this, "Error").set_text("");
        required_node<Button>(*this, "Confirm").set_disabled(false);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "HP").set_text(
            i18n::text(N_("Choose valid options to preview your new HP.")));
        required_node<Label>(*this, "Error").set_text(i18n::text(e.what()));
        required_node<Button>(*this, "Confirm").set_disabled(true);
    }
    // Selecting or leaving Skilled changes which pages exist, so the page state
    // has to follow every choice change, not only Back/Next. Nothing reached
    // from here calls back into this function.
    advancement_pages();
}

void LevelUpDialog::close()
{
    hide();
    advancing_ = 0;
}

void LevelUpDialog::confirm()
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
        campaign().advance(advancing_, advancement_choice_);
        close();
        advanced_();
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "Error").set_text(i18n::text(e.what()));
    }
}
