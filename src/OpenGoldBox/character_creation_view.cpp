#include "cantrip_control.h"
#include "spell_choice_controls.h"
#include "training_control.h"
#include "godot_images.h"
#include "application_settings.h"
#include "character_creation_view.h"
#include "character_sheet_text.h"
#include "character_text.h"
#include "opengold/srd5.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include "scoped_flag.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace godot;
using namespace opengold;
using namespace opengold::rules;
using presentation::required_node;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), static_cast<int64_t>(s.size()));
}

const std::array<const char *, 9> steps
{
    N_("Name, Race & Gender"), N_("Alignment"),         N_("Attributes"),
    N_("Class"),               N_("Training"),          N_("Spell Choices"),
    N_("Combat appearance"),   N_("Portrait"),          N_("Character sheet")};

CreationField choice_field(CreationStep step)
{
    switch (step)
    {
    case CreationStep::race:
        return CreationField::race;
    case CreationStep::alignment:
        return CreationField::alignment;
    case CreationStep::character_class:
        return CreationField::character_class;
    default:
        throw std::runtime_error("This step has no choice list");
    }
}

const std::array<const char *, 6> full_abilities{N_("Strength"),     N_("Dexterity"),
    N_("Constitution"), N_("Intelligence"),
    N_("Wisdom"),       N_("Charisma")};
const std::array<const char *, 16> colors
{
    N_("Black"),     N_("Blue"),       N_("Green"),       N_("Cyan"),
    N_("Red"),       N_("Magenta"),    N_("Brown"),       N_("Light gray"),
    N_("Dark gray"), N_("Light blue"), N_("Light green"), N_("Light cyan"),
    N_("Light red"), N_("Pink"),       N_("Yellow"),      N_("White")};
const std::array<const char *, 6> parts{N_("Weapon"), N_("Body"), N_("Hair / Face"),
    N_("Shield"), N_("Arms"), N_("Legs")};

std::string signed_number(int n)
{
    return (n >= 0 ? "+" : "") + std::to_string(n);
}

std::string selection(const CharacterDraft &d, CreationField f)
{
    switch (f)
    {
    case CreationField::race:
        return d.race;
    case CreationField::gender:
        return d.gender;
    case CreationField::character_class:
        return d.character_class;
    case CreationField::alignment:
        return d.alignment;
    case CreationField::background:
        return d.background;
    }
    return {};
}
} // namespace

void CharacterCreationView::_bind_methods()
{
}

void CharacterCreationView::_notification(int what)
{
    presentation::run_guarded(*this, [&]
    {
        respond_to_notification(what);
    });
}

void CharacterCreationView::respond_to_notification(int what)
{
    // Rows of buttons size to their labels, which change with the language.
    if ((what == NOTIFICATION_RESIZED || what == NOTIFICATION_TRANSLATION_CHANGED) && ready_)
    {
        layout();
        queue_redraw();
    }
}

void CharacterCreationView::_ready()
{
    i18n::prepare_ui(*this);
    ready_ = true;
    // All node pointers here and below are borrowed from the owning scene tree.
    required_node<Button>(*this, "Next").connect("pressed", presentation::guarded(this,
            &CharacterCreationView::next));
    required_node<Button>(*this, "Back").connect("pressed", presentation::guarded(this,
            &CharacterCreationView::back));
    required_node<Button>(*this, "Restart").connect("pressed",
            presentation::guarded(this, &CharacterCreationView::restart));
    required_node<Button>(*this, "Roll").connect("pressed", presentation::guarded(this,
            &CharacterCreationView::roll));
    required_node<ItemList>(*this, "Choices").connect(
        "item_selected", presentation::guarded(this, &CharacterCreationView::choice_selected));
    required_node<OptionButton>(*this, "Gender").connect(
        "item_selected", presentation::guarded(this, &CharacterCreationView::gender_selected));
    required_node<OptionButton>(*this, "Background")
    .connect("item_selected", presentation::guarded(this,
            &CharacterCreationView::background_selected));
    for (int i = 0; i < 12; ++i)
        required_node<CheckBox>(*this, gs("Targets/Rows/Class" + std::to_string(i)))
        .connect("toggled", presentation::guarded(this, &CharacterCreationView::target_toggled).bind(i));
    required_node<OptionButton>(*this, "Bonus").connect(
        "item_selected", presentation::guarded(this, &CharacterCreationView::bonus_selected));
    required_node<Control>(*this, "PortraitScroll/PortraitSheet")
    .connect("portrait_chosen", presentation::guarded(this, &CharacterCreationView::portrait_selected));
    required_node<LineEdit>(*this, "Name").connect("text_changed",
            presentation::guarded(this, &CharacterCreationView::name_changed));
    for (int i = 0; i < 6; ++i)
    {
        required_node<Button>(*this, gs("Ability" + std::to_string(i)))
        .connect("pressed", presentation::guarded(this, &CharacterCreationView::score_selected).bind(i));
        required_node<Control>(*this, gs("Dice" + std::to_string(i)))
        .set_drag_forwarding(presentation::guarded(this, &CharacterCreationView::drag_roll).bind(i),
                             Callable(), Callable());
        required_node<Control>(*this, gs("Dice" + std::to_string(i)))
        .set_default_cursor_shape(Control::CURSOR_DRAG);
        required_node<Control>(*this, gs("Dice" + std::to_string(i)))
        .set_tooltip_text(
            i18n::text(N_("Drag this rolled result onto an attribute to assign it.")));
        for (const char *stem :
                {"Ability", "Score", "BonusScore", "TotalScore"
                })
            required_node<Control>(*this, gs(std::string(stem) + std::to_string(i)))
            .set_drag_forwarding(
                Callable(), presentation::guarded(this, &CharacterCreationView::can_drop_roll).bind(i),
                presentation::guarded(this, &CharacterCreationView::drop_roll).bind(i));
        auto *score = &required_node<Button>(*this, gs("Score" + std::to_string(i)));
        score->set_drag_forwarding(presentation::guarded(this,
                &CharacterCreationView::drag_roll).bind(i + 6),
                                   presentation::guarded(this, &CharacterCreationView::can_drop_roll).bind(i),
                                   presentation::guarded(this, &CharacterCreationView::drop_roll).bind(i));
        score->set_default_cursor_shape(Control::CURSOR_DRAG);
        score->set_tooltip_text(
            i18n::text(N_("Drop a roll here. Drag a filled box onto another ability to swap.")));
    }
    required_node<Button>(*this, "SavingThrows")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_saving_throws));
    required_node<Button>(*this, "SavingThrowsModal/Close")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::close_saving_throws));
    required_node<Window>(*this, "SavingThrowsModal")
    .connect("close_requested",
             presentation::guarded(this, &CharacterCreationView::close_saving_throws));
    required_node<LineEdit>(*this, "SavingThrowsModal/DC")
    .connect("text_changed", presentation::guarded(this,
            &CharacterCreationView::update_saving_throws));
    required_node<Button>(*this, "Modifiers")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::show_modifiers));
    required_node<Button>(*this, "ModifiersModal/Close")
    .connect("pressed", presentation::guarded(this, &CharacterCreationView::close_modifiers));
    required_node<Window>(*this, "ModifiersModal")
    .connect("close_requested", presentation::guarded(this, &CharacterCreationView::close_modifiers));
    for (int direction :
            {
                -1, 1
            })
    {
        const auto suffix = direction < 0 ? "Previous" : "Next";
        required_node<Button>(*this, gs(std::string("CombatHead") + suffix))
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::combat_part).bind(0, direction));
        required_node<Button>(*this, gs(std::string("Body") + suffix))
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::combat_part).bind(1, direction));
    }
    required_node<OptionButton>(*this, "Size").connect("item_selected",
            presentation::guarded(this, &CharacterCreationView::size_selected));
    for (int bank = 0; bank < 2; ++bank)
        for (int part = 0; part < 6; ++part)
            required_node<Button>(*this,
                                  gs("Color" + std::to_string(bank) + "_" + std::to_string(part)))
            .connect(
                "pressed",
                presentation::guarded(this, &CharacterCreationView::color_selected).bind(bank, part));
    for (int i = 0; i < 16; ++i)
    {
        auto *button = &required_node<Button>(*this, gs("Palette" + std::to_string(i)));
        button->connect("pressed",
                        presentation::guarded(this, &CharacterCreationView::palette_selected).bind(i));
        button->set_tooltip_text(i18n::text(colors[i]));
        button->call("configure_palette_index", i);
    }
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    portrait_check_ = args.has("--portrait-check");
    checking_ = args.has("--character-check") || portrait_check_;
    capture_ = args.has("--capture");
    try
    {
        const auto directory = settings::game_path();
        art_ = por::CharacterArt::load(presentation::path_from_godot(directory));
        body_groups_ = art_->combat_body_groups();
        presentation::load_additional_portrait_heads(*art_);
        load_portraits();
        const auto seed =
            // --fixed-seed repeats the same rolls, so screenshots compare.
            (checking_ || args.has("--party-check") || args.has("--fixed-seed"))
            ? 42ULL
            : static_cast<std::uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
        creator_ = std::make_unique<CharacterCreator>(srd5::character_rules(), seed);
        setup_party();
        recommend_portrait();
        refresh();
    }
    catch (const std::exception &e)
    {
        fatal_ = true;
        error_ = i18n::text(e.what());
        required_node<Label>(*this, "Instructions")
        .set_text(i18n::text(N_(
                                 "Character art could not be loaded. Restart with --reset-game-path to choose your Pool of Radiance folder.")));
        required_node<Label>(*this, "Status").set_text(error_);
        required_node<Button>(*this, "Next").set_disabled(true);
        for (int i = 0; i < get_child_count(); ++i)
            if (auto *c = Object::cast_to<Control>(get_child(i)))
                if (c->get_name() != StringName("Title") &&
                        c->get_name() != StringName("Instructions") &&
                        c->get_name() != StringName("Status"))
                    c->hide();
    }
}

void CharacterCreationView::layout()
{
    auto &creation_layout = required_node<Node>(*this, "CreationLayout");
    creation_layout.call("play", "normal");
    creation_layout.call("advance", 0);

    if (creator_)
    {
        if (creator_->step() == CreationStep::race)
            creation_layout.call("play", "race");
        else if (creator_->step() == CreationStep::attributes)
            creation_layout.call("play", "attributes");
        else if (creator_->step() == CreationStep::sheet)
            creation_layout.call("play", "sheet");
        creation_layout.call("advance", 0);
    }

    required_node<Node>(*this, "ColumnLayout").call("apply_layout");

    required_node<Node>(*this, "ModifiersModal/WindowLayout").call("fit", Vector2i(get_size()));
    required_node<Node>(*this, "SavingThrowsModal/WindowLayout").call("fit", Vector2i(get_size()));
    if (campaign_)
        party_layout();
}

void CharacterCreationView::recommend_portrait()
{
    if (portrait_chosen_)
        return;
    auto a = creator_->appearance();
    const auto &d = creator_->draft();
    a.portrait = portraits_->recommended(d);
    creator_->appearance(a);
}

void CharacterCreationView::recommend_icon_size()
{
    auto a = creator_->appearance();
    const auto &race = creator_->draft().race;
    a.icon_size = race == "gnome"     ? por::IconSize::small
                  : race == "goliath" ? por::IconSize::large
                  : por::IconSize::regular;
    creator_->appearance(a);
}

void CharacterCreationView::show_race_portrait(bool again)
{
    const auto &d = creator_->draft();
    if (!again && race_portrait_.is_valid() && race_portrait_of_ == d.race &&
            race_portrait_gender_ == d.gender)
        return;
    auto a = creator_->appearance();
    a.portrait = portraits_->of_race_and_gender(
                     d, static_cast<std::uint32_t>(UtilityFunctions::randi()));
    race_portrait_ = portraits_->texture(a, d);
    race_portrait_of_ = d.race;
    race_portrait_gender_ = d.gender;
    race_portrait_file_ = a.portrait;
}

void CharacterCreationView::refresh_art()
{
    if (!creator_ || !art_ || rendered_ == creator_->appearance())
        return;
    const auto &a = creator_->appearance();
    // Unarmed, as combat shows a character with nothing equipped; weapons and
    // shields appear there once they are equipped.
    constexpr unsigned unarmed_body = 0;
    const std::array<opengold::Image, 3> images{opengold::Image{},
            art_->equipped_icon(a, unarmed_body, por::IconPose::ready),
            art_->equipped_icon(a, unarmed_body, por::IconPose::action)};
    images_[0] = portraits_->texture(a, creator_->draft());
    for (unsigned i = 1; i < images.size(); ++i)
    {
        const auto &source = images[i];
        images_[i] = presentation::image_texture(source);
    }
    rendered_ = a;
}

void CharacterCreationView::refresh()
{
    if (!creator_)
        return;
    const presentation::ScopedFlag refreshing(refreshing_);
    const auto step = creator_->step();
    const auto &d = creator_->draft();
    const auto &a = creator_->appearance();
    const auto show = [&](const String &node, bool visible)
    {
        required_node<Control>(*this, node).set_visible(visible);
    };
    const bool choosing = step == CreationStep::race || step == CreationStep::alignment ||
                          step == CreationStep::character_class,
                          stats = step == CreationStep::attributes, icon = step == CreationStep::combat_icon,
                          portrait = step == CreationStep::portrait;
    for (const auto *n : {"Choices"})
        show(n, choosing);
    show("GenderLabel", step == CreationStep::race);
    show("Gender", step == CreationStep::race);
    show("Description", choosing || step == CreationStep::sheet);
    show("Modifiers", step == CreationStep::sheet);
    show("SavingThrows", step == CreationStep::sheet);
    for (const auto *n :
            {"BackgroundLabel", "Background", "BonusLabel", "Bonus", "Columns", "DiceHeader",
             "BaseHeader", "BonusHeader", "TotalHeader", "Roll", "SwapHint"
            })
        show(n, stats);
    show("DiceHint", stats);
    show("Instructions", !stats);
    for (const auto *n :
            {"TargetsTitle", "Targets", "TargetHint"
            })
        show(n, stats);
    for (unsigned i = 0; i < 6; ++i)
        show(gs("Warning" + std::to_string(i)),
             stats && rules::unmet_targets(creator_->rules(), d)[rules::all_abilities[i]]);
    for (const auto *n :
            {"BaseHeader", "BonusHeader", "TotalHeader"
            })
        show(n, false);
    show("NameLabel", step == CreationStep::race);
    show("RaceSelection", step == CreationStep::race);
    if (step == CreationStep::race)
        show_race_portrait(false);
    show("Name", step == CreationStep::race);
    show("SpellChoices", step == CreationStep::spell_choices);
    show("TrainingFixed", step == CreationStep::training);
    show("TrainingFixedTitle", step == CreationStep::training);
    show("Training", step == CreationStep::training);
    show("SpeciesChoices", step == CreationStep::race && creator_->has_species_choices());
    for (const auto *n :
            {"PortraitGender", "PortraitClass", "PortraitRace", "PortraitIdentity", "PortraitFilters",
             "PortraitScroll"
            })
        show(n, portrait);
    show("PortraitPreview", portrait || step == CreationStep::sheet);
    if (!portrait)
        show("PortraitZoom", false);
    for (const auto *n :
            {"PortraitGender", "PortraitClass", "PortraitRace"
            })
        required_node<Button>(*this, n).set_disabled(added_to_party_);
    for (const auto *n :
            {"CombatHeadPrevious", "CombatHeadNext", "CombatHeadLabel", "BodyPrevious", "BodyNext",
             "BodyLabel", "Size", "ColorTitle", "Color1Title", "Color2Title", "PaletteHint",
             "ReadyLabel", "ActionLabel"
            })
        show(n, icon);
    for (int i = 0; i < 6; ++i)
    {
        for (const auto &stem :
                {
                    std::string("Ability"), std::string("Dice"), std::string("Score"),
                    std::string("BonusScore"), std::string("TotalScore")
                })
            required_node<Control>(*this, gs(stem + std::to_string(i))).set_visible(stats);
        required_node<Control>(*this, gs("TotalScore" + std::to_string(i))).hide();
        required_node<Control>(*this, gs("Part" + std::to_string(i))).set_visible(icon);
        for (int bank = 0; bank < 2; ++bank)
            required_node<Control>(*this,
                                   gs("Color" + std::to_string(bank) + "_" + std::to_string(i)))
            .set_visible(icon);
    }
    for (int i = 0; i < 16; ++i)
        required_node<Control>(*this, gs("Palette" + std::to_string(i))).set_visible(icon);
    required_node<Label>(*this, "PageTitle")
    .set_text(i18n::text(steps[static_cast<unsigned>(step)]));
    std::string progress;
    unsigned ordinal = 0;
    for (unsigned i = 0; i < steps.size(); ++i)
    {
        if (i == static_cast<unsigned>(CreationStep::spell_choices) &&
                !creator_->has_spell_choices())
            continue;
        // A new body keeps the alignment, scores, class and training.
        if (creator_->reincarnating() && i >= static_cast<unsigned>(CreationStep::alignment) &&
                i <= static_cast<unsigned>(CreationStep::training))
            continue;
        progress +=
            (i == static_cast<unsigned>(step) ? "> " : "  ") + std::to_string(++ordinal) + ". " +
            i18n::utf8(i == static_cast<unsigned>(CreationStep::combat_icon) ? N_("Combat icon")
                       : steps[i]) +
            "\n\n";
    }
    required_node<Label>(*this, "Steps").set_text(gs(progress));
    required_node<Button>(*this, "Back").set_disabled(step == CreationStep::race);
    required_node<Button>(*this, "Back").set_text(i18n::text(N_("Back")));
    show("Next", step != CreationStep::sheet);
    if (campaign_)
        required_node<Button>(*this, "AddParty").set_visible(step == CreationStep::sheet
                && !added_to_party_);
    required_node<Button>(*this, "Next").set_text(i18n::text(portrait ? N_("Show character sheet") :
            N_("Next")));
    required_node<Button>(*this, "Next").set_disabled(
        (stats && !creator_->scores_assigned()) ||
        (step == CreationStep::character_class &&
         !rules::class_eligible(creator_->rules(), d, d.character_class)) ||
        (step == CreationStep::training && !creator_->training_complete()) ||
        (step == CreationStep::spell_choices && !creator_->spell_choices_complete()) ||
        (step == CreationStep::race && (d.name.empty() || !creator_->species_complete())));
    required_node<Label>(*this, "Status").set_text(error_);
    std::string instructions;
    if (choosing)
    {
        const auto field = choice_field(step);
        const auto choices = creator_->rules().choices(field);
        auto *list = &required_node<ItemList>(*this, "Choices");
        list->clear();
        for (unsigned i = 0; i < choices.size(); ++i)
        {
            list->add_item(i18n::text(choices[i].label));
            if (step == CreationStep::character_class)
            {
                list->set_item_disabled(
                    i, !rules::class_eligible(creator_->rules(), d, choices[i].id));
                list->set_item_tooltip(
                    i, i18n::format("Requires {requirements}",
                {
                    {
                        "requirements",
                        i18n::requirements(
                            creator_->rules().class_requirements(choices[i].id))
                    }
                }));
            }
            if (choices[i].id == selection(d, field))
            {
                list->select(i);
                required_node<RichTextLabel>(*this, "Description")
                .set_text(i18n::text(choices[i].description));
            }
        }
        if (step == CreationStep::character_class)
            required_node<RichTextLabel>(*this, "Description")
            .append_text(
                "\n\n" +
                i18n::text(
                    "Starting-class minimums use the multiclass prerequisites as an OpenGoldBox house rule. Disabled classes do not qualify; go Back to reassign scores or bonuses."));
        instructions =
            step == CreationStep::character_class
            ? N_("Choose one starting class. Checked targets are future plans, not additional class levels.")
            : step == CreationStep::race
            ? N_("Name your character (up to 40 characters), then choose a race (species in SRD 5.2.1) and gender.")
            : N_("Select an option, then continue.");
    }
    if (step == CreationStep::race)
    {
        auto *gender = &required_node<OptionButton>(*this, "Gender");
        gender->clear();
        const auto choices = creator_->rules().choices(CreationField::gender);
        for (unsigned i = 0; i < choices.size(); ++i)
        {
            gender->add_item(i18n::text(choices[i].label));
            if (choices[i].id == d.gender)
                gender->select(i);
        }
    }
    if (stats)
    {
        const auto targets = creator_->rules().choices(CreationField::character_class);
        for (unsigned i = 0; i < targets.size(); ++i)
        {
            auto *check =
                &required_node<CheckBox>(*this, gs("Targets/Rows/Class" + std::to_string(i)));
            const auto requirements = creator_->rules().class_requirements(targets[i].id);
            check->set_text(i18n::text(targets[i].label));
            required_node<Label>(*this, gs("Targets/Rows/Requirements" + std::to_string(i)))
            .set_text(i18n::requirements(requirements));
            check->set_tooltip_text(
                i18n::text(targets[i].description) + "\n" +
                i18n::text(rules::class_eligible(creator_->rules(), d, targets[i].id)
                           ? N_("Requirements met.")
                           : N_("Not yet qualified. You may still plan for this class.")));
            check->set_pressed_no_signal(std::find(d.target_classes.begin(), d.target_classes.end(),
                                                   targets[i].id) != d.target_classes.end());
        }
        auto *background = &required_node<OptionButton>(*this, "Background");
        background->clear();
        const auto choices = creator_->rules().choices(CreationField::background);
        for (unsigned i = 0; i < choices.size(); ++i)
        {
            background->add_item(i18n::text(choices[i].label));
            if (choices[i].id == d.background)
                background->select(i);
        }
        auto *bonus = &required_node<OptionButton>(*this, "Bonus");
        bonus->clear();
        for (const auto &option : creator_->rules().adjustments(d.background))
            bonus->add_item(i18n::adjustment(option));
        bonus->select(d.adjustment);
        required_node<Button>(*this, "Roll").set_text(
            i18n::text(d.rolled ? N_("Reroll all six") : N_("Roll all six")));
        required_node<Label>(*this, "SwapHint")
        .set_text(i18n::text(
                      N_("Fill all six boxes to continue.\nAssigned scores include bonuses.")));
    }
    std::optional<CharacterSheet> s;
    if (completed_)
        s = completed_->sheet();
    else if (creator_->scores_assigned())
        s = creator_->sheet();
    if (stats)
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto ability = rules::all_abilities[i];
            auto *b = &required_node<Button>(*this, gs("Ability" + std::to_string(i)));
            b->set_text(String(std::cmp_equal(selected_score_, i) ? "> " : "") + i18n::text(full_abilities[i]));
            b->set_disabled(!d.rolled);
            std::string dice;
            if (d.rolled &&
                    std::find(d.assignment.begin(), d.assignment.end(), i) == d.assignment.end())
            {
                dice = std::to_string(d.rolls[i].total());
            }
            required_node<RichTextLabel>(*this, gs("Dice" + std::to_string(i)))
            .set_text(gs("[center]" + dice + "[/center]"));
            const auto score = creator_->rules().ability_score(d, static_cast<rules::Ability>(i));
            auto *score_box = &required_node<Button>(*this, gs("Score" + std::to_string(i)));
            score_box->set_text(score ? gs(std::to_string(*score)) : String());
            const bool unmet = rules::unmet_targets(creator_->rules(), d)[ability];
            const int change =
                score ? *score - d.rolls[d.assignment[ability]].total() : 0;
            score_box->call("configure_score", unmet, change);
            std::string modifier;
            if (score && change)
            {
                const auto backgrounds = creator_->rules().choices(CreationField::background);
                const auto found = std::find_if(backgrounds.begin(), backgrounds.end(),
                                                [&](const auto & b)
                {
                    return b.id == d.background;
                });
                modifier = i18n::utf8(found->label) + " (" + signed_number(change) + ")";
            }
            required_node<Label>(*this, gs("BonusScore" + std::to_string(i)))
            .set_text(gs(modifier));
            required_node<Label>(*this, gs("TotalScore" + std::to_string(i)))
            .set_text(s ? gs(std::to_string(s->scores[ability])) : String("--"));
        }
    if (step == CreationStep::race && creator_->has_species_choices())
        presentation::refresh_training_controls(
            *this, *creator_, presentation::guarded(this, &CharacterCreationView::training_toggled),
            i18n::text, {}, true);
    if (step == CreationStep::training)
    {
        presentation::refresh_training_controls(
            *this, *creator_, presentation::guarded(this, &CharacterCreationView::training_toggled),
            i18n::text);
        instructions = N_("Choose the required training options. Back preserves your selections.");
    }
    if (step == CreationStep::spell_choices)
    {
        presentation::refresh_cantrip_controls(
            *this, *creator_, presentation::guarded(this, &CharacterCreationView::cantrip_toggled),
            i18n::text);
        auto *book =
            presentation::spell_rows(required_node<VBoxContainer>(*this, "SpellChoices/Rows"),
                                     "BookChoices");
        presentation::refresh_spell_groups(
            *book, creator_->spell_choice_options(),
            d.spells.value_or(opengold::rules::SpellChoices{}),
            presentation::guarded(this, &CharacterCreationView::creation_spell_toggled),
            i18n::text);
        presentation::equalize_choice_widths(required_node<Control>(*this, "SpellChoices"));
        instructions =
            d.spells && (d.character_class == "paladin" || d.character_class == "ranger")
            ? N_("Choose prepared spells. Unfilled choices remain pending; Back preserves your choices.")
            : d.spells && (d.character_class == "cleric" || d.character_class == "sorcerer" ||
                           d.character_class == "warlock" || d.character_class == "bard" ||
                           d.character_class == "druid")
            ? N_("Choose cantrips and prepared spells. Unfilled choices remain pending; Back preserves your choices.")
            : d.spells && d.character_class == "wizard"
            ? N_("Choose cantrips, spellbook entries and prepared spells. Back preserves your choices.")
            : d.spells
            ? N_("Choose the spells Magic Initiate teaches. Unfilled choices remain pending; Back preserves your choices.")
            : N_("Choose your available cantrips. Unfilled choices remain pending; Back preserves your selections.");
    }
    if (icon)
        instructions = N_(
                           "Select a part's Color-1 or Color-2, then a swatch. Watch both poses change. Absent parts are disabled.");
    refresh_portraits();
    required_node<Label>(*this, "CombatHeadLabel")
    .set_text(i18n::format("{number} / {count}", {{"number", a.combat_head + 1}, {"count", 14}}));
    const auto body_group = combat_body_group(a.combat_body);
    required_node<Label>(*this, "BodyLabel")
    .set_text(i18n::format("{number} / {count}",
    {
        {"number", static_cast<std::uint64_t>(body_group + 1)},
        {"count", static_cast<std::uint64_t>(body_groups_.size())}
    }));
    auto *size = &required_node<OptionButton>(*this, "Size");
    size->clear();
    for (const auto *label : {N_("Size: Short"), N_("Size: Regular"), N_("Size: Large")})
        size->add_item(i18n::text(label));
    size->select(static_cast<int>(a.icon_size));
    if (icon)
    {
        const auto usage = art_->color_usage(a);
        // The preview is unarmed, but a weapon and shield take these colors
        // once equipped, so they stay choosable.
        constexpr int weapon_part = 0, shield_part = 3;
        if (!usage.contains(color_bank_, color_part_) && color_part_ != weapon_part &&
                color_part_ != shield_part)
        {
            for (unsigned i = 0; i < 12; ++i)
                if (usage.contains(i / 6, i % 6))
                {
                    color_bank_ = i / 6;
                    color_part_ = i % 6;
                    break;
                }
        }
        required_node<Label>(*this, "PaletteHint")
        .set_text(i18n::format(
                      "{part} / Color-{bank}: choose a color",
        {{"part", i18n::text(parts[color_part_])}, {"bank", color_bank_ + 1}}));
        for (int bank = 0; bank < 2; ++bank)
            for (int part = 0; part < 6; ++part)
            {
                auto *button = &required_node<Button>(*this,
                                                      gs("Color" + std::to_string(bank) + "_" + std::to_string(part)));
                const bool selected = bank == color_bank_ && part == color_part_;
                const bool equipment = part == weapon_part || part == shield_part;
                const bool present = equipment || usage.contains(bank, part);
                button->set_disabled(!present);
                button->set_tooltip_text(i18n::text(
                                             equipment
                                             ? N_("Choose a swatch to recolor this part when it is equipped in combat.")
                                             : present
                                             ? N_("Choose a swatch to recolor this part in the combat preview.")
                                             : N_("This part is not present in either pose. Choose another head or body to use it.")));
                button->set_text(present ? String(selected ? "> " : "") +
                                 i18n::text(colors[a.colors[bank][part]])
                                 : i18n::text("Not present"));
                button->call("configure_part_index", a.colors[bank][part], selected);
            }
    }
    if (step == CreationStep::sheet)
    {
        instructions = N_("Review your character before adding it to the party.");
        required_node<RichTextLabel>(*this, "Description")
        .call("render", presentation::sheet_text(*completed_));
    }
    required_node<Label>(*this, "Instructions").set_text(i18n::text(instructions));
    // The party waits while a new body is made.
    required_node<Button>(*this, "Party").set_visible(!creator_->reincarnating());
    if (creator_->reincarnating())
    {
        // Only the species' choices change on the race page.
        auto *list = &required_node<ItemList>(*this, "Choices");
        const auto races = creator_->rules().choices(CreationField::race);
        for (unsigned i = 0; i < races.size() && step == CreationStep::race; ++i)
            list->set_item_disabled(i, races[i].id != d.race);
        required_node<OptionButton>(*this, "Gender").set_disabled(true);
        auto *name = &required_node<LineEdit>(*this, "Name");
        name->set_text(String::utf8(d.name.c_str()));
        name->set_editable(false);
        required_node<Button>(*this, "Restart").set_text(i18n::text(N_("Cancel")));
        required_node<Button>(*this, "AddParty").set_text(i18n::text(N_("Reincarnate")));
        String species;
        for (const auto &race : races)
            if (race.id == d.race)
                species = i18n::text(race.label);
        if (step == CreationStep::race)
            required_node<Label>(*this, "Instructions")
            .set_text(i18n::format(N_("{name} returns in a new body: {species}. Make the species' choices, then continue."),
            {{"name", String::utf8(d.name.c_str())}, {"species", species}}));
        else if (step == CreationStep::spell_choices)
            required_node<Label>(*this, "Instructions")
            .set_text(i18n::text(N_("Choose the spells Magic Initiate teaches.")));
        else if (step == CreationStep::sheet)
            required_node<Label>(*this, "Instructions")
            .set_text(i18n::text(N_("Review the new body, then cast Reincarnate.")));
    }
    // The portrait page names the character's race, gender and class so a
    // matching portrait is easy to find.
    const auto chosen_label = [&](CreationField field)
    {
        for (const auto &choice : creator_->rules().choices(field))
            if (choice.id == selection(d, field))
                return i18n::text(choice.label);
        return String();
    };
    required_node<Label>(*this, "PortraitIdentity")
    .set_text(i18n::format("Your character: {race} / {gender} / {class}",
    {
        {"race", chosen_label(CreationField::race)},
        {"gender", chosen_label(CreationField::gender)},
        {"class", chosen_label(CreationField::character_class)}
    }));
    refresh_art();
    layout();
    queue_redraw();
}

void CharacterCreationView::_draw()
{
    presentation::run_guarded(*this, [&]
    {
        draw_view();
    });
}

void CharacterCreationView::report_failure(const std::exception &failure)
{
    error_ = i18n::text(failure.what());
    required_node<Label>(*this, "Status").set_text(error_);
}

void CharacterCreationView::draw_view()
{
    const auto step = creator_ ? creator_->step() : CreationStep::race;
    required_node<Control>(*this, "CreationCanvas").call("set_images",
            images_[1], images_[2], creator_ && step == CreationStep::attributes,
            creator_ && step == CreationStep::combat_icon,
            creator_ && step == CreationStep::race ? race_portrait_ : Ref<ImageTexture>(),
            creator_ && creator_->appearance().icon_size == por::IconSize::large);
}

void CharacterCreationView::perform(const std::function<void()> &action)
{
    if (!creator_ || fatal_)
        return;
    try
    {
        error_ = "";
        action();
        refresh();
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        required_node<Label>(*this, "Status").set_text(error_);
    }
}

void CharacterCreationView::target_toggled(bool selected, int index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        creator_->target_class(
        creator_->rules().choices(CreationField::character_class).at(index).id, selected);
    });
}

void CharacterCreationView::creation_spell_toggled(bool selected, String group, String option)
{
    if (refreshing_ || !creator_)
        return;
    perform(
        [&]
    {
        creator_->spell_choice({.group = group.utf8().get_data(),
                                .option = option.utf8().get_data(),
                                .selected = selected});
    });
}

void CharacterCreationView::cantrip_toggled(bool selected, String option)
{
    if (refreshing_ || !creator_ || creator_->step() != CreationStep::spell_choices)
        return;
    perform(
        [&]
    {
        creator_->cantrip_choice(option.utf8().get_data(), selected);
    });
}

void CharacterCreationView::training_toggled(bool selected, String group, String option)
{
    // The training page, or a species' choices on the race page.
    if (refreshing_ || !creator_ ||
            (creator_->step() != CreationStep::training && creator_->step() != CreationStep::race))
        return;
    // A radio button turning off only means another of its group was chosen,
    // and that choice replaces it.
    if (!selected)
    {
        const auto groups = creator_->training_options();
        const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto & g)
        {
            return g.id == group.utf8().get_data();
        });
        if (found != groups.end() &&
                found->control == opengold::rules::TrainingChoiceControl::single_selection)
            return;
    }
    perform(
        [&]
    {
        creator_->training_choice({.group = group.utf8().get_data(),
                                   .option = option.utf8().get_data(),
                                   .selected = selected});
    });
}

void CharacterCreationView::next()
{
    perform(
        [&]
    {
        creator_->next();
        if (creator_->step() == CreationStep::sheet)
            completed_ = creator_->create_character();
        selected_score_ = -1;
    });
}

void CharacterCreationView::back()
{
    perform(
        [&]
    {
        creator_->back();
        completed_.reset();
        selected_score_ = -1;
    });
}

void CharacterCreationView::restart()
{
    // While reincarnating, the button cancels the spell.
    if (reincarnation_)
    {
        perform([&]
        {
            leave_reincarnation();
        });
        return;
    }
    perform(
        [&]
    {
        creator_->restart();
        completed_.reset();
        added_to_party_ = false;
        portrait_chosen_ = false;
        recommend_portrait();
        required_node<LineEdit>(*this, "Name").set_text("");
        selected_score_ = -1;
    });
}

void CharacterCreationView::choice_selected(std::int64_t index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        const auto f = choice_field(creator_->step());
        creator_->select(f, creator_->rules().choices(f).at(index).id);
        if (f == CreationField::race || f == CreationField::character_class)
            recommend_portrait();
        if (f == CreationField::race)
        {
            show_race_portrait(true);
            recommend_icon_size();
        }
    });
}

void CharacterCreationView::gender_selected(std::int64_t index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        creator_->select(CreationField::gender,
        creator_->rules().choices(CreationField::gender).at(index).id);
        recommend_portrait();
        show_race_portrait(true);
    });
}

void CharacterCreationView::background_selected(std::int64_t index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        creator_->select(CreationField::background,
        creator_->rules().choices(CreationField::background).at(index).id);
    });
}

void CharacterCreationView::bonus_selected(std::int64_t index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        creator_->select_adjustment(static_cast<unsigned>(index));
    });
}

void CharacterCreationView::roll()
{
    perform(
        [&]
    {
        creator_->roll();
        selected_score_ = -1;
    });
}

void CharacterCreationView::score_selected(int index)
{
    perform(
        [&]
    {
        if (selected_score_ < 0)
        selected_score_ = index;
        else
        {
            creator_->swap_scores(static_cast<rules::Ability>(selected_score_),
                                  static_cast<rules::Ability>(index));
                selected_score_ = -1;
            }
    });
}

void CharacterCreationView::name_changed(String value)
{
    perform(
        [&]
    {
        creator_->name(value.utf8().get_data());
    });
}

void CharacterCreationView::combat_part(int part, int direction)
{
    perform(
        [&]
    {
        auto a = creator_->appearance();
        if (part == 0)
        {
            constexpr int heads = 14;
            a.combat_head = static_cast<unsigned>((static_cast<int>(a.combat_head) + direction +
                                                   heads) % heads);
        }
        else
        {
            // Bodies that differ only in their weapon or shield are one choice.
            const auto groups = static_cast<int>(body_groups_.size());
            const auto next = (static_cast<int>(combat_body_group(a.combat_body)) + direction +
                               groups) % groups;
            a.combat_body = body_groups_[static_cast<std::size_t>(next)].front();
        }
        creator_->appearance(a);
    });
}

std::size_t CharacterCreationView::combat_body_group(unsigned body) const
{
    const auto found = std::find_if(body_groups_.begin(), body_groups_.end(),
                                    [&](const auto & group)
    {
        return std::find(group.begin(), group.end(), body) != group.end();
    });
    if (found == body_groups_.end())
        throw std::runtime_error("Unknown combat body");
    return static_cast<std::size_t>(found - body_groups_.begin());
}

void CharacterCreationView::size_selected(std::int64_t index)
{
    if (refreshing_)
        return;
    perform(
        [&]
    {
        auto a = creator_->appearance();
        a.icon_size = static_cast<por::IconSize>(index);
        creator_->appearance(a);
    });
}

void CharacterCreationView::color_selected(int bank, int part)
{
    perform(
        [&]
    {
        color_bank_ = bank;
        color_part_ = part;
    });
}

void CharacterCreationView::palette_selected(int index)
{
    perform(
        [&]
    {
        auto a = creator_->appearance();
        a.colors[color_bank_][color_part_] = index;
        creator_->appearance(a);
    });
}

void CharacterCreationView::capture(const char *name)
{
    if (!capture_)
        return;
    const auto path =
        presentation::path_from_godot(ProjectSettings::get_singleton()
                                      ->globalize_path(gs(std::string("user://checks/") + name)));
    std::filesystem::create_directories(path.parent_path());
    const auto image = get_viewport()->get_texture()->get_image();
    if (image.is_null() || image->save_png(gs(path.generic_string())) != OK)
        throw std::runtime_error("Character capture failed");
}

void CharacterCreationView::_process(double)
{
    if (equipment_art_check_)
    {
        try
        {
            if (++check_frames_ % 4 == 0)
                equipment_art_check();
            if (check_frames_ > 160)
                throw std::runtime_error("Equipment artwork check timed out");
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr("Equipment artwork check failed: ", gs(e.what()));
            equipment_art_check_ = false;
            get_tree()->quit(1);
        }
        return;
    }
    if (advancement_check_ || advancement_review_)
    {
        try
        {
            advancement_check();
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr("Advancement check failed: ", gs(e.what()));
            advancement_check_ = advancement_review_ = false;
            get_tree()->quit(1);
        }
        return;
    }
    if (save_capture_frames_)
    {
        try
        {
            capture_save_ui();
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr(gs(e.what()));
            get_tree()->quit(1);
        }
        return;
    }
    if (save_read_check_)
    {
        try
        {
            load_checkpoint_check();
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr("Save restart check failed: ", gs(e.what()));
            get_tree()->quit(1);
        }
        save_read_check_ = false;
        return;
    }
    try
    {
        if (campaign_)
        {
            update_party_navigation();
            refresh_advancement_arrows();
        }
        if (expedition_check_)
        {
            expedition_check();
            return;
        }
        if (reincarnate_check_)
        {
            reincarnate_check();
            return;
        }
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        UtilityFunctions::push_error(error_);
        if (expedition_check_ || party_check_ || defeat_check_ || reincarnate_check_)
        {
            get_tree()->quit(1);
            return;
        }
        required_node<Button>(*this, "ReturnParty").set_tooltip_text(error_);
        return;
    }
    if (defeat_check_ && !Engine::get_singleton()->is_editor_hint())
    {
        try
        {
            defeat_check();
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr("Defeat check failed: ", gs(e.what()));
            defeat_check_ = false;
            get_tree()->quit(1);
        }
        return;
    }
    if (party_check_ && !Engine::get_singleton()->is_editor_hint())
    {
        try
        {
            if (++check_frames_ % 4 == 0)
                party_check();
            if (check_frames_ > 3000)
                throw std::runtime_error("Party check timed out");
        }
        catch (const std::exception &e)
        {
            UtilityFunctions::printerr("Party check failed: ", gs(e.what()));
            party_check_ = false;
            get_tree()->quit(1);
        }
        return;
    }
    if (!checking_ || Engine::get_singleton()->is_editor_hint())
        return;
    try
    {
        if (fatal_ || !error_.is_empty())
            throw std::runtime_error(error_.utf8().get_data());
        if (++check_frames_ % 4 == 0)
            check_run();
        if (check_frames_ > 400 + portraits_->entries().size() * 4)
            throw std::runtime_error("Character UI check timed out");
    }
    catch (const std::exception &e)
    {
        UtilityFunctions::printerr("Character UI check failed at stage ", check_stage_, ": ",
                                   gs(e.what()));
        checking_ = false;
        get_tree()->quit(1);
    }
}

void CharacterCreationView::check_run()
{
    if (check_stage_ == 7 && drag_check_stage_ < 12)
    {
        const auto group = drag_check_stage_ / 4, phase = drag_check_stage_ % 4;
        const auto source = required_node<Control>(*this, group == 0   ? "Dice0"
            : group == 1 ? "Dice1"
            : "Score3")
                            .get_global_rect()
                            .get_center();
        const auto target =
            required_node<Control>(*this, group == 0 ? "Score3" : "Score1")
            .get_global_rect()
            .get_center();
        if (phase == 0 || phase == 2)
        {
            Ref<InputEventMouseButton> event;
            event.instantiate();
            const auto p = phase == 0 ? source : target;
            event->set_position(p);
            event->set_global_position(p);
            event->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
            event->set_pressed(phase == 0);
            get_viewport()->push_input(event, true);
        }
        else if (phase == 1)
        {
            Ref<InputEventMouseMotion> event;
            event.instantiate();
            event->set_position(target);
            event->set_global_position(target);
            event->set_relative(target - source);
            event->set_button_mask(MouseButtonMask::MOUSE_BUTTON_MASK_LEFT);
            get_viewport()->push_input(event, true);
        }
        else
        {
            const auto &assigned = creator_->draft().assignment;
            if (group == 0 &&
                    (assigned[Ability::intelligence] != 0 || assigned[Ability::strength] != 6))
                throw std::runtime_error("Mouse drag did not fill only the target ability box");
            if (group == 0)
            {
                const auto raw = creator_->draft().rolls[0].total();
                required_node<OptionButton>(*this, "Background").emit_signal("item_selected", 0);
                if (required_node<Button>(*this, "Score3").get_text() !=
                        gs(std::to_string(raw + 2)))
                    throw std::runtime_error(
                        "Background selector did not refresh the assigned score");
                required_node<OptionButton>(*this, "Bonus").emit_signal("item_selected", 2);
                if (required_node<Button>(*this, "Score3").get_text() !=
                        gs(std::to_string(raw + 1)) ||
                        !required_node<Button>(*this, "Score0").get_text().is_empty())
                    throw std::runtime_error(
                        "Bonus selector did not refresh partial scores correctly");
                required_node<OptionButton>(*this, "Background").emit_signal("item_selected", 3);
                required_node<OptionButton>(*this, "Bonus").emit_signal("item_selected", 1);
            }
            if (group == 1 && assigned[Ability::dexterity] != 1)
                throw std::runtime_error("Second dice assignment failed");
            if (group == 2 &&
                    (assigned[Ability::dexterity] != 0 || assigned[Ability::intelligence] != 1))
                throw std::runtime_error("Dragging between filled boxes failed to swap scores");
            if (can_drop_roll({}, String("invalid"), 0))
                throw std::runtime_error("Invalid drag data accepted");
            if (group == 2)
            {
                for (unsigned i :
                        {
                            0u, 2u, 4u, 5u
                        })
                {
                    Dictionary data;
                    data["opengold_ability_roll"] = i == 0 ? 2 : i == 2 ? 3 : i;
                    drop_roll({}, data, i);
                }
            }
        }
        ++drag_check_stage_;
        return;
    }
    if (check_stage_ == 14 && modal_check_stage_ < 7)
    {
        if (modal_check_stage_ == 0)
        {
            required_node<Button>(*this, "Modifiers").emit_signal("pressed");
            if (!required_node<Window>(*this, "ModifiersModal").is_visible() ||
                    !required_node<RichTextLabel>(*this, "ModifiersModal/Text")
                    .get_text()
                    .contains("Dwarven Toughness"))
                throw std::runtime_error("Modifier modal failed");
            const auto text = required_node<RichTextLabel>(*this, "ModifiersModal/Text").get_text();
            if (text.contains("(Score - 10)") || !text.contains("Ability score adjustments"))
                throw std::runtime_error("Ability adjustments still include derived save bonuses");
            for (unsigned i = 0; i < 6; ++i)
            {
                const auto &s = completed_->sheet();
                const auto ability = rules::all_abilities[i];
                const auto heading = "[b]" + std::string(full_abilities[i]) + "[/b]\n";
                if (s.bonuses[ability] == 0)
                {
                    if (text.contains(gs(heading)))
                        throw std::runtime_error("Unadjusted ability shown in modifiers");
                }
                else if (!text.contains(gs(heading + "Rolled score: " +
                                           std::to_string(s.base[ability]) + "\n" +
                                           s.background + " background (" +
                                           signed_number(s.bonuses[ability]) +
                                           ")\nFinal score: " +
                                           std::to_string(s.scores[ability]))))
                    throw std::runtime_error("Adjustment lines do not match approved format");
            }
        }
        else if (modal_check_stage_ == 1)
        {
            if (capture_)
            {
                const auto image =
                    required_node<Window>(*this, "ModifiersModal").get_texture()->get_image();
                if (image.is_valid())
                    image->save_png(ProjectSettings::get_singleton()->globalize_path(
                                        "user://checks/character-modifiers.png"));
            }
            required_node<Button>(*this, "ModifiersModal/Close").emit_signal("pressed");
        }
        else if (modal_check_stage_ == 2)
        {
            if (required_node<Window>(*this, "ModifiersModal").is_visible())
                throw std::runtime_error("Modifier modal did not close");
        }
        else if (modal_check_stage_ == 3)
        {
            required_node<Button>(*this, "SavingThrows").emit_signal("pressed");
            const auto text =
                required_node<RichTextLabel>(*this, "SavingThrowsModal/Text").get_text();
            if (!required_node<Window>(*this, "SavingThrowsModal").is_visible() ||
                    !text.contains("Strength save:") ||
                    !text.contains("Fighter saving throw proficiency"))
                throw std::runtime_error("Saving throw modal omitted sources");
        }
        else if (modal_check_stage_ == 4)
        {
            if (capture_)
            {
                const auto image =
                    required_node<Window>(*this, "SavingThrowsModal").get_texture()->get_image();
                if (image.is_valid())
                    image->save_png(ProjectSettings::get_singleton()->globalize_path(
                                        "user://checks/character-saving-throws.png"));
            }
            auto *dc = &required_node<LineEdit>(*this, "SavingThrowsModal/DC");
            const auto edit = [&](const char *value)
            {
                dc->set_text(value);
                dc->emit_signal("text_changed", String(value));
                return required_node<RichTextLabel>(*this, "SavingThrowsModal/Text").get_text();
            };
            if (!edit("999").contains("Cannot reach this DC") ||
                    !edit("1").contains("Any d20 roll saves") ||
                    !edit("").contains("Enter a whole-number") ||
                    !edit("abc").contains("Enter a whole-number") ||
                    !edit("0").contains("Enter a whole-number"))
                throw std::runtime_error("Saving throw DC changes failed");
            const int needed =
                srd5::minimum_save_roll(15, completed_->sheet().saving_throws[Ability::strength]);
            if (!edit("15").contains(gs("Roll " + std::to_string(needed) + " or higher")))
                throw std::runtime_error("Saving throw DC did not restore");
        }
        else if (modal_check_stage_ == 5)
            required_node<Button>(*this, "SavingThrowsModal/Close").emit_signal("pressed");
        else if (required_node<Window>(*this, "SavingThrowsModal").is_visible())
            throw std::runtime_error("Saving throw modal did not close");
        ++modal_check_stage_;
        return;
    }
    const auto click = [&](Vector2 position)
    {
        for (bool pressed :
                {
                    true, false
                })
        {
            Ref<InputEventMouseButton> event;
            event.instantiate();
            event->set_position(position);
            event->set_global_position(position);
            event->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
            event->set_pressed(pressed);
            get_viewport()->push_input(event, true);
        }
    };
    const auto press = [&](const char *name)
    {
        auto *button = &required_node<Button>(*this, name);
        if (!button->is_visible_in_tree() || button->is_disabled())
            throw std::runtime_error(std::string("Unavailable button: ") + name);
        click(button->get_global_rect().get_center());
    };
    const auto select_size = [&](por::IconSize size)
    {
        auto *sizes = &required_node<OptionButton>(*this, "Size");
        if (!sizes->is_visible_in_tree())
            throw std::runtime_error("Size must be on Combat appearance");
        sizes->select(static_cast<int>(size));
        sizes->emit_signal("item_selected", static_cast<int>(size));
    };
    const auto choose = [&](CreationField field, const char *id)
    {
        const auto choices = creator_->rules().choices(field);
        if (field == CreationField::gender)
        {
            auto *gender = &required_node<OptionButton>(*this, "Gender");
            if (!gender->is_visible_in_tree())
                throw std::runtime_error("Gender must be on Race & Gender");
            for (unsigned i = 0; i < choices.size(); ++i)
                if (choices[i].id == id)
                {
                    gender->emit_signal("item_selected", i);
                    return;
                }
        }
        for (unsigned i = 0; i < choices.size(); ++i)
            if (choices[i].id == id)
            {
                auto *list = &required_node<ItemList>(*this, "Choices");
                click(list->get_global_position() + list->get_item_rect(i).get_center());
                return;
            }
        throw std::runtime_error("Missing UI choice");
    };
    // The portrait picker is checked on its own page, after combat appearance.
    const bool on_portraits = creator_->step() == CreationStep::portrait;
    if (on_portraits && check_head_ < portraits_->entries().size())
    {
        if (check_head_ == 0)
        {
            // Arriving here, the filters show the character's gender and race,
            // of every class; the walk below clears them to step through all.
            const auto &d = creator_->draft();
            if (filtered_portraits_.empty() ||
                    required_node<OptionButton>(*this, "PortraitClass").get_selected() != 0)
                throw std::runtime_error("Portrait filters must start at the character's gender and race");
            for (const auto index : filtered_portraits_)
            {
                const auto &shown = portraits_->entries().at(index);
                if (!PortraitCatalog::same(shown.gender, d.gender) ||
                        !PortraitCatalog::same(shown.race, d.race))
                    throw std::runtime_error("Portrait filters must start at the character's gender and race");
            }
            for (const auto *name : {"PortraitGender", "PortraitRace"})
            {
                auto *filter = &required_node<OptionButton>(*this, name);
                filter->select(0);
                filter->emit_signal("item_selected", 0);
            }
            portrait_selected(0);
        }
        const auto &p = portraits_->entries()[check_head_];
        if (creator_->appearance().portrait != p.filename || images_[0]->get_width() != 1254)
            throw std::runtime_error("Portrait navigation or full-resolution texture failed");
        ++check_head_;
        portrait_part(1);
        return;
    }
    if (on_portraits && check_default_ == 0)
    {
        const auto before = creator_->appearance();
        const auto draft = creator_->draft();
        auto *gender_filter = &required_node<OptionButton>(*this, "PortraitGender");
        auto *class_filter = &required_node<OptionButton>(*this, "PortraitClass");
        auto *race_filter = &required_node<OptionButton>(*this, "PortraitRace");
        if (gender_filter->get_item_count() != 4 || class_filter->get_item_count() != 13 ||
                race_filter->get_item_count() != 10)
            throw std::runtime_error("Portrait filters do not list every gender, class and race");
        for (int gender = 1; gender < gender_filter->get_item_count(); ++gender)
            for (int klass = 1; klass < class_filter->get_item_count(); ++klass)
                for (int race = 1; race < race_filter->get_item_count(); ++race)
                {
                    for (const auto &[filter, index] :
                            {std::pair{gender_filter, gender}, std::pair{class_filter, klass},
                             std::pair{race_filter, race}})
                    {
                        filter->select(index);
                        filter->emit_signal("item_selected", index);
                    }
                    const auto gender_value = String(gender_filter->get_item_metadata(gender));
                    const auto class_value = String(class_filter->get_item_metadata(klass));
                    const auto race_value = String(race_filter->get_item_metadata(race));
                    std::size_t count = 0;
                    for (const auto &p : portraits_->entries())
                        if (gs(p.gender) == gender_value && gs(p.klass) == class_value &&
                                gs(p.race) == race_value)
                            ++count;
                    if (count == 0 || filtered_portraits_.size() != count ||
                            !bool(required_node<Control>(*this, "PortraitScroll/PortraitSheet")
                                  .get("enabled")))
                        throw std::runtime_error("Portrait intersection is missing or cannot be selected");
                    for (const auto index : filtered_portraits_)
                    {
                        const auto &portrait = portraits_->entries().at(index);
                        if (gs(portrait.gender) != gender_value ||
                                gs(portrait.klass) != class_value ||
                                gs(portrait.race) != race_value)
                            throw std::runtime_error("Portrait filter returned the wrong character");
                    }
                    if (creator_->appearance() != before || creator_->draft().race != draft.race ||
                            creator_->draft().gender != draft.gender ||
                            creator_->draft().character_class != draft.character_class)
                        throw std::runtime_error("Filters changed the character");
                }
        for (const auto *name :
                {"PortraitGender", "PortraitClass", "PortraitRace"
                })
        {
            auto *filter = &required_node<OptionButton>(*this, name);
            filter->select(0);
            filter->emit_signal("item_selected", 0);
        }
        if (filtered_portraits_.size() != portraits_->entries().size())
            throw std::runtime_error("Clearing filters failed");
        ++check_default_;
        if (portrait_check_)
        {
            UtilityFunctions::print("Portrait picker check passed: ",
                                    static_cast<std::int64_t>(portraits_->entries().size()),
                                    " portraits and all gender, class and race combinations");
            checking_ = false;
            get_tree()->quit(0);
        }
        return;
    }
    switch (check_stage_++)
    {
    case 0:
        capture("character-race-columns.png");
        if (!required_node<Button>(*this, "Next").is_disabled())
            throw std::runtime_error("Empty name accepted");
        required_node<LineEdit>(*this, "Name").grab_focus();
        for (char c : std::string("Mira Stoneward"))
            for (bool pressed :
                    {
                        true, false
                    })
            {
                Ref<InputEventKey> event;
                event.instantiate();
                event->set_unicode(c);
                event->set_keycode(static_cast<Key>(c >= 'a' && c <= 'z' ? c - 32 : c));
                event->set_pressed(pressed);
                get_viewport()->push_input(event, true);
            }
        break; // LineEdit publishes text_changed on the next idle turn.
    case 1:
        choose(CreationField::race, "dwarf");
        break;
    case 2:
    {
        const auto shown_portrait = [&]
        {
            const auto &entries = portraits_->entries();
            const auto shown = std::find_if(entries.begin(), entries.end(), [&](const auto & p)
            {
                return p.filename == race_portrait_file_;
            });
            return shown == entries.end() ? nullptr : &*shown;
        };
        // The first page shows a portrait of the chosen race beside its traits.
        const auto *shown = shown_portrait();
        if (!required_node<Label>(*this, "RaceSelection").is_visible() || !shown ||
                shown->race != "Dwarf" || race_portrait_of_ != "dwarf" ||
                !required_node<RichTextLabel>(*this, "Description").get_text().contains("Racial traits"))
            throw std::runtime_error("The race page must show a portrait and the traits of the chosen race");
        // Changing gender shows a portrait of the race and the new gender.
        choose(CreationField::gender, "female");
        shown = shown_portrait();
        if (!shown || shown->race != "Dwarf" || shown->gender != "Female")
            throw std::runtime_error("The race page must show a portrait of the chosen gender");
    }
        press("Next");
        break;
    case 3:
        choose(CreationField::alignment, "neutral_good");
        press("Next");
        break;
    case 4:
        if (creator_->step() != CreationStep::attributes)
            throw std::runtime_error("Attributes must precede Class");
        for (unsigned i = 0; i < 12; ++i)
            required_node<CheckBox>(*this, gs("Targets/Rows/Class" + std::to_string(i)))
            .set_pressed(true);
        if (creator_->draft().target_classes.size() != 12 ||
                !required_node<Label>(*this, "Warning3").is_visible())
            throw std::runtime_error(
                "Target checkboxes must accept future goals and show unmet requirements");
        break;
    case 5:
        if (!required_node<Button>(*this, "Next").is_disabled())
            throw std::runtime_error("Unrolled scores accepted");
        capture("character-empty-rolls.png");
        press("Roll");
        if (!required_node<Button>(*this, "Next").is_disabled() ||
                !required_node<Button>(*this, "Score0").get_text().is_empty())
            throw std::runtime_error("Dice were automatically assigned");
        break;
    case 6:
    {
        capture("character-unassigned-rolls.png");
        const auto old = creator_->draft().rolls;
        press("Roll");
        if (old == creator_->draft().rolls)
            throw std::runtime_error("Reroll did not replace dice");
        required_node<OptionButton>(*this, "Background").emit_signal("item_selected", 3);
        required_node<OptionButton>(*this, "Bonus").emit_signal("item_selected", 1);
        break;
    }
    case 7:
    {
        const auto old = creator_->draft().assignment;
        press("Ability0");
        press("Ability2");
        if (creator_->draft().assignment[Ability::strength] != old[Ability::constitution])
            throw std::runtime_error("UI swap failed");
        break;
    }
    case 8:
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto ability = rules::all_abilities[i];
            const bool unmet =
                rules::unmet_targets(creator_->rules(), creator_->draft())[ability];
            if (required_node<Label>(*this, gs("Warning" + std::to_string(i))).is_visible() !=
                    unmet)
                throw std::runtime_error("Target warnings did not follow score assignment");
            const Ref<StyleBoxFlat> style =
                required_node<Button>(*this, gs("Score" + std::to_string(i)))
                .get_theme_stylebox("normal");
            if (style->get_bg_color() != get_theme_color(
                    unmet ? "score_unmet_bg" : "creation_field", "OpenGoldPalette"))
                throw std::runtime_error("Target score background is stale");
        }
        required_node<CheckBox>(*this, "Targets/Rows/Class11").set_pressed(false);
        if (required_node<Label>(*this, "Warning3").is_visible())
            throw std::runtime_error("Removing Wizard target must clear Intelligence warning");
        required_node<CheckBox>(*this, "Targets/Rows/Class11").set_pressed(true);
        for (unsigned i = 0; i < 6; ++i)
            if (required_node<Button>(*this, gs("Score" + std::to_string(i))).get_text() !=
                    gs(std::to_string(creator_->sheet().scores[rules::all_abilities[i]])))
                throw std::runtime_error(
                    "Displayed ability score differs from the character sheet");
        capture("character-attributes.png");
        press("Next");
        break;
    case 9:
        if (creator_->step() != CreationStep::character_class)
            throw std::runtime_error("Attributes must advance to Class");
        choose(CreationField::character_class, "fighter");
        press("Next");
        if (creator_->step() != CreationStep::training
                || !required_node<Button>(*this, "Next").is_disabled())
            throw std::runtime_error("Qualified Fighter must complete Training first");
        required_node<CheckBox>(*this, "Training/Rows/Group1/perception").set_pressed(true);
        required_node<CheckBox>(*this, "Training/Rows/Group1/history").set_pressed(true);
        required_node<CheckBox>(*this, "Training/Rows/Group0/archery").set_pressed(true);
        // A Fighter also masters three weapons.
        for (const char *weapon : {"dagger", "longsword", "greataxe"})
            required_node<CheckBox>(*this, gs(std::string("Training/Rows/Group2/") + weapon))
            .set_pressed(true);
        press("Next");
        if (creator_->step() != CreationStep::combat_icon)
            throw std::runtime_error("Completed training must advance to combat appearance");
        break;
    case 10:
        for (int bank = 0; bank < 2; ++bank)
            for (int part :
                    {
                        0, 3
                    })
                if (required_node<Button>(*this,
                                          gs("Color" + std::to_string(bank) + "_" + std::to_string(part)))
                        .is_disabled())
                    throw std::runtime_error("Weapon and shield colors must stay choosable");
        press("CombatHeadNext");
        for (int i = 0; i < 4; ++i)
            press("BodyNext");
        if (creator_->appearance().combat_body != body_groups_.at(4).front() ||
                required_node<Label>(*this, "BodyLabel").get_text() !=
                String("5 / ") + String::num_uint64(body_groups_.size()))
            throw std::runtime_error("Body buttons must step through the bodies without weapons");
        select_size(por::IconSize::small);
        break;
    case 11:
        capture("character-before-colors.png");
        for (int bank = 0; bank < 2; ++bank)
            for (int part = 0; part < 6; ++part)
            {
                const auto before = creator_->appearance();
                const auto chosen = (before.colors[bank][part] + 3) % 16;
                const auto portrait = images_[0]->get_image()->get_data();
                press(("Color" + std::to_string(bank) + "_" + std::to_string(part)).c_str());
                press(("Palette" + std::to_string(chosen)).c_str());
                if (creator_->appearance().colors[bank][part] != chosen)
                    throw std::runtime_error("Color input did not change the requested region");
                if (images_[0]->get_image()->get_data() != portrait)
                    throw std::runtime_error("Combat colors changed the portrait");
                for (unsigned pose = 0; pose < 2; ++pose)
                {
                    const auto pixels = images_[pose + 1]->get_image()->get_data();
                    const auto expected = art_->equipped_icon(creator_->appearance(), 0,
                                          pose != 0 ? por::IconPose::action
                                          : por::IconPose::ready);
                    if (std::cmp_not_equal(pixels.size(), expected.rgba.size()) ||
                            !std::equal(expected.rgba.begin(), expected.rgba.end(), pixels.ptr()))
                        throw std::runtime_error("Preview texture is stale after palette input");
                }
            }
        break;
    case 12:
        capture("character-appearance.png");
        press("Next");
        if (creator_->step() != CreationStep::portrait)
            throw std::runtime_error("Combat appearance must advance to Portrait");
        break;
    case 13:
    {
        required_node<Control>(*this, "PortraitScroll/PortraitSheet")
        .emit_signal("portrait_chosen", 0);
        const auto chosen = creator_->appearance().portrait;
        press("Back");
        press("Next");
        if (chosen != creator_->appearance().portrait)
            throw std::runtime_error("Manual portrait changed on Back/Next");
        capture("character-portrait.png");
        press("Next");
        break;
    }
    case 14:
        if (creator_->step() != CreationStep::sheet || !completed_ ||
                completed_->sheet().name != "Mira Stoneward" || !completed_->inventory().empty() ||
                completed_->appearance() != creator_->appearance() ||
                completed_->appearance().portrait.empty())
            throw std::runtime_error("Character not completed with the selected portrait");
        capture("character-sheet.png");
        press("Back");
        break;
    case 15:
    {
        press("Back");
        const auto a = creator_->appearance();
        select_size(por::IconSize::regular);
        press("CombatHeadNext");
        if (!required_node<Button>(*this, "Color0_2").is_disabled())
            throw std::runtime_error("Helmet-covered hair control enabled");
        press("CombatHeadPrevious");
        select_size(a.icon_size);
        press("Next");
        press("Next");
        if (a != creator_->appearance() || !completed_ || a != completed_->appearance())
            throw std::runtime_error("Appearance lost on part changes or review");
        press("Restart");
        break;
    }
    case 16:
        if (completed_ || creator_->draft().rolled || !creator_->draft().name.empty() ||
                creator_->step() != CreationStep::race)
            throw std::runtime_error("Start over did not clear the character");
        if (creator_->appearance().portrait != portraits_->recommended(creator_->draft()))
            throw std::runtime_error("Restart did not restore the default race's recommended head");
        UtilityFunctions::print(
            "Godot C++ character check passed: choices, mouse drag assignment, swaps, background, HP, name, complete portraits, race/gender defaults, live texture recolors, saving throws, modifier modal, character/inventory, sheet, edit, restart");
        checking_ = false;
        get_tree()->quit(0);
        break;
    }
}
