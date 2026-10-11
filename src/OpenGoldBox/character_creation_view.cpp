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
    N_("Race & Gender"), N_("Alignment"),         N_("Attributes"),
    N_("Class"),         N_("Training"),          N_("Spell Choices"),
    N_("Name"),          N_("Combat appearance"), N_("Character sheet")};

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

Color ega(unsigned index)
{
    const auto c = por::character_color(index);
    return Color(c[0] / 255.f, c[1] / 255.f, c[2] / 255.f);
}

Ref<StyleBoxFlat> box(Control &control, Color color, Color border, int width = 0)
{
    Ref<StyleBoxFlat> result;
    result.instantiate();
    result->set_bg_color(color);
    result->set_border_color(border);
    result->set_border_width_all(width ? width :
                                 control.get_theme_constant("swatch_border_width", "OpenGoldMetrics"));
    result->set_corner_radius_all(
        control.get_theme_constant("swatch_corner_radius", "OpenGoldMetrics"));
    result->set_content_margin_all(
        control.get_theme_constant("swatch_padding", "OpenGoldMetrics"));
    return result;
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
    required_node<Label>(*this, "PreviewName").set_auto_translate_mode(
        Node::AUTO_TRANSLATE_MODE_DISABLED);
    ready_ = true;
    get_window()->set_min_size(Vector2i(get_theme_constant("creation_min_width", "OpenGoldMetrics"),
                                        get_theme_constant("creation_min_height", "OpenGoldMetrics")));
    set_texture_filter(TEXTURE_FILTER_NEAREST);
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
    required_node<OptionButton>(*this, "PortraitSelect")
    .connect("item_selected", presentation::guarded(this, &CharacterCreationView::portrait_selected));
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
        required_node<Button>(*this, gs(std::string("Portrait") + suffix))
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::portrait_part).bind(direction));
        required_node<Button>(*this, gs(std::string("CombatHead") + suffix))
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::combat_part).bind(0, direction));
        required_node<Button>(*this, gs(std::string("Weapon") + suffix))
        .connect("pressed",
                 presentation::guarded(this, &CharacterCreationView::combat_part).bind(1, direction));
    }
    required_node<Button>(*this, "Size").connect("pressed",
            presentation::guarded(this, &CharacterCreationView::toggle_size));
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
        button->add_theme_stylebox_override("normal", box(*button, ega(i),
            get_theme_color("swatch_border", "OpenGoldPalette")));
        button->add_theme_stylebox_override("hover", box(*button, ega(i),
            get_theme_color("score_active_border", "OpenGoldPalette"),
            get_theme_constant("swatch_selected_border_width", "OpenGoldMetrics")));
        button->add_theme_stylebox_override("focus", box(*button,
            get_theme_color("swatch_focus_bg", "OpenGoldPalette"),
            get_theme_color("swatch_hover", "OpenGoldPalette"),
            get_theme_constant("swatch_focus_border_width", "OpenGoldMetrics")));
    }
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    checking_ = args.has("--character-check");
    capture_ = args.has("--capture");
    try
    {
        const auto directory = settings::game_path();
        art_ = por::CharacterArt::load(presentation::path_from_godot(directory));
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
    // Godot anchors and offsets own the default rectangles. Restore them when
    // leaving a step whose scene-authored variant moved shared controls.
    for (int i = 0; i < get_child_count(); ++i)
        if (auto *control = Object::cast_to<Control>(get_child(i));
                control && control->has_meta("layout_reference"))
            presentation::restore_scene_control(*control);

    const auto rect = [&](const char *name)
    {
        auto &control = required_node<Control>(*this, name);
        return Rect2(control.get_position(), control.get_size());
    };
    page_rect_ = rect("PageBounds");
    preview_rect_ = rect("PreviewBounds");
    portrait_rect_ = rect("PortraitBounds");
    ready_rect_ = rect("ReadyBounds");
    action_rect_ = rect("ActionBounds");

    const auto apply_variant = [&](const char *group)
    {
        auto &guides = required_node<Control>(*this, group);
        for (int i = 0; i < guides.get_child_count(); ++i)
        {
            auto *guide = Object::cast_to<Control>(guides.get_child(i));
            if (!guide)
                continue;
            auto &target = required_node<Control>(*this, NodePath(guide->get_name()));
            target.set_position(guide->get_position());
            target.set_size(guide->get_size());
        }
    };
    if (creator_)
    {
        if (creator_->step() == CreationStep::race)
            apply_variant("RaceLayout");
        else if (creator_->step() == CreationStep::attributes)
            apply_variant("AttributeLayout");
        else if (creator_->step() == CreationStep::sheet)
            apply_variant("SheetLayout");
    }

    // Split the available width among the scene-authored number of columns.
    auto &choices = required_node<ItemList>(*this, "Choices");
    if (!choices.has_meta("_layout_column_width"))
        choices.set_meta("_layout_column_width", choices.get_fixed_column_width());
    const Rect2 page_design = required_node<Control>(*this, "PageBounds").get_meta("layout_reference");
    choices.set_fixed_column_width(int(choices.get_meta("_layout_column_width")) +
                                   static_cast<int>((page_rect_.size.x - page_design.size.x) /
                                                    std::max(1, choices.get_max_columns())));
    required_node<Control>(*this, "PreviewSummary").set_visible(
        page_rect_.size.y >= get_theme_constant("preview_summary_min_height", "OpenGoldMetrics"));

    const double max_width = get_theme_constant("creation_modal_max_width", "OpenGoldMetrics");
    const double horizontal_margin = get_theme_constant("creation_modal_horizontal_margin", "OpenGoldMetrics");
    const double vertical_margin = get_theme_constant("creation_modal_vertical_margin", "OpenGoldMetrics");
    const Vector2i modal_size(std::min(max_width, get_size().x - horizontal_margin),
                              get_size().y - vertical_margin);
    presentation::size_scene_window(required_node<Window>(*this, "ModifiersModal"), modal_size);
    presentation::size_scene_window(required_node<Window>(*this, "SavingThrowsModal"), modal_size);
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

void CharacterCreationView::refresh_art()
{
    if (!creator_ || !art_ || rendered_ == creator_->appearance())
        return;
    const auto &a = creator_->appearance();
    const std::array<opengold::Image, 3> images{opengold::Image{}, art_->icon(a, por::IconPose::ready),
            art_->icon(a, por::IconPose::action)};
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
                          stats = step == CreationStep::attributes, icon = step == CreationStep::combat_icon;
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
    show("Name", step == CreationStep::name);
    show("SpellChoices", step == CreationStep::spell_choices);
    show("TrainingFixed", step == CreationStep::training);
    show("Training", step == CreationStep::training);
    for (const auto *n :
            {"PortraitPrevious", "PortraitNext", "PortraitSelect", "PortraitGender",
             "PortraitClass", "PortraitRace"
            })
        show(n, true);
    for (const auto *n :
            {"PortraitPrevious", "PortraitNext", "PortraitSelect", "PortraitGender",
             "PortraitClass", "PortraitRace"
            })
        required_node<Button>(*this, n).set_disabled(added_to_party_);
    for (const auto *n :
            {"CombatHeadPrevious", "CombatHeadNext", "CombatHeadLabel", "WeaponPrevious", "WeaponNext",
             "WeaponLabel", "Size", "ColorTitle", "Color1Title", "Color2Title", "PaletteHint"
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
        progress +=
            (i == static_cast<unsigned>(step) ? "> " : "  ") + std::to_string(++ordinal) + ". " +
            i18n::utf8(i == static_cast<unsigned>(CreationStep::combat_icon) ? N_("Combat icon")
                       : steps[i]) +
            "\n\n";
    }
    required_node<Label>(*this, "Steps").set_text(gs(progress));
    required_node<Button>(*this, "Back").set_disabled(step == CreationStep::race);
    required_node<Button>(*this, "Back").set_text(
        i18n::text(step == CreationStep::sheet ? N_("Edit appearance") : N_("Back")));
    show("Next", step != CreationStep::sheet);
    if (campaign_)
        required_node<Button>(*this, "AddParty").set_visible(step == CreationStep::sheet
                && !added_to_party_);
    required_node<Button>(*this, "Next").set_text(i18n::text(icon ? N_("Show character sheet") :
            N_("Next")));
    required_node<Button>(*this, "Next").set_disabled(
        (stats && !creator_->scores_assigned()) ||
        (step == CreationStep::character_class &&
         !rules::class_eligible(creator_->rules(), d, d.character_class)) ||
        (step == CreationStep::training && !creator_->training_complete()) ||
        (step == CreationStep::spell_choices && !creator_->spell_choices_complete()) ||
        (step == CreationStep::name && d.name.empty()));
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
            : step == CreationStep::race ? N_("Choose your race (species in SRD 5.2.1) and gender.")
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
            check->set_text(i18n::text(targets[i].label) + "\n" + i18n::requirements(requirements));
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
            score_box->set_theme_type_variation(unmet ? "ScoreUnmet" : "ScoreNormal");
            const int change =
                score ? *score - d.rolls[d.assignment[ability]].total() : 0;
            const auto color = get_theme_color(change > 0 ? "score_positive"
                                               : change < 0 ? "score_negative" : "score_neutral",
                                               "OpenGoldPalette");
            for (const auto *state :
                    {"font_color", "font_hover_color", "font_pressed_color", "font_focus_color"
                    })
                score_box->add_theme_color_override(state, color);
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
    if (step == CreationStep::training)
    {
        presentation::refresh_training_controls(
            *this, *creator_, presentation::guarded(this, &CharacterCreationView::training_toggled),
            presentation::guarded(this, &CharacterCreationView::training_selected),
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
            *book, creator_->rules().spell_choice_options(d),
            d.spells.value_or(opengold::rules::SpellChoices{}),
            presentation::guarded(this, &CharacterCreationView::creation_spell_toggled),
            i18n::text);
        instructions =
            d.spells && (d.character_class == "paladin" || d.character_class == "ranger")
            ? N_("Choose prepared spells. Unfilled choices remain pending; Back preserves your choices.")
            : d.spells && (d.character_class == "cleric" || d.character_class == "sorcerer" ||
                           d.character_class == "warlock" || d.character_class == "bard" ||
                           d.character_class == "druid")
            ? N_("Choose cantrips and prepared spells. Unfilled choices remain pending; Back preserves your choices.")
            : d.spells
            ? N_("Choose cantrips, spellbook entries and prepared spells. Back preserves your choices.")
            : N_("Choose your available cantrips. Unfilled choices remain pending; Back preserves your selections.");
    }
    if (step == CreationStep::name)
        instructions = N_("Choose a name for your character (up to 40 characters).");
    if (icon)
        instructions = N_(
                           "Select a part's Color-1 or Color-2, then a swatch. Watch both poses change. Absent parts are disabled.");
    refresh_portraits();
    required_node<Label>(*this, "CombatHeadLabel")
    .set_text(i18n::format("Head {number} / 14", {{"number", a.combat_head + 1}}));
    required_node<Label>(*this, "WeaponLabel")
    .set_text(i18n::format("Weapon {number} / 35", {{"number", a.combat_body + 1}}));
    required_node<Button>(*this, "Size").set_text(i18n::text(a.tall ? N_("Size: Tall") :
            N_("Size: Short")));
    if (icon)
    {
        const auto usage = art_->color_usage(a);
        if (!usage.contains(color_bank_, color_part_))
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
                const auto color = ega(a.colors[bank][part]);
                const bool present = usage.contains(bank, part);
                button->set_disabled(!present);
                button->set_tooltip_text(i18n::text(
                                             present
                                             ? N_("Choose a swatch to recolor this part in the combat preview.")
                                             : N_("This part is not present in either pose. Choose another head or weapon to use it.")));
                button->set_text(present ? String(selected ? "> " : "") +
                                 i18n::text(colors[a.colors[bank][part]])
                                 : i18n::text("Not present"));
                button->add_theme_stylebox_override(
                    "normal",
                    box(*button, color,
                        get_theme_color(selected ? "swatch_selected" : "swatch_border",
                                        "OpenGoldPalette"),
                        get_theme_constant(selected ? "swatch_selected_border_width"
                                           : "swatch_border_width", "OpenGoldMetrics")));
                button->add_theme_stylebox_override("hover", box(*button, color,
                    get_theme_color("swatch_hover", "OpenGoldPalette"),
                    get_theme_constant("swatch_hover_border_width", "OpenGoldMetrics")));
                button->add_theme_stylebox_override("disabled",
                    box(*button, get_theme_color("swatch_disabled_bg", "OpenGoldPalette"),
                        get_theme_color("swatch_disabled_border", "OpenGoldPalette")));
                const double contrast =
                    (color.r * get_theme_constant("swatch_red_luma", "OpenGoldMetrics") +
                     color.g * get_theme_constant("swatch_green_luma", "OpenGoldMetrics") +
                     color.b * get_theme_constant("swatch_blue_luma", "OpenGoldMetrics"));
                const auto foreground = contrast >
                                        get_theme_constant("swatch_contrast_threshold", "OpenGoldMetrics")
                                        ? get_theme_color("swatch_dark_text", "OpenGoldPalette")
                                        : get_theme_color("swatch_light_text", "OpenGoldPalette");
                button->add_theme_color_override("font_color", foreground);
                button->add_theme_color_override("font_hover_color", foreground);
            }
    }
    if (step == CreationStep::sheet)
    {
        instructions = N_(
                           "Review your character and use the portrait controls to choose a complete portrait before adding it to the party.");
        required_node<RichTextLabel>(*this, "Description")
        .set_text(presentation::sheet_text(*completed_));
    }
    required_node<Label>(*this, "Instructions").set_text(i18n::text(instructions));
    required_node<Label>(*this, "PreviewTitle")
    .set_text(i18n::text(icon ? N_("COMBAT PREVIEW") : N_("CHARACTER PREVIEW")));
    required_node<Label>(*this, "PreviewName")
    .set_text(d.name.empty() ? i18n::text("Unnamed character") : gs(d.name));
    std::string identity;
    for (const auto field :
            {
                CreationField::race, CreationField::gender, CreationField::character_class
            })
        for (const auto &choice : creator_->rules().choices(field))
            if (choice.id == selection(d, field))
                identity += (identity.empty() ? "" : " / ") + i18n::utf8(choice.label);
    required_node<Label>(*this, "PreviewSummary")
    .set_text(gs(identity) + (s ? "\n" + i18n::format("{alignment} / {hp} HP",
    {
        {"alignment", i18n::text(s->alignment)},
        {"hp", s->hit_points}
    })
    : String()));
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
    draw_rect(Rect2(Vector2(), get_size()), get_theme_color("background", "OpenGoldPalette"));
    for (const auto &rect :
            {
                page_rect_, preview_rect_
            })
    {
        draw_rect(rect, get_theme_color("panel", "OpenGoldPalette"));
        draw_rect(rect, get_theme_color("creation_panel_border", "OpenGoldPalette"), false);
    }
    for (const auto &rect :
            {
                ready_rect_, action_rect_
            })
        draw_rect(rect, get_theme_color("creation_field", "OpenGoldPalette"));
    if (creator_ && creator_->step() == CreationStep::attributes)
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto rect =
                required_node<Control>(*this, gs("Dice" + std::to_string(i))).get_rect();
            draw_rect(rect, get_theme_color("creation_field", "OpenGoldPalette"));
            draw_rect(rect, get_theme_color("creation_border", "OpenGoldPalette"), false);
        }
    {
        draw_rect(portrait_rect_, get_theme_color("creation_field", "OpenGoldPalette"));
        if (images_[0].is_valid())
            draw_texture_rect(images_[0], portrait_rect_, false);
    }
    if (images_[1].is_valid())
        draw_texture_rect(images_[1], ready_rect_, false);
    if (images_[2].is_valid())
        draw_texture_rect(images_[2], action_rect_, false);
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

void CharacterCreationView::training_selected(std::int64_t index, String group)
{
    if (refreshing_ || !creator_ || creator_->step() != CreationStep::training || index <= 0)
        return;
    perform(
        [&]
    {
        const auto groups = creator_->rules().training_options(creator_->draft());
        const auto found = std::find_if(groups.begin(), groups.end(),
                                        [&](const auto & g)
        {
            return g.id == group.utf8().get_data();
        });
        if (found == groups.end() ||
                found->control != opengold::rules::TrainingChoiceControl::single_selection ||
                static_cast<std::size_t>(index) > found->options.size())
            throw std::runtime_error("Unknown training choice");
        creator_->training_choice({.group = found->id,
                                   .option = found->options[index - 1].id,
                                   .selected = true});
    });
}

void CharacterCreationView::training_toggled(bool selected, String group, String option)
{
    if (refreshing_ || !creator_ || creator_->step() != CreationStep::training)
        return;
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
        auto &id = part == 0 ? a.combat_head : a.combat_body;
        const int count = part == 0 ? 14 : 35;
        id = (static_cast<int>(id) + direction + count) % count;
        creator_->appearance(a);
    });
}

void CharacterCreationView::toggle_size()
{
    perform(
        [&]
    {
        auto a = creator_->appearance();
        a.tall = !a.tall;
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
    }
    catch (const std::exception &e)
    {
        error_ = i18n::text(e.what());
        UtilityFunctions::push_error(error_);
        if (expedition_check_ || party_check_ || defeat_check_)
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
    if (check_stage_ == 6 && drag_check_stage_ < 12)
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
    if (check_stage_ == 0 && check_head_ < portraits_->entries().size())
    {
        if (check_head_ == 0)
        {
            capture("character-race-columns.png");
            portrait_selected(0);
        }
        const auto &p = portraits_->entries()[check_head_];
        if (creator_->appearance().portrait != p.filename || images_[0]->get_width() != 1254)
            throw std::runtime_error("Portrait navigation or full-resolution texture failed");
        ++check_head_;
        press("PortraitNext");
        return;
    }
    if (check_stage_ == 0 && check_default_ == 0)
    {
        const auto before = creator_->appearance();
        const auto draft = creator_->draft();
        for (const auto *gender :
                {"Female", "Male", "Nonbinary"
                })
            for (const auto *klass :
                    {"Barbarian", "Wizard"
                    })
                for (const auto *race :
                        {"Human", "Orc", "Dragonborn"
                        })
                {
                    for (const auto &filter : std::array<std::pair<const char *, const char *>, 3>
                {
                    {   {"PortraitGender", gender},
                        {"PortraitClass", klass},
                        {"PortraitRace", race}
                    }
                })
                    {
                        auto *c = &required_node<OptionButton>(*this, filter.first);
                        for (int i = 1; i < c->get_item_count(); ++i)
                            if (c->get_item_text(i) == filter.second)
                            {
                                c->select(i);
                                c->emit_signal("item_selected", i);
                                break;
                            }
                    }
                    std::size_t count = 0;
                    for (const auto &p : portraits_->entries())
                        if (p.gender == gender && p.klass == klass && p.race == race)
                            ++count;
                    if (filtered_portraits_.size() != count ||
                            required_node<Button>(*this, "PortraitNext").is_disabled() !=
                            (count == 0))
                        throw std::runtime_error("Portrait intersection or empty-state failed");
                    if (creator_->appearance() != before || creator_->draft().race != draft.race ||
                            creator_->draft().gender != draft.gender ||
                            creator_->draft().character_class != draft.character_class)
                        throw std::runtime_error("Filters changed the character");
                }
        for (const auto *name :
                {"PortraitGender", "PortraitClass", "PortraitRace"
                })
        {
            required_node<OptionButton>(*this, name).select(0);
            portrait_filter_selected(0);
        }
        if (filtered_portraits_.size() != portraits_->entries().size())
            throw std::runtime_error("Clearing filters failed");
        ++check_default_;
        restart();
        return;
    }
    switch (check_stage_++)
    {
    case 0:
        choose(CreationField::race, "dwarf");
        break;
    case 1:
        choose(CreationField::gender, "female");
        press("Next");
        break;
    case 2:
        choose(CreationField::alignment, "neutral_good");
        press("Next");
        break;
    case 3:
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
    case 4:
        if (!required_node<Button>(*this, "Next").is_disabled())
            throw std::runtime_error("Unrolled scores accepted");
        capture("character-empty-rolls.png");
        press("Roll");
        if (!required_node<Button>(*this, "Next").is_disabled() ||
                !required_node<Button>(*this, "Score0").get_text().is_empty())
            throw std::runtime_error("Dice were automatically assigned");
        break;
    case 5:
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
    case 6:
    {
        const auto old = creator_->draft().assignment;
        press("Ability0");
        press("Ability2");
        if (creator_->draft().assignment[Ability::strength] != old[Ability::constitution])
            throw std::runtime_error("UI swap failed");
        break;
    }
    case 7:
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
    case 8:
        if (creator_->step() != CreationStep::character_class)
            throw std::runtime_error("Attributes must advance to Class");
        choose(CreationField::character_class, "fighter");
        press("Next");
        if (creator_->step() != CreationStep::training
                || !required_node<Button>(*this, "Next").is_disabled())
            throw std::runtime_error("Qualified Fighter must complete Training before Name");
        required_node<CheckBox>(*this, "Training/Rows/Group1/athletics").set_pressed(true);
        required_node<CheckBox>(*this, "Training/Rows/Group1/history").set_pressed(true);
        required_node<OptionButton>(*this, "Training/Rows/Group0/Choice").select(2);
        required_node<OptionButton>(*this, "Training/Rows/Group0/Choice")
        .emit_signal("item_selected", 2);
        // A Fighter also masters three weapons.
        for (const char *weapon : {"dagger", "longsword", "greataxe"})
            required_node<CheckBox>(*this, gs(std::string("Training/Rows/Group2/") + weapon))
            .set_pressed(true);
        press("Next");
        if (creator_->step() != CreationStep::name)
            throw std::runtime_error("Completed training must advance to Name");
        break;
    case 9:
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
    case 10:
        press("Next");
        required_node<OptionButton>(*this, "PortraitSelect").select(0);
        required_node<OptionButton>(*this, "PortraitSelect").emit_signal("item_selected", 0);
        press("PortraitPrevious");
        break;
    case 11:
    {
        auto *list = &required_node<OptionButton>(*this, "PortraitSelect");
        list->select(0);
        list->emit_signal("item_selected", 0);
        const auto chosen = creator_->appearance().portrait;
        press("Back");
        press("Next");
        if (chosen != creator_->appearance().portrait)
            throw std::runtime_error("Manual portrait changed on Back/Next");
    }
    if (creator_->step() != CreationStep::combat_icon)
        throw std::runtime_error("Name must advance directly to combat appearance");
    for (int bank = 0; bank < 2; ++bank)
        for (int part :
                {
                    0, 3
                })
            if (!required_node<Button>(*this,
                                       gs("Color" + std::to_string(bank) + "_" + std::to_string(part)))
                    .is_disabled())
                throw std::runtime_error("Absent weapon/shield control enabled");
    press("CombatHeadNext");
    for (int i = 0; i < 4; ++i)
        press("WeaponNext");
    press("Size");
    break;
    case 12:
        capture("character-before-colors.png");
        for (int bank = 0; bank < 2; ++bank)
            for (int part = 0; part < 6; ++part)
            {
                const auto before = creator_->appearance();
                const auto usage = art_->color_usage(before);
                const auto chosen = (before.colors[bank][part] + 3) % 16;
                const auto portrait = images_[0]->get_image()->get_data();
                const std::array<PackedByteArray, 2> old{images_[1]->get_image()->get_data(),
                        images_[2]->get_image()->get_data()};
                press(("Color" + std::to_string(bank) + "_" + std::to_string(part)).c_str());
                press(("Palette" + std::to_string(chosen)).c_str());
                if (creator_->appearance().colors[bank][part] != chosen)
                    throw std::runtime_error("Color input did not change the requested region");
                if (images_[0]->get_image()->get_data() != portrait)
                    throw std::runtime_error("Combat colors changed the portrait");
                for (unsigned pose = 0; pose < 2; ++pose)
                {
                    const auto pixels = images_[pose + 1]->get_image()->get_data();
                    const auto expected = art_->icon(creator_->appearance(),
                                                              pose != 0 ? por::IconPose::action
                                                              : por::IconPose::ready);
                    if (std::cmp_not_equal(pixels.size(), expected.rgba.size()) ||
                            !std::equal(expected.rgba.begin(), expected.rgba.end(), pixels.ptr()))
                        throw std::runtime_error("Preview texture is stale after palette input");
                    unsigned changed = 0;
                    for (int64_t p = 0; p < pixels.size(); p += 4)
                        if (!std::equal(pixels.ptr() + p, pixels.ptr() + p + 4,
                                        old[pose].ptr() + p))
                            ++changed;
                    if (changed != (pose ? usage.action : usage.ready)[bank][part])
                        throw std::runtime_error(
                            "Recolor changed the wrong number of visible pixels");
                }
            }
        break;
    case 13:
        capture("character-appearance.png");
        press("Next");
        break;
    case 14:
        if (creator_->step() != CreationStep::sheet || !completed_ ||
                completed_->sheet().name != "Mira Stoneward" || !completed_->inventory().empty() ||
                completed_->appearance() != creator_->appearance() ||
                completed_->appearance().portrait.empty())
            throw std::runtime_error("Character not completed with the selected portrait");
        {
            const auto before = completed_->appearance();
            press("PortraitNext");
            if (completed_->appearance() == before ||
                    completed_->appearance() != creator_->appearance() ||
                    creator_->step() != CreationStep::sheet)
                throw std::runtime_error("Portrait controls did not update the reviewed character");
            press("PortraitPrevious");
        }
        capture("character-sheet.png");
        press("Back");
        break;
    case 15:
    {
        const auto a = creator_->appearance();
        press("Size");
        press("CombatHeadNext");
        if (!required_node<Button>(*this, "Color0_2").is_disabled())
            throw std::runtime_error("Helmet-covered hair control enabled");
        press("CombatHeadPrevious");
        press("Size");
        press("Next");
        if (a != creator_->appearance() || a != completed_->appearance())
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
