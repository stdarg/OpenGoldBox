// The Buck Rogers games' pages of the creation screen (docs/CHARACTER-CREATION.md,
// "The Buck Rogers games"): the same screen and creator as every game's, with
// the pages where these games' rules differ. The character is its record
// (opengold/buck_rogers_creation.h); every number shown comes from the rules.
#include "buck_rogers_icons.h"
#include "game_lifecycle.h"
#include "character_creation_view.h"
#include "character_text.h"
#include "game_resources.h"
#include "godot_nodes.h"
#include "godot_text.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "opengold/buck_rogers_creation.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "opengold/rolf_tour.h"
#include "rolf_tour_view.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/button_group.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace godot;
using opengold::CreationStep;
using opengold::por::RolfTourSession;
using presentation::gs;
using presentation::required_node;

namespace
{
namespace og = opengold::original;
using og::Skill;

constexpr const char *tech_label = N_("Tech");

godot::String ability_label(std::size_t ability)
{
    return i18n::text(ability < i18n::ability_names.size() ? i18n::ability_names[ability]
                                                           : tech_label);
}

// The skill screen's tabs (skills.pas BuildSkillLists): the career's skills,
// the general skills by group in the screen's order, then all of them, which
// the original lists under the empty Constitution group.
struct SkillTab
{
    const char *label;
    std::optional<og::SkillGroup> group;
    bool career{};
};

const std::array<SkillTab, 7> skill_tabs{{{N_("Career skills"), std::nullopt, true},
                                          {N_("Technical"), og::SkillGroup::technical},
                                          {N_("Intelligence"), og::SkillGroup::intelligence},
                                          {N_("Dexterity"), og::SkillGroup::dexterity},
                                          {N_("Charisma"), og::SkillGroup::charisma},
                                          {N_("Wisdom"), og::SkillGroup::wisdom},
                                          {N_("All general"), std::nullopt}}};

godot::String skill_name(Skill skill)
{
    return i18n::text(og::skill_definition(skill).name);
}

og::GroundRecord record_of(const opengold::rules::CharacterDraft &draft)
{
    auto record = og::buck_rogers_record_of(draft.rules_data);
    if (!record)
    {
        throw std::logic_error("A Buck Rogers draft holds its record once rolled");
    }
    return std::move(*record);
}

// The roll a record holds, without what the later pages change: another
// roll, or another character, starts the skill screen over.
std::string roll_of(std::string_view rules_data)
{
    auto record = og::buck_rogers_record_of(rules_data);
    if (!record)
    {
        return {};
    }
    auto &creature = record->creature;
    creature.name.clear();
    creature.skills = {};
    creature.specializations = {};
    creature.icon = 0;
    return og::encode_ground_record(*record);
}

// Rows built again: the old ones leave the tree at once, so the new ones
// take their names, and are freed once their signals are done.
void clear_rows(Node &rows)
{
    while (rows.get_child_count() > 0)
    {
        // The pressed button may be among them: Godot frees it later.
        presentation::detach_child(rows, *rows.get_child(0)).release()->queue_free();
    }
}

template <class T> T *add_row_node(Node &rows, const String &name)
{
    auto *node = presentation::attach_child(rows, presentation::make_node<T>());
    node->set_name(name);
    return node;
}

Label *add_label(Node &rows, const String &name, const String &text)
{
    auto *label = add_row_node<Label>(rows, name);
    label->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    label->set_text(text);
    return label;
}

Button *add_button(Node &rows, const String &name, const String &text)
{
    auto *button = add_row_node<Button>(rows, name);
    button->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
    button->set_text(text);
    button->set_custom_minimum_size(Vector2(36, 28));
    return button;
}

void color(Control &control, const Control &theme_owner, const char *palette_color)
{
    const auto shade = theme_owner.get_theme_color(palette_color, "OpenGoldPalette");
    for (const char *state : {"font_color", "font_pressed_color", "font_hover_color",
                              "font_hover_pressed_color", "font_focus_color"})
    {
        control.add_theme_color_override(state, shade);
    }
}

// The least a career takes of an ability; 1, the least any score is, is no
// minimum.
bool has_minimum(int minimum)
{
    return minimum > 1;
}

// "+2" for an adjustment, nothing for none.
String adjustment_text(int adjustment)
{
    return adjustment == 0 ? String()
                           : gs((adjustment > 0 ? "+" : "") + std::to_string(adjustment));
}

// "Strength +2, Dexterity +2, Charisma -1" (or the minimums "Strength 10,
// Constitution 12"), or "none".
String adjustments_text(const og::BuckRogersScores &adjustments, bool signed_values)
{
    String listed;
    for (std::size_t ability = 0; ability < adjustments.size(); ++ability)
    {
        if (signed_values ? adjustments[ability] == 0 : !has_minimum(adjustments[ability]))
        {
            continue;
        }
        listed += (listed.is_empty() ? String() : String(", ")) + ability_label(ability) + " " +
                  (signed_values ? adjustment_text(adjustments[ability])
                                 : gs(std::to_string(adjustments[ability])));
    }
    return listed.is_empty() ? i18n::text(N_("none")) : listed;
}

// Why the skill screen would refuse the next point, as the row says it.
String refusal_note(og::SkillRefusal refusal, int max_per_skill)
{
    switch (refusal)
    {
    case og::SkillRefusal::prerequisites_unmet:
        return i18n::text(N_("locked"));
    case og::SkillRefusal::at_level_maximum:
        return i18n::format(N_("capped at {points}"), {{"points", max_per_skill}});
    case og::SkillRefusal::at_general_maximum:
        return i18n::text(N_("at the most a general skill takes, 80"));
    case og::SkillRefusal::no_new_skills:
        return i18n::text(N_("no more new general skills"));
    case og::SkillRefusal::none:
    case og::SkillRefusal::no_points:
    case og::SkillRefusal::not_listed:
        break;
    }
    return {};
}
} // namespace

bool CharacterCreationView::buck_rogers() const
{
    return creator_ && opengold::is_buck_rogers(og::game_of(creator_->rules().identity()));
}

// The attributes table's rows and the page's handlers, made once.
void CharacterCreationView::setup_buck_rogers()
{
    auto &table = required_node<GridContainer>(*this, "BuckRogersRoll/Table");
    for (std::size_t ability = 0; ability < og::buck_rogers_ability_count; ++ability)
    {
        const auto n = std::to_string(ability);
        add_label(table, gs("Ability" + n), ability_label(ability));
        add_label(table, gs("Score" + n), {})
            ->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        add_label(table, gs("Race" + n), {})->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        add_label(table, gs("Minimum" + n), {})
            ->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    }
    required_node<Button>(*this, "BuckRogersRoll/RollRow/Roll")
        .connect("pressed", presentation::guarded(this, &CharacterCreationView::roll));
    // The groups as tabs, where the original pages through them with Prev
    // and Next (the user's choice, 2026-11-20).
    auto &tabs = required_node<HBoxContainer>(*this, "Skills/Groups");
    const Ref<ButtonGroup> group(memnew(ButtonGroup));
    for (std::size_t tab = 0; tab < skill_tabs.size(); ++tab)
    {
        auto *button =
            add_button(tabs, gs("Tab" + std::to_string(tab)), i18n::text(skill_tabs[tab].label));
        button->set_toggle_mode(true);
        button->set_button_group(group);
        button->connect("pressed",
                        presentation::guarded(this, &CharacterCreationView::skill_tab_selected)
                            .bind(static_cast<std::int64_t>(tab)));
    }
    // A ConfirmationDialog, reached as a Window: the game's Godot class list
    // leaves that class out.
    required_node<Window>(*this, "UnspentSkills")
        .connect("confirmed", presentation::guarded(this, &CharacterCreationView::leave_skills));
    auto &icons = required_node<GridContainer>(*this, "BodyIcons/Grid");
    for (unsigned icon = 0; icon < presentation::buck_rogers_body_icon_count; ++icon)
    {
        auto *button = add_button(icons, gs("Icon" + std::to_string(icon)), {});
        button->set_toggle_mode(true);
        button->set_custom_minimum_size(Vector2(56, 56));
        button->set_expand_icon(true);
        button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        button->connect("pressed",
                        presentation::guarded(this, &CharacterCreationView::body_icon_chosen)
                            .bind(static_cast<int>(icon)));
    }
}

void CharacterCreationView::refresh_buck_rogers(CreationStep step)
{
    const bool shown = buck_rogers();
    const auto show = [&](const char *node, bool visible)
    {
        required_node<Control>(*this, node).set_visible(shown && visible);
    };
    show("BuckRogersRoll", step == CreationStep::attributes);
    show("Specializations", step == CreationStep::specialization);
    show("Skills", step == CreationStep::skills);
    show("BodyIcons", step == CreationStep::combat_icon);
    if (!shown)
    {
        return;
    }
    keep_spending_current();
    // The original's look: no portraits, colors or parts, only the body icon.
    for (const auto *node :
         {"Roll", "SwapHint", "Columns", "CombatHeadPrevious", "CombatHeadNext", "CombatHeadLabel",
          "BodyPrevious", "BodyNext", "BodyLabel", "Size", "ColorTitle", "Color1Title",
          "Color2Title", "PaletteHint", "ReadyLabel", "ActionLabel"})
    {
        required_node<Control>(*this, node).hide();
    }
    for (int i = 0; i < 6; ++i)
    {
        for (const auto &stem : {"Ability", "Score", "Part"})
        {
            required_node<Control>(*this, gs(std::string(stem) + std::to_string(i))).hide();
        }
        for (int bank = 0; bank < 2; ++bank)
        {
            required_node<Control>(*this,
                                   gs("Color" + std::to_string(bank) + "_" + std::to_string(i)))
                .hide();
        }
    }
    for (int i = 0; i < 16; ++i)
    {
        required_node<Control>(*this, gs("Palette" + std::to_string(i))).hide();
    }
    auto &name = required_node<LineEdit>(*this, "Name");
    name.set_max_length(static_cast<int>(opengold::buck_rogers::Creature::name_capacity));
    required_node<Label>(*this, "Footer")
        .set_text(i18n::text(N_("Game art from the original, rules from the original game")));
    auto &instructions = required_node<Label>(*this, "Instructions");
    switch (step)
    {
    case CreationStep::race:
        instructions.set_text(i18n::text(
            N_("Name your character (up to 15 letters), then choose a race and gender.")));
        break;
    case CreationStep::character_class:
        instructions.set_text(
            i18n::text(N_("Choose a career. Careers in gray are not open to your race.")));
        break;
    case CreationStep::attributes:
        show_buck_rogers_roll();
        break;
    case CreationStep::specialization:
        show_buck_rogers_specializations();
        break;
    case CreationStep::skills:
        show_buck_rogers_skills();
        break;
    case CreationStep::combat_icon:
        show_buck_rogers_icons();
        break;
    case CreationStep::sheet:
        required_node<Control>(*this, "PortraitPreview")
            .call("show_texture",
                  presentation::buck_rogers_body_icon(application_settings(*this),
                                                      record_of(creator_->draft()).creature.icon)
                      .ready);
        break;
    default:
        break;
    }
    if (step == CreationStep::race || step == CreationStep::character_class)
    {
        const auto &d = creator_->draft();
        const auto id = step == CreationStep::race ? d.race : d.character_class;
        required_node<RichTextLabel>(*this, "Description")
            .set_text(buck_rogers_description(step, id));
    }
    const auto *const title = step == CreationStep::character_class  ? N_("Career")
                              : step == CreationStep::skills         ? N_("Skills")
                              : step == CreationStep::specialization ? N_("Weapon specialization")
                              : step == CreationStep::combat_icon    ? N_("Combat icon")
                                                                     : nullptr;
    if (title)
    {
        required_node<Label>(*this, "PageTitle").set_text(i18n::text(title));
    }
}

// What a race or career means in play, read from the rules (our wording).
String CharacterCreationView::buck_rogers_description(CreationStep step,
                                                      const std::string &id) const
{
    const auto game = og::game_of(creator_->rules().identity());
    if (step == CreationStep::race)
    {
        const auto race = og::buck_rogers_race_named(id);
        if (!race)
        {
            return {};
        }
        String careers;
        for (const auto career : og::professions_for(*race, game))
        {
            careers += (careers.is_empty() ? String() : String(", ")) +
                       i18n::text(og::buck_rogers_career_name(career));
        }
        return i18n::format(
            N_("{race}\n\nAbilities: {adjustments}.\nMovement {movement}.\n\nCareers open: {careers}."),
            {{"race", i18n::text(og::buck_rogers_race_name(*race))},
             {"adjustments", adjustments_text(og::race_ability_adjustments(*race), true)},
             {"movement", og::buck_rogers_movement(*race)},
             {"careers", careers}});
    }
    const auto career = og::buck_rogers_career_named(id);
    if (!career)
    {
        return {};
    }
    String skills;
    for (const auto skill : og::career_skills(*career))
    {
        skills += (skills.is_empty() ? String() : String(", ")) + skill_name(skill);
    }
    const int level = og::buck_rogers_starting_level(game);
    return i18n::format(
        N_("{career}\n\nLowest scores: {minimums}. A lower roll is raised to them, never refused.\nCareer skills (career points only): {skills}.\nStarts at level {level} with {experience} experience."),
        {{"career", i18n::text(og::buck_rogers_career_name(*career))},
         {"minimums", adjustments_text(og::career_ability_minimums(*career), false)},
         {"skills", skills},
         {"level", level},
         {"experience", static_cast<std::int64_t>(
                            og::career_experience_for_level(*career, level, game).value_or(0))}});
}

// The roll: the seven abilities where they fell, with the race's adjustment
// and the career's minimum beside them, and what the roll also made.
void CharacterCreationView::show_buck_rogers_roll()
{
    const auto &d = creator_->draft();
    const auto chosen = og::buck_rogers_draft_choices(d, og::game_of(creator_->rules().identity()));
    const auto adjustments = og::race_ability_adjustments(chosen.race);
    const auto minimums = og::career_ability_minimums(chosen.career);
    const auto record = d.rolled ? og::buck_rogers_record_of(d.rules_data) : std::nullopt;
    required_node<Label>(*this, "BuckRogersRoll/Table/MinimumHeader")
        .set_text(
            i18n::format(N_("{career} needs"),
                         {{"career", i18n::text(og::buck_rogers_career_name(chosen.career))}}));
    for (std::size_t ability = 0; ability < og::buck_rogers_ability_count; ++ability)
    {
        const auto n = std::to_string(ability);
        required_node<Label>(*this, gs("BuckRogersRoll/Table/Score" + n))
            .set_text(record ? gs(std::to_string(record->creature.current_abilities[ability]))
                             : String("--"));
        required_node<Label>(*this, gs("BuckRogersRoll/Table/Race" + n))
            .set_text(adjustment_text(adjustments[ability]));
        required_node<Label>(*this, gs("BuckRogersRoll/Table/Minimum" + n))
            .set_text(has_minimum(minimums[ability]) ? gs(std::to_string(minimums[ability]))
                                                     : String());
    }
    required_node<Button>(*this, "BuckRogersRoll/RollRow/Roll")
        .set_text(i18n::text(d.rolled ? N_("Roll again") : N_("Roll")));
    const auto &c = record ? record->creature : opengold::buck_rogers::Creature{};
    required_node<Label>(*this, "BuckRogersRoll/RollRow/Rolled")
        .set_text(
            record
                ? i18n::format(
                      N_("Hit Points {hp}   Age {age}   Level {level}   Experience {xp}   Credits {credits}"),
                      {{"hp", static_cast<int>(c.hit_point_maximum)},
                       {"age", static_cast<int>(c.age)},
                       {"level", c.level},
                       {"xp", static_cast<std::int64_t>(c.experience)},
                       {"credits", static_cast<std::int64_t>(c.credits)}})
                : String());
    required_node<Label>(*this, "Instructions")
        .set_text(i18n::text(
            N_("Roll the character. Roll again until you have one to keep, then continue.")));
}

// A Warrior's weapon specializations (viewchar.pas SpecializationMenu): the
// weapons of item templates 1 to 15, at most three in one.
void CharacterCreationView::show_buck_rogers_specializations()
{
    const auto *training = buck_rogers_training();
    const auto character =
        training ? training->choices.character() : record_of(creator_->draft()).creature;
    auto asked = creator_->rules().in_place_pages(creator_->draft());
    if (training)
    {
        asked.specializations = training->choices.specializations_owed();
        asked.chosen_specializations = training->choices.specializations_chosen();
    }
    const auto files = game_rules_files(application_settings(*this));
    const auto &stock = buck_rogers_stock(files);
    required_node<Label>(*this, "Specializations/Count")
        .set_text(i18n::format(
            N_("Chosen {chosen} of {count}. Each adds one to hit and to damage with its weapon, at most three in one."),
            {{"chosen", asked.chosen_specializations}, {"count", asked.specializations}}));
    auto &rows = required_node<GridContainer>(*this, "Specializations/Scroll/Rows");
    clear_rows(rows);
    for (const auto type : og::specializable_weapon_types(stock.templates))
    {
        const auto weapon =
            std::ranges::find(stock.templates, type, &opengold::buck_rogers::Item::type);
        const auto n = std::to_string(type);
        const int level = og::specialization_level(character, type);
        auto *name = add_label(rows, gs("Weapon" + n),
                               gs(opengold::buck_rogers_item_name(*weapon, stock.item_words)));
        // The columns stay together at the left, as the original's screen has them.
        name->set_custom_minimum_size(Vector2(260, 0));
        add_label(rows, gs("Level" + n), level ? gs("+" + std::to_string(level)) : String());
        auto *lower = add_button(rows, gs("Lower" + n), "-");
        lower->set_disabled(level == 0);
        lower->connect("pressed",
                       presentation::guarded(this, &CharacterCreationView::specialization_changed)
                           .bind(static_cast<int>(type), -1));
        auto *raise = add_button(rows, gs("Raise" + n), "+");
        raise->set_disabled(asked.chosen_specializations >= asked.specializations ||
                            level >= og::max_specialization);
        raise->connect("pressed",
                       presentation::guarded(this, &CharacterCreationView::specialization_changed)
                           .bind(static_cast<int>(type), 1));
    }
    required_node<Label>(*this, "Instructions")
        .set_text(
            i18n::plural("Choose a weapon to specialize in.",
                         "Choose {count} weapon specializations; one weapon may take several.",
                         asked.specializations, {{"count", asked.specializations}}));
    required_node<Button>(*this, "Next")
        .set_disabled(asked.chosen_specializations < asked.specializations);
}

// The spending begins from the roll, and starts over with another.
void CharacterCreationView::keep_spending_current()
{
    const auto &d = creator_->draft();
    const auto roll = d.rolled ? roll_of(d.rules_data) : std::string();
    if (spending_ && roll == spending_roll_)
    {
        return;
    }
    spending_.reset();
    spending_roll_ = roll;
    if (roll.empty())
    {
        return;
    }
    const auto game = og::game_of(creator_->rules().identity());
    spending_ = og::SkillSpending::at_creation(record_of(d).creature, game);
    career_points_given_ = spending_->career_points();
    general_points_given_ = spending_->general_points();
    selected_skill_ = 0;
    // The record starts with the skills the screen starts with: the
    // career's free levels, or Matrix Cubed's presets.
    change_record(
        [](opengold::buck_rogers::Creature &)
        {
        });
}

template <class Change> void CharacterCreationView::change_record(Change &&change)
{
    auto record = record_of(creator_->draft());
    std::forward<Change>(change)(record.creature);
    if (spending_)
    {
        spending_->apply_to(record.creature);
    }
    creator_->change_record(og::buck_rogers_draft(record).rules_data);
}

// The skill screen (skills.pas SkillScreen): the points left, then the tab's
// skills with their points, the group ability's bonus and their total.
void CharacterCreationView::show_buck_rogers_skills()
{
    const auto game = og::game_of(creator_->rules().identity());
    const auto *training = buck_rogers_training();
    // Matrix Cubed's creation moves preset points; its training spends new
    // ones, as Countdown's does.
    const bool matrix = !training && game == opengold::GoldBoxGame::matrix_cubed;
    const auto character =
        training ? training->choices.character() : record_of(creator_->draft()).creature;
    const auto career = training ? og::career_from_record(character.career).value()
                                 : og::buck_rogers_draft_choices(creator_->draft(), game).career;
    const auto &spending = training ? training->choices.spending() : *spending_;
    auto &points = required_node<Label>(*this, "Skills/Points");
    if (matrix)
    {
        points.set_text(i18n::format(
            N_("Level {level}   Career points to move {career}   General points to move {general}   Most above a skill's start {most}"),
            {{"level", character.level},
             {"career", spending.career_points()},
             {"general", spending.general_points()},
             {"most", spending.max_per_skill()}}));
    }
    else
    {
        points.set_text(i18n::format(
            N_("Career points left {career} of {career_given}   General points left {general} of {general_given}   Most in one skill {most}   General skills known {known} of {limit}"),
            {{"career", spending.career_points()},
             {"career_given", training ? training_career_given_ : career_points_given_},
             {"general", spending.general_points()},
             {"general_given", training ? training_general_given_ : general_points_given_},
             {"most", spending.max_per_skill()},
             {"known", spending.known_general_skills()},
             {"limit", spending.general_skills_allowed()}}));
    }
    color(points, *this, "score_positive");
    for (std::size_t n = 0; n < skill_tabs.size(); ++n)
    {
        required_node<Button>(*this, gs("Skills/Groups/Tab" + std::to_string(n)))
            .set_pressed_no_signal(std::cmp_equal(n, skill_tab_));
    }
    const auto &tab = skill_tabs.at(static_cast<std::size_t>(skill_tab_));
    std::vector<Skill> listed;
    if (tab.career)
    {
        const auto skills = og::career_skills(career);
        listed.assign(skills.begin(), skills.end());
    }
    else
    {
        for (const auto skill : og::general_skills(career))
        {
            if (!tab.group || og::skill_definition(skill).group == *tab.group)
            {
                listed.push_back(skill);
            }
        }
    }
    auto &rows = required_node<GridContainer>(*this, "Skills/Scroll/Rows");
    clear_rows(rows);
    for (const auto *header : {N_("Skill"), N_("Points"), N_("Bonus"), N_("Total"), "", "", ""})
    {
        add_label(rows, gs(std::string("Header") + header),
                  *header ? i18n::text(header) : String());
    }
    auto shown = character;
    spending.apply_to(shown);
    for (const auto skill : listed)
    {
        const auto n = std::to_string(static_cast<unsigned>(skill));
        const bool locked = !og::meets_skill_prerequisite(spending.levels(), career, skill);
        auto trial = spending;
        const auto refusal = trial.add(skill);
        auto lowered = spending;
        auto *name = add_button(rows, gs("Skill" + n), skill_name(skill));
        name->set_flat(true);
        name->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
        name->set_custom_minimum_size(Vector2(300, 28));
        name->set_toggle_mode(true);
        name->set_pressed_no_signal(static_cast<int>(skill) == selected_skill_);
        name->connect("pressed", presentation::guarded(this, &CharacterCreationView::skill_selected)
                                     .bind(static_cast<int>(skill)));
        std::array<Label *, 3> numbers{
            add_label(rows, gs("Points" + n),
                      gs(std::to_string(og::level_of(spending.levels(), skill)))),
            add_label(rows, gs("Bonus" + n),
                      gs(std::to_string(og::skill_ability_score(shown, skill)))),
            add_label(rows, gs("Total" + n), gs(std::to_string(og::skill_value(shown, skill))))};
        auto *lower = add_button(rows, gs("Lower" + n), "-");
        lower->set_disabled(!lowered.subtract(skill));
        lower->connect("pressed", presentation::guarded(this, &CharacterCreationView::skill_lowered)
                                      .bind(static_cast<int>(skill)));
        auto *raise = add_button(rows, gs("Raise" + n), "+");
        raise->set_disabled(refusal != og::SkillRefusal::none);
        raise->connect("pressed", presentation::guarded(this, &CharacterCreationView::skill_raised)
                                      .bind(static_cast<int>(skill)));
        auto *note =
            add_label(rows, gs("Note" + n), refusal_note(refusal, spending.max_per_skill()));
        if (locked)
        {
            color(*name, *this, "score_negative");
            for (auto *number : numbers)
            {
                color(*number, *this, "score_negative");
            }
            color(*note, *this, "score_negative");
        }
        else if (refusal != og::SkillRefusal::none)
        {
            color(*note, *this, "score_positive");
        }
    }
    // What the selected skill needs (SkillScreen's "Needs N points in"), and
    // in Matrix Cubed the presets outside the career's group.
    String needs;
    if (selected_skill_ > 0)
    {
        const auto skill = og::skill_numbered(static_cast<unsigned>(selected_skill_));
        const auto prerequisites = og::skill_prerequisites(skill);
        String listed_needs;
        for (const auto &need : prerequisites.needs)
        {
            listed_needs += (listed_needs.is_empty() ? String()
                             : prerequisites.any     ? i18n::text(N_(" or "))
                                                     : String(", ")) +
                            i18n::format(N_("{points} in {skill} (it has {has})"),
                                         {{"points", need.level},
                                          {"skill", skill_name(need.skill)},
                                          {"has", og::level_of(spending.levels(), need.skill)}});
        }
        if (og::is_medical_skill(skill) && career != og::Profession::medic)
        {
            needs = i18n::format(N_("{skill} is a Medic's skill."), {{"skill", skill_name(skill)}});
        }
        else if (!listed_needs.is_empty())
        {
            needs = i18n::format(N_("{skill} needs {needs}."),
                                 {{"skill", skill_name(skill)}, {"needs", listed_needs}});
        }
    }
    if (matrix && tab.career)
    {
        String presets;
        for (const auto &preset : og::preset_skills(career, game))
        {
            if (!og::is_career_skill(career, preset.skill))
            {
                presets += (presets.is_empty() ? String() : String(", ")) +
                           skill_name(preset.skill) + " " +
                           gs(std::to_string(og::level_of(spending.levels(), preset.skill)));
            }
        }
        if (!presets.is_empty())
        {
            needs += (needs.is_empty() ? String() : String("\n")) +
                     i18n::format(N_("Also preset, in the general groups: {skills}."),
                                  {{"skills", presets}});
        }
    }
    required_node<Label>(*this, "Skills/Needs").set_text(needs);
    required_node<Label>(*this, "Instructions")
        .set_text(i18n::text(
            training
                ? N_("Spend the training's career points on career skills and its general points on general skills. Points can be taken back until you leave the skills.")
            : matrix
                ? N_("A new Matrix Cubed character starts with its career's skills set. You can move points from one skill to another of the same kind; there are no new points to spend.")
                : N_("Spend career points on career skills and general points on general skills. Points can be taken back until the character is finished.")));
}

// The body icon (chargen.pas CustomizeIcon): the 36 icons, the chosen one's
// ready and action poses beside them.
void CharacterCreationView::show_buck_rogers_icons()
{
    const auto icon = record_of(creator_->draft()).creature.icon;
    auto &grid = required_node<GridContainer>(*this, "BodyIcons/Grid");
    for (unsigned n = 0; n < presentation::buck_rogers_body_icon_count; ++n)
    {
        auto &button = required_node<Button>(grid, gs("Icon" + std::to_string(n)));
        button.set_button_icon(
            presentation::buck_rogers_body_icon(application_settings(*this), n).ready);
        button.set_pressed_no_signal(n == icon);
    }
    const auto chosen = presentation::buck_rogers_body_icon(application_settings(*this), icon);
    required_node<TextureRect>(*this, "BodyIcons/Preview/Ready").set_texture(chosen.ready);
    required_node<TextureRect>(*this, "BodyIcons/Preview/Action").set_texture(chosen.action);
    required_node<Label>(*this, "Instructions")
        .set_text(
            i18n::text(N_("Select a body icon. It is your character's picture in every screen.")));
}

void CharacterCreationView::skill_tab_selected(std::int64_t tab)
{
    perform(
        [&]
        {
            skill_tab_ = static_cast<int>(tab);
        });
}

void CharacterCreationView::skill_selected(int number)
{
    perform(
        [&]
        {
            selected_skill_ = number;
        });
}

void CharacterCreationView::skill_raised(int number)
{
    perform(
        [&]
        {
            selected_skill_ = number;
            const auto skill = og::skill_numbered(static_cast<unsigned>(number));
            auto *session = training_session();
            const auto refusal =
                session ? session->raise_training_skill(skill) : spending_->add(skill);
            if (refusal != og::SkillRefusal::none)
            {
                throw std::runtime_error(refusal == og::SkillRefusal::no_points
                                             ? "No skill points left"
                                             : "Skill at maximum for this level");
            }
            if (session)
            {
                return;
            }
            change_record(
                [](opengold::buck_rogers::Creature &)
                {
                });
        });
}

void CharacterCreationView::skill_lowered(int number)
{
    perform(
        [&]
        {
            selected_skill_ = number;
            const auto skill = og::skill_numbered(static_cast<unsigned>(number));
            if (auto *session = training_session())
            {
                (void)session->lower_training_skill(skill);
                return;
            }
            (void)spending_->subtract(skill);
            change_record(
                [](opengold::buck_rogers::Creature &)
                {
                });
        });
}

void CharacterCreationView::specialization_changed(int item_type, int direction)
{
    perform(
        [&]
        {
            const auto type = static_cast<std::uint8_t>(item_type);
            if (auto *session = training_session())
            {
                if (direction < 0)
                {
                    (void)session->unspecialize_in_training(type);
                }
                else if (!session->specialize_in_training(type))
                {
                    throw std::runtime_error("Specialized three times in that weapon");
                }
                return;
            }
            change_record(
                [&](opengold::buck_rogers::Creature &creature)
                {
                    if (direction < 0)
                    {
                        (void)og::unspecialize(creature, type);
                    }
                    else if (!og::specialize(creature, type))
                    {
                        throw std::runtime_error("Specialized three times in that weapon");
                    }
                });
        });
}

void CharacterCreationView::body_icon_chosen(int icon)
{
    perform(
        [&]
        {
            change_record(
                [&](opengold::buck_rogers::Creature &creature)
                {
                    creature.icon = static_cast<std::uint8_t>(icon);
                });
        });
}

bool CharacterCreationView::skill_points_unspent() const
{
    if (const auto *training = buck_rogers_training())
    {
        const auto &spending = training->choices.spending();
        return training_on_skills_ && spending.career_points() + spending.general_points() > 0;
    }
    return buck_rogers() && creator_->step() == CreationStep::skills && spending_ &&
           spending_->career_points() + spending_->general_points() > 0;
}

void CharacterCreationView::leave_skills()
{
    perform(
        [&]
        {
            if (training_open_)
            {
                finish_buck_rogers_training();
                return;
            }
            creator_->next();
        });
}

RolfTourSession *CharacterCreationView::training_session() const
{
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    return training_open_ && town ? town->waiting_buck_rogers_training() : nullptr;
}

const opengold::por::BuckRogersTraining *CharacterCreationView::buck_rogers_training() const
{
    const auto *session = training_session();
    return session ? session->buck_rogers_training() : nullptr;
}

// The town gives way to the pages, as Reincarnate's do; every other node of
// the creation screen is hidden until they close.
void CharacterCreationView::open_buck_rogers_training()
{
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    training_open_ = true;
    const auto *training = buck_rogers_training();
    if (!town || !training)
    {
        training_open_ = false;
        return;
    }
    training_on_skills_ = training->choices.specializations_owed() == 0;
    training_career_given_ = training->choices.spending().career_points();
    training_general_given_ = training->choices.spending().general_points();
    skill_tab_ = 0;
    selected_skill_ = 0;
    error_ = "";
    training_hidden_.clear();
    for (int n = 0; n < get_child_count(); ++n)
    {
        auto *control = Object::cast_to<Control>(get_child(n));
        if (control && control != town && control->is_visible())
        {
            training_hidden_.emplace_back(control->get_name());
            control->hide();
        }
    }
    town->hide();
    town->set_process(false);
    town->set_process_input(false);
    refresh();
}

// Only the page, its title, instructions, status and Next (Back too on the
// skill screen after a specialization) show.
void CharacterCreationView::refresh_buck_rogers_training()
{
    const auto *training = buck_rogers_training();
    const bool skills = training_on_skills_;
    const bool owed = training->choices.specializations_owed() > 0;
    for (int n = 0; n < get_child_count(); ++n)
    {
        auto *control = Object::cast_to<Control>(get_child(n));
        if (!control || control->get_name() == StringName("CampaignTown"))
        {
            continue;
        }
        const String name = control->get_name();
        control->set_visible(name == "PageTitle" || name == "Instructions" || name == "Status" ||
                             name == "Next" || (name == "Back" && skills && owed) ||
                             (name == "Skills" && skills) ||
                             (name == "Specializations" && !skills));
    }
    required_node<Label>(*this, "PageTitle")
        .set_text(i18n::format(N_("{name} trains to level {level}"),
                               {{"name", gs(training->choices.character().name)},
                                {"level", static_cast<int>(training->choices.character().level)}}));
    required_node<Label>(*this, "Status").set_text(error_);
    auto &next = required_node<Button>(*this, "Next");
    next.set_text(i18n::text(skills ? N_("Done") : N_("Next")));
    next.set_disabled(false);
    if (skills)
    {
        show_buck_rogers_skills();
    }
    else
    {
        show_buck_rogers_specializations();
    }
}

// Keeps the choices in the trainee's record; the town's team menu shows again.
void CharacterCreationView::finish_buck_rogers_training()
{
    training_session()->finish_buck_rogers_training();
    training_open_ = false;
    for (const auto &name : training_hidden_)
    {
        required_node<Control>(*this, name).show();
    }
    training_hidden_.clear();
    for (const auto *page : {"Specializations", "Skills"})
    {
        required_node<Control>(*this, page).hide();
    }
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->show();
        town->set_process(true);
        town->set_process_input(true);
        town->resume_party();
    }
}
