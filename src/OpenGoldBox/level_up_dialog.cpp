#include "character_text.h"
#include "godot_text.h"
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
using presentation::gs;

} // namespace

presentation::NodeOwner<> LevelUpDialog::create()
{
    auto scene = presentation::instantiate_scene("res://scenes/level_up_dialog.tscn");
    auto *dialog = Object::cast_to<LevelUpDialog>(scene.get());
    if (!dialog)
    {
        throw std::runtime_error("Invalid level-up dialog scene");
    }
    dialog->set_title(i18n::text(N_("Level up")));
    dialog->bind_controls();
    return scene;
}

void LevelUpDialog::connect_host(CampaignAccess campaign, std::function<void()> advanced,
                                 FailureReport report)
{
    if (!campaign || !advanced || !report)
    {
        throw std::logic_error("Level-up dialog needs a campaign, a follow-up and a report");
    }
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
    required_node<Label>(*this, "FeatLabel").set_text(i18n::text(N_("Feat or ability points")));
    auto *feat = &required_node<OptionButton>(*this, "Feat");
    feat->connect("item_selected",
                  presentation::guarded(this, &LevelUpDialog::advancement_changed));
    auto *training = &required_node<OptionButton>(*this, "AdvancementTraining");
    training->connect("item_selected",
                      presentation::guarded(this, &LevelUpDialog::advancement_changed));
    for (unsigned i = 0; i < 6; ++i)
    {
        required_node<Label>(*this, String("AbilityLabel") + String::num_uint64(i))
            .set_text(i18n::text(i18n::ability_short[i]));
        auto *points =
            &required_node<OptionButton>(*this, String("Ability") + String::num_uint64(i));
        for (int n = 0; n <= 2; ++n)
        {
            points->add_item(String("+") + String::num_int64(n));
        }
        points->connect("item_selected",
                        presentation::guarded(this, &LevelUpDialog::advancement_changed));
    }
    required_node<Label>(*this, "SpellLabel").set_text(i18n::text(N_("Prepared spells")));
    auto *style = &required_node<OptionButton>(*this, "FightingStyle");
    style->connect("item_selected",
                   presentation::guarded(this, &LevelUpDialog::advancement_changed));
    for (int i = 0; i < 4; ++i)
    {
        auto *spell = &required_node<CheckBox>(*this, String("Spell") + String::num_int64(i));
        spell->connect(
            "toggled",
            presentation::guarded(this, &LevelUpDialog::advancement_spell_changed).bind(i));
    }
    for (const char *name : {"SwapCantrip", "SwapCantripWith", "SwapSpell", "SwapSpellWith",
                             "SwapOption", "SwapOptionWith", "SwapDiscovery", "SwapDiscoveryWith"})
    {
        required_node<OptionButton>(*this, String("SwapPage/") + name)
            .connect("item_selected",
                     presentation::guarded(this, &LevelUpDialog::advancement_swap_changed));
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

namespace
{
// Skilled's three proficiencies, a Sorcerer's two Metamagic options and a
// Warlock's two invocations are checked on their own page; a single pick is a
// dropdown on the first page.
bool picks_several(const opengold::rules::TrainingChoiceGroup &group)
{
    return group.id == "feat:skilled" || group.count > 1;
}
} // namespace

std::vector<const opengold::rules::TrainingChoiceGroup *>
LevelUpDialog::advancement_skilled_groups() const
{
    std::vector<const opengold::rules::TrainingChoiceGroup *> groups;
    for (const auto &group : advancement_options_.training)
    {
        if (picks_several(group))
        {
            groups.push_back(&group);
        }
    }
    return groups;
}

const opengold::rules::TrainingChoiceGroup *LevelUpDialog::advancement_skilled_group() const
{
    const auto groups = advancement_skilled_groups();
    if (groups.empty())
    {
        return nullptr;
    }
    return groups[std::min(skilled_index_, groups.size() - 1)];
}

const opengold::rules::TrainingChoiceGroup *LevelUpDialog::advancement_dropdown_group() const
{
    const auto found =
        std::find_if(advancement_options_.training.begin(), advancement_options_.training.end(),
                     [](const auto &g)
                     {
                         return !picks_several(g);
                     });
    return found == advancement_options_.training.end() ? nullptr : &*found;
}

std::vector<LevelUpDialog::AdvancementStep> LevelUpDialog::advancement_steps() const
{
    // Lessons of the First Ones' Skilled taken as a replacement asks for its
    // skills after the Replace page that chose it.
    const auto after_swaps = [&](const opengold::rules::TrainingChoiceGroup &group)
    {
        return group.id == "invocation:lessons_skilled" &&
               advancement_choice_.option_swap.replacement == "lessons_skilled";
    };
    const auto groups = advancement_skilled_groups();
    std::vector<AdvancementStep> steps{{AdvancementPage::choices, 0}};
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        if (!after_swaps(*groups[i]))
        {
            steps.push_back({AdvancementPage::skilled, i});
        }
    }
    if (has_swaps())
    {
        steps.push_back({AdvancementPage::swaps, 0});
    }
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        if (after_swaps(*groups[i]))
        {
            steps.push_back({AdvancementPage::skilled, i});
        }
    }
    if (advancement_choice_.spell_learning)
    {
        steps.push_back({AdvancementPage::spells, 0});
    }
    return steps;
}

LevelUpDialog::AdvancementStep LevelUpDialog::current_step() const
{
    return {advancement_page_, advancement_page_ == AdvancementPage::skilled ? skilled_index_ : 0};
}

LevelUpDialog::AdvancementStep LevelUpDialog::advancement_next_step() const
{
    const auto steps = advancement_steps();
    const auto at = std::find(steps.begin(), steps.end(), current_step());
    if (at == steps.end())
    {
        return steps.front();
    }
    return at + 1 == steps.end() ? *at : *(at + 1);
}

LevelUpDialog::AdvancementStep LevelUpDialog::advancement_previous_step() const
{
    const auto steps = advancement_steps();
    const auto at = std::find(steps.begin(), steps.end(), current_step());
    return at == steps.end() || at == steps.begin() ? steps.front() : *(at - 1);
}

void LevelUpDialog::show_step(AdvancementStep step)
{
    advancement_page_ = step.page;
    skilled_index_ = step.index;
    advancement_pages();
    advancement_changed();
}

bool LevelUpDialog::has_swaps() const
{
    const auto &o = advancement_options_;
    const std::array offers{&o.cantrip_swap, &o.spell_swap, &o.option_swap, &o.discovery_swap};
    return std::any_of(offers.begin(), offers.end(),
                       [](const auto *offer)
                       {
                           return !offer->replaceable.empty() && !offer->replacements.empty();
                       });
}

namespace
{
// What the row asks the player to replace, by the offer's kind.
String swap_question(const std::string &kind)
{
    return kind == "Cantrip"               ? i18n::text(N_("Replace a cantrip"))
           : kind == "Spell"               ? i18n::text(N_("Replace a spell"))
           : kind == "Metamagic"           ? i18n::text(N_("Replace a Metamagic option"))
           : kind == "Magical Discoveries" ? i18n::text(N_("Replace a Magical Discoveries spell"))
                                           : i18n::text(N_("Replace an Eldritch Invocation"));
}
} // namespace

void LevelUpDialog::refresh_swaps()
{
    // The invocations picked at this level are not offered again.
    const auto picked = advancement_choice_.training.find("class:warlock:invocations:2");
    const auto chosen = [&](const std::string &id)
    {
        return picked != advancement_choice_.training.end() &&
               std::find(picked->second.begin(), picked->second.end(), id) != picked->second.end();
    };
    const auto row = [&](const char *name, const opengold::rules::SwapOffer &offer,
                         const opengold::rules::LevelSwap &swap)
    {
        const String base(name);
        const bool shown = !offer.replaceable.empty() && !offer.replacements.empty();
        for (const auto &suffix : {String("Label"), String(), String("WithLabel"), String("With")})
        {
            required_node<Control>(*this, String("SwapPage/") + base + suffix).set_visible(shown);
        }
        if (!shown)
        {
            return;
        }
        required_node<Label>(*this, String("SwapPage/") + base + String("Label"))
            .set_text(swap_question(offer.label));
        required_node<Label>(*this, String("SwapPage/") + base + String("WithLabel"))
            .set_text(i18n::text(N_("With")));
        auto &replaced = required_node<OptionButton>(*this, String("SwapPage/") + base);
        replaced.clear();
        replaced.add_item(i18n::text(N_("Keep current")));
        replaced.set_item_metadata(0, String());
        for (const auto &o : offer.replaceable)
        {
            replaced.add_item(i18n::text(o.label));
            replaced.set_item_metadata(replaced.get_item_count() - 1, String::utf8(o.id.c_str()));
            if (o.id == swap.replaced)
            {
                replaced.select(replaced.get_item_count() - 1);
            }
        }
        if (swap.replaced.empty())
        {
            replaced.select(0);
        }
        auto &replacement =
            required_node<OptionButton>(*this, String("SwapPage/") + base + String("With"));
        replacement.clear();
        replacement.add_item(i18n::text(N_("Choose an option")));
        replacement.set_item_metadata(0, String());
        replacement.select(0);
        for (const auto &o : offer.replacements)
        {
            if (chosen(o.id))
            {
                continue;
            }
            replacement.add_item(
                i18n::text(o.label) +
                (o.description.empty() ? String() : String(" / ") + i18n::text(o.description)));
            replacement.set_item_metadata(replacement.get_item_count() - 1,
                                          String::utf8(o.id.c_str()));
            if (o.id == swap.replacement)
            {
                replacement.select(replacement.get_item_count() - 1);
            }
        }
        replacement.set_disabled(swap.replaced.empty());
    };
    const presentation::ScopedFlag refreshing(advancement_refreshing_);
    row("SwapCantrip", advancement_options_.cantrip_swap, swap_rows_[0]);
    row("SwapSpell", advancement_options_.spell_swap, swap_rows_[1]);
    row("SwapOption", advancement_options_.option_swap, swap_rows_[2]);
    row("SwapDiscovery", advancement_options_.discovery_swap, swap_rows_[3]);
}

void LevelUpDialog::advancement_swap_changed(std::int64_t)
{
    if (advancement_refreshing_)
    {
        return;
    }
    static constexpr std::array<const char *, 4> names{"SwapCantrip", "SwapSpell", "SwapOption",
                                                       "SwapDiscovery"};
    const auto value = [&](const String &node)
    {
        auto &dropdown = required_node<OptionButton>(*this, String("SwapPage/") + node);
        const auto index = dropdown.get_selected();
        return index <= 0
                   ? std::string()
                   : std::string(String(dropdown.get_item_metadata(index)).utf8().get_data());
    };
    swap_incomplete_ = false;
    std::array<opengold::rules::LevelSwap, 4> complete{};
    for (std::size_t n = 0; n < names.size(); ++n)
    {
        const String base(names[n]);
        auto &row = swap_rows_[n];
        const auto replaced = value(base);
        // Another cantrip to replace starts its replacement over.
        row.replacement = replaced == row.replaced ? value(base + String("With")) : std::string();
        row.replaced = replaced;
        if (row.replaced.empty())
        {
            row.replacement.clear();
        }
        swap_incomplete_ |= !row.replaced.empty() && row.replacement.empty();
        if (!row.replacement.empty())
        {
            complete[n] = row;
        }
    }
    // A spell swap changes the list the level's spell choices start from.
    const auto before = advancement_choice_.spell_swap;
    advancement_choice_.cantrip_swap = complete[0];
    advancement_choice_.spell_swap = complete[1];
    advancement_choice_.option_swap = complete[2];
    advancement_choice_.discovery_swap = complete[3];
    auto &spells = advancement_choice_.spells;
    if (!before.replacement.empty())
    {
        std::replace(spells.begin(), spells.end(), before.replacement, before.replaced);
    }
    if (const auto &now = advancement_choice_.spell_swap; !now.replacement.empty())
    {
        std::erase(spells, now.replacement);
        std::replace(spells.begin(), spells.end(), now.replaced, now.replacement);
    }
    advancement_changed();
}

void LevelUpDialog::advancement_pages()
{
    if (!advancing_)
    {
        return;
    }
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
    required_node<Control>(*this, "SwapPage")
        .set_visible(advancement_page_ == AdvancementPage::swaps);
    if (advancement_page_ == AdvancementPage::swaps)
    {
        refresh_swaps();
    }
    // Back appears on any page after the first rather than only a Wizard's
    // spell page, so a non-Wizard taking Skilled can still step back.
    required_node<Button>(*this, "Back").set_visible(!on_first);
    required_node<Button>(*this, "Confirm")
        .set_text(advancement_next_step() == current_step() ? i18n::text(N_("Confirm"))
                                                            : i18n::text(N_("Next")));
    for (const char *name : {"HP", "FeatLabel", "Feat", "Note"})
    {
        required_node<Control>(*this, name).set_visible(on_first);
    }
    // Match open_advancement exactly: a dropdown alongside a feat choice is
    // supplemental, and then the ability controls stay visible beside it.
    const bool has_training = advancement_dropdown_group() != nullptr;
    const bool supplemental = has_training && !advancement_options_.feats.empty();
    for (const char *name : {"AdvancementTrainingLabel", "AdvancementTraining"})
    {
        required_node<Control>(*this, name).set_visible(on_first && has_training);
    }
    for (unsigned i = 0; i < 6; ++i)
    {
        required_node<Control>(*this, String("Ability") + String::num_uint64(i))
            .set_visible(on_first && (!has_training || supplemental));
        required_node<Control>(*this, String("AbilityLabel") + String::num_uint64(i))
            .set_visible(on_first && (!has_training || supplemental));
    }
    if (advancement_page_ == AdvancementPage::skilled)
    {
        refresh_advancement_skilled();
    }
    if (!spell_page)
    {
        required_node<Control>(*this, "SpellLabel").set_visible(on_first);
        for (unsigned i = 0; i < 4; ++i)
        {
            required_node<Control>(*this, String("Spell") + String::num_uint64(i))
                .set_visible(on_first && i < advancement_options_.spells.size());
        }
        return;
    }
    required_node<Control>(*this, "SpellLabel").hide();
    for (unsigned i = 0; i < 4; ++i)
    {
        required_node<Control>(*this, String("Spell") + String::num_uint64(i)).hide();
    }
    if (advancement_page_ != AdvancementPage::spells)
    {
        return;
    }
    auto sheet = campaign().rule_module().spell_choice_sheet(
        campaign().member(advancing_).character.sheet(), advancement_choice_);
    auto options = campaign().rule_module().spell_choice_options(
        sheet, opengold::rules::SpellChoiceContext::advancement);
    opengold::rules::SpellChoices choices{
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    auto learning = choices;
    learning.prepared.reset();
    try
    {
        campaign().rule_module().apply_spell_choices(
            sheet, learning, opengold::rules::SpellChoiceContext::advancement,
            opengold::rules::ChoiceCompleteness::partial);
        options.preparation =
            campaign()
                .rule_module()
                .spell_choice_options(sheet, opengold::rules::SpellChoiceContext::advancement)
                .preparation;
    }
    catch (const std::exception &)
    {
    } // Invalid edits remain visible; final preview reports the error.
    presentation::refresh_spell_groups(
        required_node<VBoxContainer>(*this, "SpellChoicesPage/Rows"), options, choices,
        presentation::guarded(this, &LevelUpDialog::advancement_learning_toggled), i18n::text,
        true);
}

// One checkbox per offered proficiency, prefixed so the two catalogs stay
// distinguishable in a single list. Controls are reused across refreshes so
// toggling never destroys the focused checkbox.
void LevelUpDialog::refresh_advancement_skilled()
{
    const auto *group = advancement_skilled_group();
    if (!group)
    {
        return;
    }
    auto &rows = required_node<VBoxContainer>(*this, "SkilledPage/Rows");
    const auto &picked = advancement_choice_.training[group->id];
    auto &count = required_node<Label>(*this, "SkilledCount");
    if (group->id == "feat:skilled")
    {
        count.set_text(i18n::format(
            "Selected: {count}/{total}",
            {{"count", static_cast<unsigned>(picked.size())}, {"total", group->count}}));
    }
    else
    {
        count.set_text(i18n::format("{choice}: {count}/{total} selected",
                                    {{"choice", i18n::text(group->label)},
                                     {"count", static_cast<unsigned>(picked.size())},
                                     {"total", group->count}}));
    }
    // Hide every row first; the loop below shows only what is still offered, so
    // no reverse name mapping is needed (ids contain underscores of their own).
    for (int n = 0; n < rows.get_child_count(); ++n)
    {
        if (auto *box = Object::cast_to<CheckBox>(rows.get_child(n)))
        {
            box->hide();
        }
    }
    for (const auto &option : group->options)
    {
        const auto name = presentation::training_string(option.id).replace(":", "_");
        auto *box = Object::cast_to<CheckBox>(rows.get_node_or_null(name));
        if (!box)
        {
            auto owned = presentation::instantiate_control<CheckBox>(
                "res://scenes/control_templates/skilled_choice.tscn");
            owned->set_name(name);
            box = owned.get();
            box->connect("toggled",
                         presentation::guarded(this, &LevelUpDialog::advancement_skilled_toggled)
                             .bind(presentation::training_string(option.id)));
            presentation::attach_child(rows, std::move(owned));
        }
        // Each template has to sit directly inside the i18n::format call: the
        // catalog extractor matches on that call, so a literal behind a ternary
        // is never collected and would ship untranslated.
        if (group->id != "feat:skilled")
        {
            box->set_text(i18n::text(option.label) +
                          (option.description.empty()
                               ? String()
                               : String(" / ") + i18n::text(option.description)));
        }
        else
        {
            box->set_text(option.id.starts_with("skill:")
                              ? i18n::format("Skill: {name}", {{"name", i18n::text(option.label)}})
                              : i18n::format("Tool: {name}", {{"name", i18n::text(option.label)}}));
        }
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
    const auto *group = advancement_skilled_group();
    if (!group)
    {
        return;
    }
    auto &picked = advancement_choice_.training[group->id];
    if (selected)
    {
        if (std::find(picked.begin(), picked.end(), id) == picked.end() &&
            picked.size() < group->count)
        {
            picked.push_back(id);
        }
    }
    else
    {
        std::erase(picked, id);
    }
    refresh_advancement_skilled();
    advancement_changed();
}

void LevelUpDialog::advancement_back()
{
    show_step(advancement_previous_step());
}

void LevelUpDialog::advancement_learning_toggled(bool selected, String group, String option)
{
    opengold::rules::SpellChoices choices{
        *advancement_choice_.spell_learning, advancement_choice_.spells, {}, {}};
    presentation::toggle_spell(choices, {.group = group.utf8().get_data(),
                                         .option = option.utf8().get_data(),
                                         .selected = selected});
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
    skilled_index_ = 0;
    advancing_ = id;
    advancement_options_ = campaign().advancement_options(id);
    advancement_choice_ = campaign().default_advancement(id);
    swap_rows_ = {};
    swap_incomplete_ = false;
    // Several picks start empty, as the dropdown starts at "Choose an option".
    for (const auto *group : advancement_skilled_groups())
    {
        advancement_choice_.training.erase(group->id);
    }
    {
        const presentation::ScopedFlag refreshing(advancement_refreshing_);
        for (const char *name : {"HP", "FeatLabel", "Feat", "Note", "SpellLabel"})
        {
            required_node<Control>(*this, name).show();
        }
        required_node<Label>(*this, "Title")
            .set_text(i18n::format("{name} / Level {level}",
                                   {{"name", gs(campaign().member(id).character.sheet().name)},
                                    {"level", advancement_options_.level}}));
        required_node<Label>(*this, "Note").set_text(i18n::text(advancement_options_.description));
        auto *feat = &required_node<OptionButton>(*this, "Feat");
        feat->clear();
        if (advancement_options_.feats.empty())
        {
            feat->add_item(i18n::text(N_("No feat or ability increase at this level")));
        }
        for (unsigned i = 0; i < advancement_options_.feats.size(); ++i)
        {
            const auto &option = advancement_options_.feats[i];
            feat->add_item(i18n::text(option.label) +
                           (option.available ? String() : i18n::text(" (Unavailable)")));
            feat->set_item_disabled(i, !option.available);
            feat->set_item_tooltip(i, i18n::text(option.description));
            if (option.id == advancement_choice_.feat)
            {
                feat->select(i);
            }
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
            {
                training->add_item(i18n::text(option.label) +
                                   (option.description.empty()
                                        ? String()
                                        : String(" / ") + i18n::text(option.description)));
            }
            training->select(0);
            advancement_choice_.training.clear();
        }
        for (unsigned i = 0; i < 6; ++i)
        {
            required_node<OptionButton>(*this, String("Ability") + String::num_uint64(i))
                .select(advancement_choice_.abilities[opengold::rules::all_abilities[i]]);
        }
        required_node<Label>(*this, "SpellLabel")
            .set_text(i18n::text(advancement_options_.spells.empty()
                                     ? N_("No spell choices for this class")
                                     : N_("Prepared spells: select at least one")));
        for (unsigned i = 0; i < 4; ++i)
        {
            auto *spell = &required_node<CheckBox>(*this, String("Spell") + String::num_uint64(i));
            spell->set_visible(i < advancement_options_.spells.size());
            if (i >= advancement_options_.spells.size())
            {
                continue;
            }
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
                {
                    style->select(i);
                }
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

// Spells learned through a Fighting Style (Blessed Warrior's cantrips) or a
// feat (Magic Initiate) leave with it, so a changed choice drops picks its
// learning group no longer offers. A class without spells of its own has a
// spell page only while such a choice offers some.
void LevelUpDialog::drop_unoffered_learning()
{
    const auto &rules = campaign().rule_module();
    const auto offered =
        rules
            .spell_choice_options(
                rules.spell_choice_sheet(campaign().member(advancing_).character.sheet(),
                                         advancement_choice_),
                opengold::rules::SpellChoiceContext::advancement)
            .learning;
    if (!campaign().default_advancement(advancing_).spell_learning)
    {
        if (offered.empty())
        {
            advancement_choice_.spell_learning.reset();
        }
        else if (!advancement_choice_.spell_learning)
        {
            advancement_choice_.spell_learning.emplace();
        }
    }
    if (!advancement_choice_.spell_learning)
    {
        return;
    }
    std::erase_if(*advancement_choice_.spell_learning,
                  [&](const auto &entry)
                  {
                      return std::none_of(offered.begin(), offered.end(),
                                          [&](const auto &group)
                                          {
                                              return group.id == entry.first;
                                          });
                  });
}

void LevelUpDialog::advancement_changed(std::int64_t)
{
    if (advancement_refreshing_ || !advancing_)
    {
        return;
    }
    if (!advancement_options_.fighting_styles.empty())
    {
        const auto index = required_node<OptionButton>(*this, "FightingStyle").get_selected();
        if (index >= 0)
        {
            const auto &option = advancement_options_.fighting_styles.at(index);
            if (option.id == "keep")
            {
                advancement_choice_.fighting_style.reset();
            }
            else
            {
                advancement_choice_.fighting_style = option.id;
            }
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
            advancement_options_.feats.at(required_node<OptionButton>(*this, "Feat").get_selected())
                .id;
        // The Skilled page is offered by the rules only while Skilled is the
        // selection, so the options have to follow the feat for every class, not
        // just the ones with a Fighting Style.
        advancement_options_ = campaign().advancement_options(advancing_, advancement_choice_);
    }
    // Keep the checkbox page's picks: they come from their own page, not this
    // dropdown, and switching away from Skilled drops them by dropping the group.
    auto previous = std::move(advancement_choice_.training);
    advancement_choice_.training.clear();
    if (const auto *group = advancement_dropdown_group())
    {
        const auto index = required_node<OptionButton>(*this, "AdvancementTraining").get_selected();
        if (index > 0 && static_cast<std::size_t>(index) <= group->options.size())
        {
            advancement_choice_.training[group->id] = {group->options[index - 1].id};
        }
    }
    for (const auto *group : advancement_skilled_groups())
    {
        advancement_choice_.training[group->id] = std::move(previous[group->id]);
    }
    // An invocation picked or swapped in can bring a choice of its own
    // (Lessons of the First Ones' Skilled), so the options follow the picks.
    advancement_options_ = campaign().advancement_options(advancing_, advancement_choice_);
    for (const auto *group : advancement_skilled_groups())
    {
        if (!advancement_choice_.training.contains(group->id))
        {
            advancement_choice_.training[group->id] = std::move(previous[group->id]);
        }
    }
    // The feat and Lessons of the First Ones can each bring Magic Initiate's spells.
    drop_unoffered_learning();
    const bool ability = advancement_choice_.feat == "ability_score_improvement";
    for (unsigned i = 0; i < 6; ++i)
    {
        auto *points =
            &required_node<OptionButton>(*this, String("Ability") + String::num_uint64(i));
        points->set_disabled(!ability);
        if (!ability)
        {
            points->select(0);
        }
        const auto shown = opengold::rules::all_abilities[i];
        advancement_choice_.abilities[shown] = ability ? points->get_selected() : 0;
        const auto value = campaign().member(advancing_).character.sheet().scores[shown];
        required_node<Label>(*this, String("AbilityLabel") + String::num_uint64(i))
            .set_text(i18n::text(i18n::ability_short[i]) + " " + String::num_int64(value) +
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
        {
            if (required_node<CheckBox>(*this, String("Spell") + String::num_uint64(i))
                    .is_pressed())
            {
                advancement_choice_.spells.push_back(advancement_options_.spells[i].id);
            }
        }
    }
    auto preview_choice = advancement_choice_;
    // The HP preview and the Next button both come from previewing the whole
    // transaction, so a page the player has not reached yet must preview with a
    // valid stand-in rather than blocking the way to it. Once the Skilled page is
    // showing, the real picks apply and an incomplete three gates Confirm.
    const auto steps = advancement_steps();
    const auto now = std::find(steps.begin(), steps.end(), current_step());
    for (auto later = now == steps.end() ? steps.end() : now + 1; later != steps.end(); ++later)
    {
        if (later->page == AdvancementPage::skilled)
        {
            const auto *group = advancement_skilled_groups()[later->index];
            auto &fill = preview_choice.training[group->id];
            if (fill.size() != group->count)
            {
                fill.clear();
                for (const auto &option : group->options)
                {
                    if (fill.size() == group->count)
                    {
                        break;
                    }
                    fill.push_back(option.id);
                }
            }
        }
    }
    if (preview_choice.spell_learning && advancement_page_ != AdvancementPage::spells)
    {
        const auto defaults = campaign().default_advancement(advancing_);
        preview_choice.spell_learning =
            defaults.spell_learning.value_or(opengold::rules::TrainingChoices{});
        preview_choice.spells = defaults.spells;
        // The defaults follow the default Fighting Style; a chosen style's own
        // learning (Blessed Warrior's cantrips) previews with its first options.
        const auto &rules = campaign().rule_module();
        const auto offered = rules.spell_choice_options(
            rules.spell_choice_sheet(campaign().member(advancing_).character.sheet(),
                                     preview_choice),
            opengold::rules::SpellChoiceContext::advancement);
        // Two groups can offer one spell (a Wizard's spellbook and Magic
        // Initiate), which a stand-in must not learn twice.
        const auto picked = [&](const std::string &id)
        {
            return std::any_of(preview_choice.spell_learning->begin(),
                               preview_choice.spell_learning->end(),
                               [&](const auto &entry)
                               {
                                   return std::find(entry.second.begin(), entry.second.end(), id) !=
                                          entry.second.end();
                               });
        };
        for (const auto &group : offered.learning)
        {
            auto &fill = (*preview_choice.spell_learning)[group.id];
            for (const auto &option : group.options)
            {
                if (fill.size() < group.count && !picked(option.id))
                {
                    fill.push_back(option.id);
                }
            }
        }
    }
    try
    {
        const auto preview = campaign().preview_advancement(advancing_, preview_choice);
        const auto &old = campaign().member(advancing_);
        required_node<Label>(*this, "HP")
            .set_text(i18n::format("Maximum HP: {old} -> {new} / Current HP: {current}",
                                   {{"old", old.character.sheet().hit_points},
                                    {"new", preview.character.sheet().hit_points},
                                    {"current", preview.vitals.hit_points}}));
        required_node<Label>(*this, "Error")
            .set_text(swap_incomplete_ ? i18n::text(N_("Choose what replaces it, or keep current."))
                                       : String());
        required_node<Button>(*this, "Confirm").set_disabled(swap_incomplete_);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "HP")
            .set_text(i18n::text(N_("Choose valid options to preview your new HP.")));
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
    {
        return;
    }
    // Confirm doubles as Next until the last page; only then does it apply the
    // whole advancement in one transaction.
    if (const auto next = advancement_next_step(); next != current_step())
    {
        show_step(next);
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
