#include "level_up_lab_view.h"
#include "game_resources.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include "level_up_dialog.h"
#include "localization.h"
#include "opengold/srd5.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

using namespace godot;
using presentation::required_node;

namespace
{
std::string selected_id(const OptionButton &control,
                        const std::vector<opengold::rules::CreationChoice> &options)
{
    const int index = control.get_selected();
    if (index < 0 || static_cast<std::size_t>(index) >= options.size())
        throw std::runtime_error("Choose a character option");
    return options[static_cast<std::size_t>(index)].id;
}

void fill_choices(const opengold::rules::CharacterRules &rules,
                  opengold::rules::CharacterDraft &draft)
{
    // Ask the rules module for every required choice after each preceding choice.
    // This fixture picks the first valid option, without implementing class rules.
    for (const auto &requested : rules.training_options(draft))
    {
        const auto groups = rules.training_options(draft);
        const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto & group)
        {
            return group.id == requested.id;
        });
        if (found == groups.end() || found->options.size() < found->count)
            throw std::runtime_error("No complete starting training choice is available");
        for (unsigned n = 0; n < found->count; ++n)
            draft.training[found->id].push_back(found->options[n].id);
    }
    const auto cantrips = rules.cantrip_options(draft);
    if (cantrips.count)
    {
        if (cantrips.options.size() < cantrips.count)
            throw std::runtime_error("No complete starting cantrip choice is available");
        draft.cantrips.emplace();
        for (unsigned n = 0; n < cantrips.count; ++n)
            draft.cantrips->push_back(cantrips.options[n].id);
    }
    const auto spells = rules.spell_choice_options(draft);
    if (!spells.may_prepare)
        return;
    draft.spells = opengold::rules::SpellChoices{};
    for (const auto &group : spells.learning)
    {
        if (group.options.size() < group.count)
            throw std::runtime_error("No complete starting spell choice is available");
        for (unsigned n = 0; n < group.count; ++n)
            draft.spells->learning[group.id].push_back(group.options[n].id);
    }
    const auto prepared = rules.spell_choice_options(draft);
    if (prepared.preparation.size() < prepared.prepared_count)
        throw std::runtime_error("No complete starting spell preparation is available");
    draft.spells->prepared.emplace();
    for (unsigned n = 0; n < prepared.prepared_count; ++n)
        draft.spells->prepared->push_back(prepared.preparation[n].id);
}

void add_options(OptionButton &control,
                 const std::vector<opengold::rules::CreationChoice> &options)
{
    control.clear();
    for (const auto &option : options)
    {
        const int index = control.get_item_count();
        control.add_item(i18n::text(option.label));
        control.set_item_metadata(index, String::utf8(option.id.c_str()));
    }
    if (!options.empty())
        control.select(0);
}

String label_for(const std::vector<opengold::rules::CreationChoice> &options,
                 const std::string &id)
{
    const auto found = std::find_if(options.begin(), options.end(), [&](const auto & option)
    {
        return option.id == id;
    });
    return found == options.end() ? String::utf8(id.c_str()) : i18n::text(found->label);
}
} // namespace

void LevelUpLabView::_ready()
{
    i18n::prepare_ui(*this);
    auto rules = opengold::srd5::character_rules();
    classes_ = rules->choices(opengold::rules::CreationField::character_class);
    races_ = rules->choices(opengold::rules::CreationField::race);
    genders_ = rules->choices(opengold::rules::CreationField::gender);
    add_options(required_node<OptionButton>(*this, "Class"), classes_);
    add_options(required_node<OptionButton>(*this, "Race"), races_);
    add_options(required_node<OptionButton>(*this, "Gender"), genders_);
    auto &level = required_node<OptionButton>(*this, "CurrentLevel");
    for (int n = 1; n <= 3; ++n)
        level.add_item(String::num_int64(n));
    level.select(0);
    required_node<Button>(*this, "CreateCharacter")
    .connect("pressed", presentation::guarded(this, &LevelUpLabView::create_character));
    required_node<Button>(*this, "OpenLevelUp")
    .connect("pressed", presentation::guarded(this, &LevelUpLabView::open_level_up));
    auto dialog = LevelUpDialog::create();
    auto *level_up = Object::cast_to<LevelUpDialog>(dialog.get());
    level_up->connect_host([this]() -> opengold::CampaignParty &
    {
        return *campaign_;
    }, [this]
    {
        refresh_result();
    }, [this](const std::exception & failure)
    {
        report_failure(failure);
    });
    presentation::attach_child(*this, std::move(dialog));
    refresh_result();
}

void LevelUpLabView::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "Status").set_text(String::utf8(failure.what()));
}

void LevelUpLabView::create_character()
{
    auto rules = opengold::srd5::character_rules();
    opengold::rules::CharacterDraft draft;
    draft.character_class = selected_id(required_node<OptionButton>(*this, "Class"), classes_);
    draft.race = selected_id(required_node<OptionButton>(*this, "Race"), races_);
    draft.gender = selected_id(required_node<OptionButton>(*this, "Gender"), genders_);
    const auto alignments = rules->choices(opengold::rules::CreationField::alignment);
    const auto backgrounds = rules->choices(opengold::rules::CreationField::background);
    if (alignments.empty() || backgrounds.empty())
        throw std::runtime_error("Rules have no starting alignment or background");
    draft.alignment = alignments.front().id;
    draft.background = backgrounds.front().id;
    const auto entered = required_node<LineEdit>(*this, "Name").get_text().strip_edges();
    draft.name = entered.is_empty() ? i18n::text(N_("Review character")).utf8().get_data()
                 : entered.utf8().get_data();
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    fill_choices(*rules, draft);
    auto next = std::make_unique<opengold::CampaignParty>(
                    opengold::srd5::load(presentation::path_from_godot(game_rules_file())));
    const auto id = next->add_pc(opengold::Character(*rules, std::move(draft), {}));
    next->award_experience(2700, "level-up-lab");
    const int target = required_node<OptionButton>(*this, "CurrentLevel").get_selected() + 1;
    for (int level = 1; level < target; ++level)
    {
        if (!next->can_advance(id))
            throw std::runtime_error("This class cannot reach the selected level");
        next->advance(id, next->default_advancement(id));
    }
    campaign_ = std::move(next);
    member_ = id;
    refresh_result();
}

void LevelUpLabView::open_level_up()
{
    if (!campaign_ || !member_ || !campaign_->can_advance(member_))
        throw std::runtime_error("Create a character ready to level up first");
    required_node<LevelUpDialog>(*this, "LevelUp").open(member_);
}

void LevelUpLabView::refresh_result()
{
    auto &button = required_node<Button>(*this, "OpenLevelUp");
    button.set_disabled(!campaign_ || !member_ || !campaign_->can_advance(member_));
    auto &summary = required_node<Label>(*this, "Summary");
    auto &status = required_node<Label>(*this, "Status");
    if (!campaign_ || !member_)
    {
        summary.set_text(i18n::text(N_("Choose a character and press Create Character.")));
        status.set_text(i18n::text(N_("No character created yet.")));
        return;
    }
    const auto &member = campaign_->member(member_);
    const auto &sheet = member.character.sheet();
    summary.set_text(
        i18n::format("{name} / {class} / {race} / {gender}\nLevel {level} · HP {current}/{maximum} · XP {xp}",
    {
        {"name", String::utf8(sheet.name.c_str())},
        {"class", label_for(classes_, sheet.character_class)},
        {"race", label_for(races_, sheet.race)},
        {"gender", label_for(genders_, sheet.gender)},
        {"level", sheet.level},
        {"current", member.vitals.hit_points},
        {"maximum", sheet.hit_points},
        {"xp", member.experience}
    }));
    status.set_text(button.is_disabled()
                    ? i18n::text(N_("No further level-up is offered for this character."))
                    : i18n::text(N_("Ready: Open Level-Up uses the game's real dialog and rules.")));
}
