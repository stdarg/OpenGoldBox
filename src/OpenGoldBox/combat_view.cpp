#include "initiative_controls.h"
#include "optional_effect_controls.h"
#include "combat_weapon_controls.h"
#include "nick_controls.h"
#include "godot_images.h"
#include "hp_presentation.h"
#include "application_settings.h"
#include "localization.h"
#include <godot_cpp/classes/popup_menu.hpp>
#include "game_resources.h"
#include "combat_view.h"
#include "combat_sprite_layout.h"
#include "godot_sound_output.h"
#include "opengold/srd5.h"
#include "opengold/character_art.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/formats.h"
#include "opengold/save_file.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/scroll_bar.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/panel_container.hpp>
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
#include <set>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
using namespace godot;
using namespace opengold;
using namespace opengold::rules;

namespace
{
String gs(std::string_view text)
{
    return String::utf8(text.data(), text.size());
}

const std::array<std::pair<const char *, const char *>, 10> action_buttons
{
    {   {"Melee", "melee"},
        {"Ranged", "ranged"},
        {"MagicMissile", "magic_missile"},
        {"CureWounds", "cure_wounds"},
        {"HealingWord", "healing_word"},
        {"ScorchingRay", "scorching_ray"},
        {"Blindness", "blindness"},
        {"Dash", "dash"},
        {"Dodge", "dodge"},
        {"Disengage", "disengage"}
    }};

std::string spell_verb(std::string verb, unsigned slot)
{
    if (slot == 2 && (verb == "magic_missile" || verb == "cure_wounds" || verb == "healing_word"))
        verb += "_2";
    return verb;
}

std::optional<Cell> movement_direction(Key key, bool shift)
{
    switch (key)
    {
    case Key::KEY_UP:
        return shift ? Cell{1, -1} :
               Cell{0, -1};
    case Key::KEY_RIGHT:
        return shift ? Cell{1, 1} :
               Cell{1, 0};
    case Key::KEY_DOWN:
        return shift ? Cell{-1, 1} :
               Cell{0, 1};
    case Key::KEY_LEFT:
        return shift ? Cell{-1, -1} :
               Cell{-1, 0};
    case Key::KEY_INSERT:
    case Key::KEY_KP_7:
        return Cell{-1, -1};
    case Key::KEY_KP_8:
        return Cell{0, -1};
    case Key::KEY_PAGEUP:
    case Key::KEY_KP_9:
        return Cell{1, -1};
    case Key::KEY_KP_4:
        return Cell{-1, 0};
    case Key::KEY_KP_6:
        return Cell{1, 0};
    case Key::KEY_DELETE:
    case Key::KEY_KP_1:
        return Cell{-1, 1};
    case Key::KEY_KP_2:
        return Cell{0, 1};
    case Key::KEY_PAGEDOWN:
    case Key::KEY_KP_3:
        return Cell{1, 1};
    default:
        return std::nullopt;
    }
}
} // namespace

void CombatView::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("selected_character_id"), &CombatView::selected_character_id);
    ClassDB::bind_method(D_METHOD("selected_character_cell"), &CombatView::selected_character_cell);
    ClassDB::bind_method(D_METHOD("attack_pose_active", "id"), &CombatView::attack_pose_active);
    ClassDB::bind_method(D_METHOD("sprite_facing_left", "id"), &CombatView::sprite_facing_left);
}

Vector2i CombatView::selected_character_cell() const
{
    if (!demo_ || !demo_->has_combat())
        return {-1, -1};
    const auto state = demo_->combat().snapshot();
    const auto selected = std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == selected_;
    });
    return selected == state.combatants.end() ? Vector2i(-1, -1)
           : Vector2i(selected->cell.x, selected->cell.y);
}

bool CombatView::sprite_facing_left(std::int64_t id) const
{
    if (!demo_ || !demo_->has_combat())
        return false;
    const auto state = demo_->combat().snapshot();
    const auto actor = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto & a)
    {
        return a.id == static_cast<EntityId>(id);
    });
    return actor != state.combatants.end() && actor->facing_left;
}

Ref<Texture2D> CombatView::sprite_texture(EntityId id, bool action) const
{
    const auto found = art_.find(id);
    if (found == art_.end())
        return {};
    return action ? found->second.action : found->second.texture;
}

void CombatView::prepare_combat()
{
    if (demo_)
        return;
    auto next = std::make_unique<CombatDemo>(
                    srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data())));
    const bool demo_mode = settings::flag("--combat-demo");
    if (demo_mode)
    {
        const auto directory = std::filesystem::u8path(settings::game_path().utf8().get_data());
        auto characters = srd5::character_rules();
        auto showcase = make_combat_demo(
                            srd5::load(std::filesystem::u8path(game_rules_file().utf8().get_data())), *characters,
                            directory, std::filesystem::u8path(game_combat_body_file().utf8().get_data()));
        campaign_ = std::move(showcase.party);
        encounter_ = std::move(showcase.encounter);
    }
    if (campaign_)
        next->campaign_party(campaign_);
    if (encounter_)
        next->encounter(*encounter_, 42);
    else if (OS::get_singleton()->get_cmdline_user_args().has("--slums"))
        next->slums(std::filesystem::u8path(settings::game_path().utf8().get_data()));
    else
        next->training(settings::flag("--conditions") ? 3 : 42, settings::flag("--conditions"));
    demo_ = std::move(next);
    sync_art();
}

void CombatView::_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && ready_)
    {
        layout();
        queue_redraw();
        update_hover(get_viewport()->get_mouse_position());
    }
    if (what == NOTIFICATION_MOUSE_EXIT && ready_)
        get_node<PanelContainer>("HoverInfo")->hide();
}

std::filesystem::path CombatView::local_path(const char *path) const
{
    return std::filesystem::u8path(
               ProjectSettings::get_singleton()->globalize_path(path).utf8().get_data());
}

void CombatView::_ready()
{
    combat_zoom_ = settings::combat_zoom_percent() / 100.0;
    i18n::prepare_ui(*this);
    get_node<Control>("BattlefieldScroll/Canvas")
    ->connect("draw", callable_mp(this, &CombatView::draw_battlefield));
    auto *hover = get_node<PanelContainer>("HoverInfo");
    hover->set_custom_minimum_size(Vector2(260, 88));
    hover->set_size(Vector2(260, 88));
    Ref<StyleBoxFlat> hover_style;
    hover_style.instantiate();
    hover_style->set_bg_color(Color(.06, .09, .12, .97));
    hover_style->set_border_color(Color(.48, .66, .68));
    hover_style->set_border_width_all(1);
    hover_style->set_corner_radius_all(4);
    hover_style->set_content_margin_all(10);
    hover->add_theme_stylebox_override("panel", hover_style);
    presentation::setup_nick(
        *this, i18n::text, callable_mp(this, &CombatView::begin_nick),
        callable_mp(this, &CombatView::nick_selected), callable_mp(this, &CombatView::confirm_nick),
        callable_mp(this, &CombatView::cancel_nick), callable_mp(this, &CombatView::nick_input));
    presentation::setup_initiative(
        *this, i18n::text, callable_mp(this, &CombatView::immediate).bind("initiative_swap"),
        callable_mp(this, &CombatView::immediate).bind("initiative_keep"),
        callable_mp(this, &CombatView::initiative_input),
        callable_mp(this, &CombatView::refresh).unbind(1));
    presentation::setup_optional_effect(
        *this, i18n::text, callable_mp(this, &CombatView::immediate).bind("effect_use"),
        callable_mp(this, &CombatView::immediate).bind("effect_skip"),
        callable_mp(this, &CombatView::optional_effect_input),
        callable_mp(this, &CombatView::refresh).unbind(1));
    presentation::setup_weapon_controls(*this, i18n::text,
                                        callable_mp(this, &CombatView::weapon_selected));
    ready_ = true;
    get_window()->set_min_size(Vector2i(1120, 800));
    set_texture_filter(TEXTURE_FILTER_NEAREST);
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    for (const auto &[node, verb] : action_buttons)
        get_node<Button>(node)->connect(
            "pressed", callable_mp(this, &CombatView::select_mode).bind(String(verb)));
    get_node<OptionButton>("Cantrip")->connect("item_selected",
            callable_mp(this, &CombatView::cantrip_selected));
    get_node<Button>("CastCantrip")
    ->connect("pressed", callable_mp(this, &CombatView::cast_cantrip));
    get_node<Button>("Stabilize")
    ->connect("pressed", callable_mp(this, &CombatView::select_mode).bind("stabilize"));
    get_node<Button>("TacticalMind/Use")
    ->connect("pressed", callable_mp(this, &CombatView::immediate).bind("mind_use"));
    get_node<Button>("TacticalMind/Skip")
    ->connect("pressed", callable_mp(this, &CombatView::immediate).bind("mind_skip"));
    get_node<Button>("StandUp")->connect(
        "pressed", callable_mp(this, &CombatView::immediate).bind("stand_up"));
    get_node<OptionButton>("ThrownWeapon")
    ->connect("item_selected", callable_mp(this, &CombatView::thrown_selected));
    get_node<Button>("Throw")->connect("pressed", callable_mp(this, &CombatView::begin_throw));
    get_node<Button>("UseCunningAction")
    ->connect("pressed", callable_mp(this, &CombatView::use_cunning_action));
    get_node<OptionButton>("CunningAction")
    ->connect("item_selected", callable_mp(this, &CombatView::cunning_selected));
    get_node<Button>("ActionSurge")
    ->connect("pressed",
              callable_mp(this, &CombatView::immediate).bind(String("action_surge")));
    get_node<Button>("AdrenalineRush")
    ->connect("pressed",
              callable_mp(this, &CombatView::immediate).bind(String("adrenaline_rush")));
    get_node<Button>("TemporaryHP/Keep")
    ->connect("pressed",
              callable_mp(this, &CombatView::immediate).bind(String("temp_hp_keep")));
    get_node<Button>("TemporaryHP/Use")
    ->connect("pressed", callable_mp(this, &CombatView::immediate).bind(String("temp_hp_use")));
    get_node<Button>("Move")->connect(
        "pressed", callable_mp(this, &CombatView::select_mode).bind(String("move")));
    get_node<Button>("SpellSlot")->connect("pressed", callable_mp(this, &CombatView::spell_slot));
    get_node<Button>("SecondWind")
    ->connect("pressed", callable_mp(this, &CombatView::immediate).bind(String("second_wind")));
    get_node<Button>("End")->connect("pressed",
                                     callable_mp(this, &CombatView::immediate).bind(String("end")));
    get_node<Button>("React")->connect(
        "pressed", callable_mp(this, &CombatView::immediate).bind(String("opportunity")));
    get_node<Button>("Decline")->connect(
        "pressed", callable_mp(this, &CombatView::immediate).bind(String("decline")));
    get_node<Button>("Training")->connect("pressed", callable_mp(this, &CombatView::training));
    get_node<Button>("Slums")->connect("pressed", callable_mp(this, &CombatView::slums));
    get_node<Button>("Replay")->connect("pressed", callable_mp(this, &CombatView::replay));
    get_node<Button>("Continue")->connect("pressed", callable_mp(this, &CombatView::next));
    get_node<Button>("Revisit")->connect("pressed", callable_mp(this, &CombatView::revisit));
    get_node<Button>("Save")->connect("pressed", callable_mp(this, &CombatView::save_game));
    get_node<Button>("Load")->connect("pressed", callable_mp(this, &CombatView::load_game));
    get_node<Button>("ZoomOut100")
    ->connect("pressed", callable_mp(this, &CombatView::adjust_zoom).bind(-100));
    get_node<Button>("ZoomOut10")
    ->connect("pressed", callable_mp(this, &CombatView::adjust_zoom).bind(-10));
    get_node<Button>("ZoomIn10")
    ->connect("pressed", callable_mp(this, &CombatView::adjust_zoom).bind(10));
    get_node<Button>("ZoomIn100")
    ->connect("pressed", callable_mp(this, &CombatView::adjust_zoom).bind(100));
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    checking_ = args.has("--combat-check");
    capture_ = args.has("--capture");
    check_slums_ = args.has("--slums");
    party_check_ = campaign_ && args.has("--party-check");
    expedition_check_ = campaign_ && args.has("--expedition-check");
    party_check_ |= expedition_check_;
    defeat_check_ = campaign_ && args.has("--defeat-check");
    try
    {
        prepare_combat();
        layout();
        refresh();
        const auto directory = std::filesystem::u8path(settings::game_path().utf8().get_data());
        if (std::filesystem::is_directory(directory))
        {
            attack_sound_ = std::make_unique<por::SoundPlayer>(
                                por::SoundBank::load(directory),
                                std::make_unique<GodotSoundOutput>(*get_node<AudioStreamPlayer>("AttackAudio")));
            effect_sound_ = std::make_unique<por::SoundPlayer>(
                                por::SoundBank::load(directory),
                                std::make_unique<GodotSoundOutput>(*get_node<AudioStreamPlayer>("EffectAudio")));
            death_sound_ = std::make_unique<por::SoundPlayer>(
                               por::SoundBank::load(directory),
                               std::make_unique<GodotSoundOutput>(*get_node<AudioStreamPlayer>("DeathAudio")));
            attack_sound_->set_volume(0.125);
            effect_sound_->set_volume(0.125);
            death_sound_->set_volume(0.125);
        }
        get_node<Label>("Help")->set_text(i18n::text(N_(
                    "Teal: party | Orange: enemies\nWheel: scroll | Shift+wheel: sideways\nMiddle-drag: pan | Scrollbars: navigate")));
        if (campaign_)
            for (const char *name :
                    {"Training", "Slums", "Replay", "Save", "Load", "Revisit"
                    })
                get_node<Control>(name)->hide();
        get_node<Label>("Footer")->set_text(i18n::text(N_(
                    "Arrows/Numpad: move | Shift+arrow: diagonal | A: action | Space: use | Z: slot | Enter: end")));
        for (const char *name :
                {"Turn", "Roster", "Prompt", "Help"
                })
            get_node<Control>(name)->hide();
        for (const char *name :
                {"Training", "Slums", "Replay", "Move", "Melee", "Ranged", "MagicMissile",
                 "CureWounds", "HealingWord", "ScorchingRay", "Blindness", "SpellSlot", "SecondWind",
                 "Dodge", "Disengage", "Continue", "Save", "Load", "Revisit"
                })
            get_node<Control>(name)->hide();
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::layout()
{
    followed_.reset();
    const double width = get_size().x, height = get_size().y, sidebar = 358,
                 left_width = width - sidebar - 72;
    const auto board = demo_ && demo_->has_combat() ? demo_->combat().snapshot().battlefield
                       : Battlefield{12, 9, {}};
    const double weapon_height = get_node<OptionButton>("Weapons")->is_visible() ? 44 : 0;
    const double battlefield_height =
        std::min((height - 180) * .85, get_node<OptionButton>("ThrownWeapon")->is_visible()
                 ? height - 348
                 : get_node<Button>("StandUp")->is_visible()
                 ? height - 304
                 : (height - 180) * .85) -
        weapon_height;
    base_tile_ = std::max(left_width / board.width, battlefield_height / board.height);
    board_rect_ = Rect2(24, 16, left_width, battlefield_height);
    const double right = width - sidebar - 24;
    auto *scroll = get_node<ScrollContainer>("BattlefieldScroll");
    for (int i = 0; i < scroll->get_child_count(true); ++i)
    {
        if (auto *bar = Object::cast_to<ScrollBar>(scroll->get_child(i, true)))
            bar->set_focus_mode(FOCUS_ALL);
    }
    scroll->set_position(board_rect_.position);
    scroll->set_size(board_rect_.size);
    get_node<Control>("BattlefieldScroll/Canvas")
    ->set_custom_minimum_size(Vector2(base_tile_ * board.width, base_tile_ * board.height) *
                              combat_zoom_);
    get_node<Control>("BattlefieldScroll/Canvas")->queue_redraw();
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = get_node<Control>(name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("Training", Rect2(right, 20, 112, 34));
    place("Slums", Rect2(right + 120, 20, 112, 34));
    place("Replay", Rect2(right + 240, 20, 118, 34));
    place("Turn", Rect2(right, 70, sidebar, 70));
    place("Roster", Rect2(right, 148, sidebar, 160));
    place("Prompt", Rect2(right, 318, sidebar, 46));
    unsigned index = 0;
    for (const char *name :
            {"Move", "Melee", "Ranged", "MagicMissile", "CureWounds", "HealingWord", "ScorchingRay",
             "Blindness", "SpellSlot", "SecondWind", "Dash", "Dodge", "Disengage", "End"
            })
    {
        const unsigned row = index / 3, column = index % 3;
        place(name, Rect2(right + column * 122, 370 + row * 39, 114, 36));
        ++index;
    }
    place("Continue", Rect2(right + 244, 526, 114, 36));
    place("React", Rect2(right, 570, 174, 36));
    place("Decline", Rect2(right + 184, 570, 174, 36));
    place("Save", Rect2(right, 614, 112, 34));
    place("Load", Rect2(right + 122, 614, 112, 34));
    place("Revisit", Rect2(right + 244, 614, 114, 34));
    place("ZoomLevel", Rect2(right, 16, 66, 34));
    for (unsigned i = 0; i < 4; ++i)
        place(std::array<const char *, 4> {"ZoomOut100", "ZoomOut10", "ZoomIn10", "ZoomIn100"} [i],
              Rect2(right + 70 + i * 72, 16, 68, 34));
    const int zoom_percent = static_cast<int>(std::lround(combat_zoom_ * 100));
    get_node<Label>("ZoomLevel")->set_text(String::num_int64(zoom_percent) + "%");
    get_node<Button>("ZoomOut100")->set_disabled(zoom_percent <= 10);
    get_node<Button>("ZoomOut10")->set_disabled(zoom_percent <= 10);
    get_node<Button>("ZoomIn10")->set_disabled(zoom_percent >= 1000);
    get_node<Button>("ZoomIn100")->set_disabled(zoom_percent >= 1000);
    place("Help", Rect2(right, 700, sidebar, height - 746));
    place("Log", Rect2(24, board_rect_.get_end().y + 16, left_width,
                       height - board_rect_.get_end().y - 64));
    bool party_controls = false;
    if (demo_ && demo_->has_combat())
    {
        const auto state = demo_->combat().snapshot();
        party_controls = state.outcome == Outcome::ongoing &&
                         std::any_of(state.combatants.begin(), state.combatants.end(),
                                     [&](const auto & actor)
        {
            return actor.id == state.actor && actor.side == 0;
        });
    }
    layout_reaction_controls(party_controls);
    place("Footer", Rect2(24, height - 34, width - 48, 24));
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *label = get_node<RichTextLabel>(gs("PartyHP" + std::to_string(slot)));
        label->set_position(Vector2(width - 300, 60 + slot * (height - 120) / 8.0 + 52));
        label->set_size(Vector2(270, 28));
    }
    layout_status();
}

void CombatView::layout_reaction_controls(bool show_controls)
{
    const double top = board_rect_.get_end().y + 16;
    const double weapon_height = get_node<OptionButton>("Weapons")->is_visible() ? 44 : 0;
    get_node<Label>("WeaponLabel")->set_position(Vector2(24, top + 88));
    get_node<Label>("WeaponLabel")->set_size(Vector2(180, 36));
    get_node<OptionButton>("Weapons")->set_position(Vector2(214, top + 88));
    get_node<OptionButton>("Weapons")->set_size(Vector2(450, 36));
    const bool rush = get_node<Button>("AdrenalineRush")->is_visible();
    const bool spells = get_node<OptionButton>("Cantrip")->is_visible();
    const bool surge = get_node<Button>("ActionSurge")->is_visible();
    const bool cunning = get_node<OptionButton>("CunningAction")->is_visible();
    const bool aid = get_node<Button>("Stabilize")->is_visible();
    const bool standing = get_node<Button>("StandUp")->is_visible();
    get_node<Button>("Stabilize")->set_position(Vector2(704, top + weapon_height + 88));
    get_node<Button>("Stabilize")->set_size(Vector2(110, 36));
    get_node<Button>("StandUp")->set_position(Vector2(24, top + weapon_height + 132));
    get_node<Button>("StandUp")->set_size(Vector2(180, 36));
    get_node<Label>("ThrownWeaponLabel")->set_position(Vector2(24, top + weapon_height + 176));
    get_node<Label>("ThrownWeaponLabel")->set_size(Vector2(200, 36));
    get_node<OptionButton>("ThrownWeapon")->set_position(Vector2(234, top + weapon_height + 176));
    get_node<OptionButton>("ThrownWeapon")->set_size(Vector2(360, 36));
    get_node<Button>("Throw")->set_position(Vector2(604, top + weapon_height + 176));
    get_node<Button>("Throw")->set_size(Vector2(110, 36));
    const double inset =
        weapon_height + (get_node<OptionButton>("ThrownWeapon")->is_visible() ? 220
                         : standing                                           ? 176
                         : (cunning || aid)
                         ? 132
                         : (show_controls ? 44 : 0) + ((rush || spells || surge) ? 44 : 0));
    get_node<Label>("CunningActionLabel")->set_position(Vector2(24, top + weapon_height + 88));
    get_node<Label>("CunningActionLabel")->set_size(Vector2(aid ? 150 : 180, 36));
    get_node<OptionButton>("CunningAction")
    ->set_position(Vector2(aid ? 184 : 214, top + weapon_height + 88));
    get_node<OptionButton>("CunningAction")->set_size(Vector2(aid ? 160 : 200, 36));
    get_node<Button>("UseCunningAction")
    ->set_position(Vector2(aid ? 354 : 424, top + weapon_height + 88));
    get_node<Button>("UseCunningAction")->set_size(Vector2(aid ? 180 : 330, 36));
    get_node<Button>("Dash")->set_position(Vector2(24, top + 44));
    get_node<Button>("Dash")->set_size(Vector2(90, 36));
    get_node<Button>("AdrenalineRush")->set_position(Vector2(124, top + 44));
    get_node<Button>("AdrenalineRush")->set_size(Vector2(260, 36));
    get_node<Button>("ActionSurge")->set_position(Vector2(394, top + 44));
    get_node<Button>("ActionSurge")->set_size(Vector2(260, 36));
    get_node<Label>("CantripLabel")->set_position(Vector2(394, top + 44));
    get_node<Label>("CantripLabel")->set_size(Vector2(64, 36));
    get_node<OptionButton>("Cantrip")->set_position(Vector2(464, top + 44));
    get_node<OptionButton>("Cantrip")->set_size(Vector2(200, 36));
    get_node<Button>("CastCantrip")
    ->set_position(Vector2(474 + get_node<OptionButton>("Cantrip")->get_size().x, top + 44));
    get_node<Button>("CastCantrip")->set_size(Vector2(80, 36));
    auto *log = get_node<RichTextLabel>("Log");
    log->set_position(Vector2(24, top + inset));
    log->set_size(Vector2(board_rect_.size.x,
                          std::max(0.0, get_size().y - board_rect_.get_end().y - 64 - inset)));
    const double button_width = 174;
    get_node<Button>("React")->set_position(Vector2(24, top));
    get_node<Button>("React")->set_size(Vector2(button_width, 36));
    get_node<Button>("Decline")->set_position(Vector2(24 + button_width + 10, top));
    get_node<Button>("Nick")->set_position(Vector2(208, top));
    get_node<Button>("Nick")->set_size(Vector2(174, 36));
    get_node<Button>("Decline")->set_size(Vector2(button_width, 36));
    get_node<Button>("End")->set_position(Vector2(24, top));
    get_node<Button>("End")->set_size(Vector2(button_width, 36));
}

void CombatView::layout_status()
{
    // Let the translated status summary determine its height. The roster keeps
    // the remaining space above the action prompt and scrolls when necessary.
    auto *turn = get_node<Label>("Turn");
    turn->set_size(Vector2(358, 0));
    auto *roster = get_node<RichTextLabel>("Roster");
    const double top = std::max(148.0, double(turn->get_position().y + turn->get_size().y + 8));
    roster->set_position(Vector2(turn->get_position().x, top));
    roster->set_size(Vector2(358, std::max(0.0, 308 - top)));
}

#include "nick_dialog_impl.h"

void CombatView::training()
{
    try
    {
        error_.clear();
        demo_->training(settings::flag("--conditions") ? 3 : 42, settings::flag("--conditions"));
        sync_art();
        mode_ = "move";
        refresh();
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::slums()
{
    try
    {
        error_.clear();
        const auto directory = settings::game_path();
        demo_->slums(std::filesystem::u8path(directory.utf8().get_data()));
        mode_ = "move";
        sync_art();
        refresh();
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::replay()
{
    if (demo_ && demo_->is_slums())
        slums();
    else if (demo_)
        training();
}

void CombatView::next()
{
    try
    {
        if (demo_)
        {
            demo_->continue_script();
            sync_art();
            refresh();
        }
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::revisit()
{
    try
    {
        demo_->revisit();
        refresh();
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::sync_art(bool preserve_effects)
{
    auto prior_dead = std::move(known_dead_);
    auto prior_skulls = std::move(skull_seconds_);
    auto prior_actions = std::move(action_seconds_);
    missing_art_.clear();
    art_.clear();
    portraits_.clear();
    terrain_art_.clear();
    skull_art_.unref();
    known_dead_.clear();
    skull_seconds_.clear();
    action_seconds_.clear();
    if (!demo_)
        return;
    const auto directory = std::filesystem::u8path(settings::game_path().utf8().get_data());
    if (std::filesystem::is_directory(directory))
    {
        for (const auto &file : std::filesystem::directory_iterator(directory))
        {
            auto name = file.path().filename().string();
            for (auto &c : name)
                if (c >= 'a' && c <= 'z')
                    c -= 32;
            if (name != "COMSPR.DAX")
                continue;
            if (std::filesystem::file_size(file.path()) > 32 * 1024 * 1024)
                throw std::runtime_error("Combat effect archive exceeds limit");
            std::ifstream input(file.path(), std::ios::binary);
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
            if (input.bad())
                throw std::runtime_error("Cannot read combat effect art");
            auto decoded = decode_ega_combat_icon(bytes, 11, 0);
            if (!decoded)
                throw std::runtime_error("Cannot decode original skull combat effect");
            skull_art_ = ImageTexture::create_from_image(presentation::rgba_image(decoded.image));
            break;
        }
        if (skull_art_.is_null())
            throw std::runtime_error("Original skull combat effect is missing");
    }
    for (const auto &source : demo_->terrain_art())
    {
        terrain_art_.push_back(presentation::image_texture(source));
    }
    const auto install = [&](const CombatArt & source, bool goliath)
    {
        missing_art_.erase(source.entity);
        if (!source.missing_combination.empty())
            missing_art_[source.entity] = source.missing_combination;
        const auto image = presentation::rgba_image(source.image);
        const auto visible = image->get_used_rect();
        const auto mirrored =
            godot::Image::create_from_data(image->get_width(), image->get_height(), false,
                                           godot::Image::FORMAT_RGBA8, image->get_data());
        mirrored->flip_x();
        const auto lying =
            godot::Image::create_from_data(image->get_width(), image->get_height(), false,
                                           godot::Image::FORMAT_RGBA8, image->get_data());
        lying->rotate_90(COUNTERCLOCKWISE);
        Ref<ImageTexture> action, left_action;
        if (source.action)
        {
            const auto action_image = presentation::rgba_image(*source.action);
            action = ImageTexture::create_from_image(action_image);
            const auto mirrored_action = godot::Image::create_from_data(
                                             action_image->get_width(), action_image->get_height(), false,
                                             godot::Image::FORMAT_RGBA8, action_image->get_data());
            mirrored_action->flip_x();
            left_action = ImageTexture::create_from_image(mirrored_action);
        }
        art_[source.entity] = {ImageTexture::create_from_image(image),
                               action,
                               ImageTexture::create_from_image(mirrored),
                               left_action,
                               ImageTexture::create_from_image(lying),
                               visible,
                               Rect2(image->get_width() - visible.get_end().x, visible.position.y,
                                     visible.size.x, visible.size.y),
                               lying->get_used_rect(),
                               goliath
                              };
    };
    for (const auto &source : demo_->art())
        install(source, false);
    if (campaign_)
    {
        const auto originals = por::CharacterArt::load(directory);
        const auto catalog = por::CombatBodyCatalog::load(
                                 std::filesystem::u8path(game_combat_body_file().utf8().get_data()),
                                 std::filesystem::u8path(game_combat_weapon_file().utf8().get_data()));
        campaign_art_.clear();
        for (const auto id : campaign_->state().slots)
            if (id)
            {
                const auto resolved =
                    por::resolve_combat_appearance(campaign_->member(id), catalog);
                campaign_art_.push_back(
                {
                    id, resolved.icon(originals, false), resolved.icon(originals, true),
resolved.selection.matched ? std::string{} : resolved.selection.label});
            }
    }
    for (const auto &source : campaign_art_)
    {
        bool goliath = false;
        if (campaign_)
            for (const auto &member : campaign_->state().roster)
                if (member.id == source.entity)
                {
                    goliath = member.character.creation_data().race == "goliath";
                    break;
                }
        install(source, goliath);
    }
    if (campaign_)
    {
        auto legacy = por::CharacterArt::load(
                                std::filesystem::u8path(settings::game_path().utf8().get_data()));
        Ref<JSON> catalog_json;
        catalog_json.instantiate();
        Dictionary catalog;
        if (catalog_json->parse(
                    FileAccess::get_file_as_string("res://bin/portraits/portraits.json")) == OK &&
                catalog_json->get_data().get_type() == Variant::DICTIONARY)
            catalog = catalog_json->get_data();
        for (const auto id : campaign_->state().slots)
            if (id)
            {
                const auto &member = campaign_->member(id);
                std::string filename = member.character.appearance().portrait;
                if (filename.empty())
                {
                    int best = -1;
                    for (const auto &key : catalog.keys())
                    {
                        const Dictionary entry = catalog[key];
                        const auto &draft = member.character.creation_data();
                        const int score = 4 * (String(entry.get("Race", "")).to_lower() ==
                                               gs(draft.race).to_lower()) +
                                          2 * (String(entry.get("Gender", "")).to_lower() ==
                                               gs(draft.gender).to_lower()) +
                                          (String(entry.get("Class", "")).to_lower() ==
                                           gs(draft.character_class).to_lower());
                        if (score > best)
                        {
                            best = score;
                            filename = String(key).utf8().get_data();
                        }
                    }
                }
                if (!filename.empty())
                {
                    const Ref<Texture2D> portrait = ResourceLoader::get_singleton()->load(
                                                        gs("res://bin/portraits/" + filename));
                    if (portrait.is_valid())
                    {
                        portraits_.emplace(id, portrait);
                        continue;
                    }
                }
                const auto &appearance = member.character.appearance();
                if (appearance.portrait_head > 255 &&
                        !legacy.heads.contains(appearance.portrait_head))
                    presentation::load_additional_portrait_heads(legacy);
                const auto image = presentation::rgba_image(legacy.portrait(appearance));
                portraits_.emplace(id, ImageTexture::create_from_image(image));
            }
    }
    if (preserve_effects)
    {
        known_dead_ = std::move(prior_dead);
        skull_seconds_ = std::move(prior_skulls);
        action_seconds_ = std::move(prior_actions);
    }
}

void CombatView::save_game()
{
    try
    {
        write_save_file(local_path("user://checks/combat.save"), demo_->save_combat(),
                        4 * 1024 * 1024);
        error_.clear();
        get_node<Label>("Prompt")->set_text(i18n::text(N_("Training combat saved.")));
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::load_game()
{
    try
    {
        const auto bytes = read_save_file(local_path("user://checks/combat.save"), 4 * 1024 * 1024);
        demo_->restore_combat(bytes);
        sync_art();
        error_.clear();
        refresh();
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::select_mode(String verb)
{
    error_.clear();
    mode_ = spell_verb(verb.utf8().get_data(), spell_slot_);
    if (mode_ == "dash" || mode_ == "dodge" || mode_ == "disengage")
    {
        immediate(verb);
        return;
    }
    refresh();
}

void CombatView::cantrip_selected(std::int64_t index)
{
    auto *choices = get_node<OptionButton>("Cantrip");
    if (index < 0 || index >= choices->get_item_count())
        return;
    cantrip_ = String(choices->get_item_metadata(index)).utf8().get_data();
    mode_ = "move";
    refresh();
}

bool CombatView::matches_item(const Command &command) const
{
    return (command.verb != "throw" || command.item == thrown_item_) &&
           ((!command.verb.starts_with("light_") && !command.verb.starts_with("nick_")) ||
            command.item == light_item_);
}

void CombatView::weapon_selected(std::int64_t index)
{
    auto *choices = get_node<OptionButton>("Weapons");
    if (!demo_ || !demo_->has_combat() || index < 0 || index >= choices->get_item_count())
        return;
    for (const auto &c : demo_->combat().legal_commands())
        if (c.verb == "weapon_select" && c.item == unsigned(choices->get_item_id(index)))
        {
            act(c);
            return;
        }
}

void CombatView::use_cunning_action()
{
    auto *choices = get_node<OptionButton>("CunningAction");
    if (choices->get_selected() < 0 || get_node<Button>("UseCunningAction")->is_disabled())
        return;
    const String key = choices->get_item_metadata(choices->get_selected());
    const String verb = key.get_slice("#", 0);
    if (verb.begins_with("light_"))
    {
        light_item_ = key.get_slice("#", 1).to_int();
        get_node<Button>("UseCunningAction")->release_focus();
        select_mode(verb);
    }
    else if (verb == "lay_on_hands")
    {
        // A touched ally is chosen on the battlefield, like a spell target.
        get_node<Button>("UseCunningAction")->release_focus();
        select_mode(verb);
    }
    else
        immediate(verb);
}

void CombatView::cunning_selected(std::int64_t)
{
    refresh();
}

void CombatView::cast_cantrip()
{
    if (!cantrip_.empty())
        select_mode(gs(cantrip_));
}

void CombatView::thrown_selected(std::int64_t index)
{
    auto *choices = get_node<OptionButton>("ThrownWeapon");
    if (index < 0 || index >= choices->get_item_count())
        return;
    thrown_item_ = choices->get_item_id(index);
    mode_ = "move";
    refresh();
}

void CombatView::begin_throw()
{
    if (get_node<Button>("Throw")->is_disabled())
        return;
    get_node<Button>("Throw")->release_focus();
    select_mode("throw");
}

void CombatView::spell_slot()
{
    spell_slot_ = spell_slot_ == 1 ? 2 : 1;
    mode_ = "move";
    refresh();
}

void CombatView::adjust_zoom(int percentage_points)
{
    const int current = static_cast<int>(std::lround(combat_zoom_ * 100));
    const int next = std::clamp(current + percentage_points, 10, 1000);
    if (next == current)
        return;
    combat_zoom_ = next / 100.0;
    layout();
    refresh();
    // ScrollContainer applies its new child bounds during the layout pass.
    zoom_center_frames_ = 2;
}

void CombatView::select_party(EntityId id)
{
    if (!demo_ || !demo_->has_combat())
        return;
    const auto state = demo_->combat().snapshot();
    if (state.free_movement)
        return;
    const auto selected = std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == id && a.side == 0;
    });
    if (selected == state.combatants.end())
        return;
    selected_ = id;
    error_.clear();
    followed_.reset();
    center_on(selected->cell);
    refresh();
}

void CombatView::move_selected(Cell direction)
{
    if (!demo_ || !demo_->has_combat())
        return;
    const auto state = demo_->combat().snapshot();
    const auto explain = [&](const char *message)
    {
        error_ = message;
        refresh();
    };
    if (state.outcome != Outcome::ongoing)
        return;
    if (state.reaction_pending)
    {
        explain("Resolve the opportunity attack or decline the reaction before moving.");
        return;
    }
    if (state.actor != selected_)
    {
        explain("It is not the selected character's turn.");
        return;
    }
    const auto selected = std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == selected_ && a.side == 0;
    });
    if (selected == state.combatants.end())
        return;
    const Cell destination{selected->cell.x + direction.x, selected->cell.y + direction.y};
    const auto offered = demo_->combat().legal_commands();
    if (!state.battlefield.contains(destination))
    {
        explain("That square is outside the battlefield.");
        return;
    }
    const auto enemy =
        std::find_if(state.combatants.begin(), state.combatants.end(),
                     [&](const auto & a)
    {
        return a.side != selected->side && !a.dead && a.cell == destination;
    });
    if (enemy != state.combatants.end())
    {
        const auto attack = std::find_if(offered.begin(), offered.end(),
                                         [&](const auto & c)
        {
            return c.verb == "melee" && c.actor == selected_ &&
                   c.target == enemy->id;
        });
        if (attack != offered.end())
            act(*attack);
        else
            explain(selected->action ? "That enemy cannot be attacked from this square."
                    : "This character has already used their action.");
        return;
    }
    const auto move = std::find_if(offered.begin(), offered.end(),
                                   [&](const auto & c)
    {
        return c.verb == "move" && c.actor == selected_ &&
               c.destination == destination;
    });
    if (move != offered.end())
        act(*move);
    else if (std::any_of(state.combatants.begin(), state.combatants.end(),
                         [&](const auto & a)
{
    return !a.dead && a.cell == destination;
}))
    explain("That square is occupied.");
    else if (state.battlefield.at(destination) == 1)
        explain("That square is blocked by terrain.");
    else
        explain("That square is out of movement range. End the turn or use Dash if available.");
}

bool CombatView::aims_area(std::string_view verb) const
{
    if (!demo_ || !demo_->has_combat())
        return false;
    const auto offered = demo_->combat().legal_commands();
    return std::any_of(offered.begin(), offered.end(), [&](const auto & c)
    {
        return c.verb == verb && c.aims_area;
    });
}

void CombatView::aim_area_at(Cell cell)
{
    for (const auto &c : demo_->combat().legal_commands())
        if (c.verb == "area_move" && c.destination == cell)
        {
            act(c);
            return;
        }
}

void CombatView::immediate(String verb)
{
    if (!demo_ || !demo_->has_combat())
        return;
    auto wanted = std::string(verb.utf8().get_data());
    const auto state = demo_->combat().snapshot();
    if (state.effect_targeting && wanted == "end")
        wanted = "effect_skip";
    // While choosing a spell's creatures, End casts on those chosen.
    if (state.spell_targeting && wanted == "end")
        wanted = "spell_cast";
    if (state.area_targeting && wanted == "end")
        wanted = "area_cast";
    if (std::none_of(state.combatants.begin(), state.combatants.end(),
                     [&](const auto & a)
{
    return a.id == state.actor && a.side == 0;
}))
    return;
    for (const auto &c : demo_->combat().legal_commands())
        if (c.verb == wanted && presentation::initiative_command(*this, c) &&
                ((wanted != "effect_use" && wanted != "effect_skip") ||
                 c.item == presentation::optional_effect_item(*this, state)))
        {
            act(c);
            return;
        }
    if (state.reaction_pending && wanted == "end")
    {
        error_ = "Resolve the opportunity attack or decline the reaction before ending the turn.";
        refresh();
    }
}

void CombatView::act(const Command &command)
{
    try
    {
        const auto before = demo_->combat().snapshot();
        if (demo_->submit(command))
        {
            const auto after = demo_->combat().snapshot();
            unsigned sound = 0;
            if (command.verb == "melee" || command.verb == "opportunity")
            {
                const auto previous =
                    std::find_if(before.combatants.begin(), before.combatants.end(),
                                 [&](const auto & actor)
                {
                    return actor.id == command.target;
                });
                const auto current = std::find_if(after.combatants.begin(), after.combatants.end(),
                                                  [&](const auto & actor)
                {
                    return actor.id == command.target;
                });
                sound = (previous != before.combatants.end() &&
                         current != after.combatants.end() &&
                         current->hit_points < previous->hit_points)
                        ? 7
                        : 9;
            }
            else if ((command.verb == "ranged" || command.verb == "throw"))
                sound = 6;
            else if (command.verb == "chill_touch" || command.verb == "shocking_grasp" ||
                     command.verb == "eldritch_blast" || command.verb == "ray_of_frost" ||
                     command.verb == "fire_bolt" || command.verb == "poison_spray" ||
                     command.verb == "sacred_flame" || command.verb == "magic_missile" ||
                     command.verb == "magic_missile_2" || command.verb == "scorching_ray" ||
                     command.verb == "blindness" || command.verb == "inflict_wounds" ||
                     command.verb == "inflict_wounds_2")
                sound = 2;
            if (sound)
            {
                action_seconds_[command.actor] = 1.0;
                if (attack_sound_)
                    attack_sound_->play(sound);
            }
            bool moved = false, dead = false;
            for (const auto &actor : after.combatants)
            {
                const auto previous =
                    std::find_if(before.combatants.begin(), before.combatants.end(),
                                 [&](const auto & old)
                {
                    return old.id == actor.id;
                });
                if (previous == before.combatants.end())
                    continue;
                moved = moved || actor.cell != previous->cell;
                dead = dead || (actor.dead && !previous->dead);
            }
            if (dead && death_sound_)
                death_sound_->play(5);
            if (moved && effect_sound_)
                effect_sound_->play(10);
            mode_ = "move";
            effect_target_index_ = 0;
            ai_delay_ = 0;
            error_.clear();
            refresh();
        }
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::initiative_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == Key::KEY_ESCAPE)
    {
        get_node<Window>("InitiativeChoice")->set_input_as_handled();
        immediate("initiative_keep");
    }
}

void CombatView::optional_effect_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == Key::KEY_ESCAPE)
    {
        get_node<Window>("OptionalEffect")->set_input_as_handled();
        immediate("effect_skip");
    }
}

void CombatView::_input(const Ref<InputEvent> &event)
{
    if (get_node<Window>("InitiativeChoice")->is_visible())
        return;
    if (get_node<Window>("OptionalEffect")->is_visible())
        return;
    if (get_node<Window>("NickAttack")->is_visible())
        return;
    if (get_node<Window>("TemporaryHP")->is_visible() ||
            get_node<Window>("TacticalMind")->is_visible())
        return;
    if (!is_visible_in_tree() || !demo_ || Engine::get_singleton()->is_editor_hint())
        return;
    const Ref<InputEventKey> key = event;
    if (demo_->has_combat() && presentation::effect_target_input(
                event, demo_->combat().snapshot(),
                demo_->combat().legal_commands(), effect_target_index_,
                [&](const Command & c)
{
    act(c);
    },
    [&]
    {
        refresh();
    }))
    {
        get_viewport()->set_input_as_handled();
        return;
    }
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && demo_->has_combat() &&
            demo_->combat().snapshot().free_movement)
    {
        if (key->get_keycode() == Key::KEY_ESCAPE ||
                (key->get_keycode() == Key::KEY_SPACE && get_node<Button>("End")->has_focus()))
        {
            immediate("end");
            get_viewport()->set_input_as_handled();
            return;
        }
    }
    if (key.is_valid() && (get_node<OptionButton>("ThrownWeapon")->has_focus() ||
                           get_node<OptionButton>("ThrownWeapon")->get_popup()->is_visible()))
        return;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            (mode_ == "stabilize" || mode_ == "throw" ||
             (mode_.starts_with("light_") || mode_.starts_with("nick_"))) &&
            demo_->has_combat())
    {
        const auto state = demo_->combat().snapshot();
        const auto active = std::find_if(state.combatants.begin(), state.combatants.end(),
                                         [&](const auto & a)
        {
            return a.id == state.actor && a.side == 0;
        });
        std::vector<Command> targets;
        for (const auto &command : demo_->combat().legal_commands())
            if (command.verb == mode_ && matches_item(command))
                targets.push_back(command);
        if (active != state.combatants.end() && !targets.empty())
        {
            const auto found = std::find_if(targets.begin(), targets.end(),
                                            [&](const auto & c)
            {
                return c.target == aid_target_;
            });
            const auto index =
                found == targets.end() ? std::size_t{0} :
                std::size_t(found - targets.begin());
            const auto code = key->get_keycode();
            if (code == Key::KEY_LEFT || code == Key::KEY_RIGHT)
            {
                aid_target_ = targets[(index + (code == Key::KEY_RIGHT ? 1 : targets.size() - 1)) %
                                      targets.size()]
                              .target;
                refresh();
                get_viewport()->set_input_as_handled();
                return;
            }
            if (code == Key::KEY_SPACE)
            {
                act(targets[index]);
                get_viewport()->set_input_as_handled();
                return;
            }
        }
    }
    if (key.is_valid() &&
            (get_node<Button>("Throw")->has_focus() || get_node<Button>("Stabilize")->has_focus() ||
             get_node<Button>("StandUp")->has_focus() || get_node<Button>("Nick")->has_focus() ||
             get_node<Button>("UseCunningAction")->has_focus() ||
             get_node<Button>("ActionSurge")->has_focus() ||
             get_node<Button>("AdrenalineRush")->has_focus() || get_node<Button>("Dash")->has_focus() ||
             get_node<Button>("CastCantrip")->has_focus()) &&
            (key->get_keycode() == Key::KEY_ENTER || key->get_keycode() == Key::KEY_KP_ENTER ||
             key->get_keycode() == Key::KEY_SPACE))
        return;
    if (key.is_valid() && (get_node<OptionButton>("Weapons")->has_focus() ||
                           get_node<OptionButton>("Weapons")->get_popup()->is_visible()))
        return;
    if (key.is_valid() && (get_node<OptionButton>("CunningAction")->has_focus() ||
                           get_node<OptionButton>("CunningAction")->get_popup()->is_visible() ||
                           get_node<OptionButton>("Cantrip")->has_focus() ||
                           get_node<OptionButton>("Cantrip")->get_popup()->is_visible()))
        return;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && !key->is_ctrl_pressed() &&
            demo_->has_combat())
    {
        // Aiming an area spell (CLASS-5): arrows move the preview, Space or Enter
        // casts and Escape cancels. With an area spell selected, Space or Enter
        // starts aiming.
        if (demo_->combat().snapshot().area_targeting)
        {
            const auto code = key->get_keycode();
            if (code == Key::KEY_SPACE || code == Key::KEY_ENTER || code == Key::KEY_KP_ENTER)
                immediate("area_cast");
            else if (code == Key::KEY_ESCAPE)
                immediate("spell_cancel");
            else if (const auto step = movement_direction(code, key->is_shift_pressed()))
            {
                const auto center = demo_->combat().snapshot().area_targeting->center;
                aim_area_at({center.x + step->x, center.y + step->y});
            }
            get_viewport()->set_input_as_handled();
            return;
        }
        if (aims_area(mode_) && (key->get_keycode() == Key::KEY_SPACE ||
                                 key->get_keycode() == Key::KEY_ENTER ||
                                 key->get_keycode() == Key::KEY_KP_ENTER))
        {
            immediate(gs(mode_));
            get_viewport()->set_input_as_handled();
            return;
        }
        // While choosing a spell's creatures, Space casts and Escape cancels.
        if (demo_->combat().snapshot().spell_targeting &&
                (key->get_keycode() == Key::KEY_SPACE || key->get_keycode() == Key::KEY_ESCAPE))
        {
            immediate(key->get_keycode() == Key::KEY_SPACE ? "spell_cast" : "spell_cancel");
            get_viewport()->set_input_as_handled();
            return;
        }
        if (key->get_keycode() == Key::KEY_ESCAPE &&
                (mode_ == "stabilize" || mode_ == "throw" ||
                 (mode_.starts_with("light_") || mode_.starts_with("nick_"))))
        {
            mode_ = "move";
            refresh();
            get_viewport()->set_input_as_handled();
            return;
        }
        if (const auto direction = movement_direction(key->get_keycode(), key->is_shift_pressed()))
        {
            move_selected(*direction);
            get_viewport()->set_input_as_handled();
            return;
        }
        if (key->get_keycode() == Key::KEY_A)
        {
            std::vector<std::string> actions;
            for (const auto &command : demo_->combat().legal_commands())
                if (command.verb != "end" && command.verb != "weapon_select" &&
                        std::find(actions.begin(), actions.end(), command.verb) == actions.end())
                    actions.push_back(command.verb);
            if (!actions.empty())
            {
                const auto current = std::find(actions.begin(), actions.end(), mode_);
                mode_ =
                    actions[current == actions.end()
                            ? 0
                            : (std::size_t(current - actions.begin()) + 1) % actions.size()];
                if ((mode_.starts_with("light_") || mode_.starts_with("nick_")))
                    for (const auto &c : demo_->combat().legal_commands())
                        if (c.verb == mode_)
                        {
                            light_item_ = c.item;
                            break;
                        }
                refresh();
            }
            get_viewport()->set_input_as_handled();
            return;
        }
        if (key->get_keycode() == Key::KEY_Z)
        {
            spell_slot();
            get_viewport()->set_input_as_handled();
            return;
        }
        if (key->get_keycode() == Key::KEY_SPACE)
        {
            for (const char *verb :
                    {"stand_up", "cunning_dash", "cunning_disengage", "steady_aim",
                     "action_surge", "adrenaline_rush", "sacred_weapon", "turn_undead",
                     "preserve_life",
                     "second_wind", "dash",
                     "dodge", "disengage", "opportunity", "decline"
                    })
                if (mode_ == verb)
                {
                    immediate(gs(mode_));
                    break;
                }
            const auto offered = demo_->combat().legal_commands();
            const auto target =
                std::find_if(offered.begin(), offered.end(),
                             [&](const auto & c)
            {
                return c.verb == mode_ && matches_item(c) && c.target == selected_;
            });
            if (target != offered.end())
                act(*target);
            get_viewport()->set_input_as_handled();
            return;
        }
    }
    if (!defeated() && key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == Key::KEY_ENTER)
    {
        if (demo_->waiting())
            next();
        else
            immediate("end");
        get_viewport()->set_input_as_handled();
        return;
    }
    if (!demo_->has_combat())
        return;
    auto *scroll = get_node<ScrollContainer>("BattlefieldScroll");
    const Ref<InputEventMouseMotion> motion = event;
    if (motion.is_valid())
        update_hover(motion->get_position());
    if (motion.is_valid() && panning_)
    {
        if (!motion->get_button_mask().has_flag(MouseButtonMask::MOUSE_BUTTON_MASK_MIDDLE))
        {
            panning_ = false;
            return;
        }
        const auto inverse = get_global_transform_with_canvas().affine_inverse();
        const auto delta = inverse.basis_xform(motion->get_relative());
        scroll->set_h_scroll(scroll->get_h_scroll() - static_cast<int>(delta.x));
        scroll->set_v_scroll(scroll->get_v_scroll() - static_cast<int>(delta.y));
        get_viewport()->set_input_as_handled();
        return;
    }
    const Ref<InputEventMouseButton> mouse = event;
    if (mouse.is_null())
        return;
    if (mouse->get_button_index() == MouseButton::MOUSE_BUTTON_MIDDLE && !mouse->is_pressed())
    {
        panning_ = false;
        return;
    }
    const auto local =
        get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
    if (mouse->is_pressed() && mouse->get_button_index() == MouseButton::MOUSE_BUTTON_LEFT &&
            campaign_)
    {
        const double right = get_size().x - 382, row_height = (get_size().y - 120) / 8.0;
        if (local.x >= right && local.x < right + 358 && local.y >= 60 &&
                local.y < 60 + 8 * row_height)
        {
            const auto slot = static_cast<unsigned>((local.y - 60) / row_height);
            if (const auto id = campaign_->state().slots[slot])
                select_party(id);
            get_viewport()->set_input_as_handled();
            return;
        }
    }
    // Keep clicks on the scrollbars out of combat targeting.
    if (!board_rect_.has_point(local))
        return;
    for (int i = 0; i < scroll->get_child_count(true); ++i)
    {
        auto *bar = Object::cast_to<ScrollBar>(scroll->get_child(i, true));
        if (bar && bar->is_visible() &&
                Rect2(Vector2(), bar->get_size())
                .has_point(bar->get_global_transform_with_canvas().affine_inverse().xform(
                               mouse->get_position())))
            return;
    }
    if (mouse->get_button_index() == MouseButton::MOUSE_BUTTON_MIDDLE && mouse->is_pressed())
    {
        panning_ = true;
        scroll->grab_focus();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (defeated() || !mouse->is_pressed())
        return;
    const auto canvas = get_node<Control>("BattlefieldScroll/Canvas")
                        ->get_global_transform_with_canvas()
                        .affine_inverse()
                        .xform(mouse->get_position());
    const auto relative = canvas / (combat_zoom_ * base_tile_);
    const Cell cell{static_cast<int>(std::floor(relative.x)),
                    static_cast<int>(std::floor(relative.y))};
    // Area spells (CLASS-5): a left click aims (starting the aim when the spell
    // is selected), a right click casts at the preview.
    const bool aiming = demo_->combat().snapshot().area_targeting.has_value();
    if (aiming && mouse->get_button_index() == MouseButton::MOUSE_BUTTON_RIGHT)
    {
        immediate("area_cast");
        get_viewport()->set_input_as_handled();
        return;
    }
    if (mouse->get_button_index() != MouseButton::MOUSE_BUTTON_LEFT)
        return;
    if (aiming || aims_area(mode_))
    {
        if (!aiming)
            immediate(gs(mode_));
        aim_area_at(cell);
        get_viewport()->set_input_as_handled();
        return;
    }
    const auto s = demo_->combat().snapshot();
    // A click on an ally that the selected action can target casts on it
    // instead of selecting it; these modes always target.
    const auto offered_here = demo_->combat().legal_commands();
    const bool targets_clicked = std::any_of(offered_here.begin(), offered_here.end(),
                                 [&](const auto & c)
    {
        if (c.verb != mode_ || !c.target)
            return false;
        const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                         [&](const auto & a)
        {
            return a.id == c.target;
        });
        return target != s.combatants.end() && target->cell == cell;
    });
    if (!s.free_movement && !s.effect_targeting && !targets_clicked && mode_ != "stabilize" &&
            mode_ != "lay_on_hands" && mode_ != "chill_touch" &&
            mode_ != "poison_spray" && mode_ != "sacred_flame" &&
            mode_ != "shocking_grasp" && mode_ != "eldritch_blast" && mode_ != "ray_of_frost")
        for (const auto &a : s.combatants)
            if (a.side == 0 && !a.dead && a.cell == cell)
            {
                select_party(a.id);
                get_viewport()->set_input_as_handled();
                return;
            }
    const auto current = std::find_if(s.combatants.begin(), s.combatants.end(),
                                      [&](const auto & a)
    {
        return a.id == s.actor;
    });
    if (current == s.combatants.end() || current->side != 0 || selected_ != s.actor)
        return;
    for (const auto &c : demo_->combat().legal_commands())
        if (c.verb == mode_ && matches_item(c))
        {
            if ((c.verb == "move" || c.verb == "effect_push") && c.destination == cell)
            {
                act(c);
                get_viewport()->set_input_as_handled();
                return;
            }
            if (c.target && c.verb != "effect_push")
            {
                const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                                 [&](const auto & a)
                {
                    return a.id == c.target;
                });
                if (target != s.combatants.end() && target->cell == cell)
                {
                    act(c);
                    get_viewport()->set_input_as_handled();
                    return;
                }
            }
        }
    if (mode_ == "move" &&
            std::max(std::abs(cell.x - current->cell.x), std::abs(cell.y - current->cell.y)) == 1)
        move_selected({cell.x - current->cell.x, cell.y - current->cell.y});
    else
    {
        error_ = "That square is not a legal destination or target for the selected action.";
        refresh();
    }
    get_viewport()->set_input_as_handled();
}

void CombatView::update_hover(const Vector2 &pointer)
{
    auto *panel = get_node<PanelContainer>("HoverInfo");
    panel->hide();
    if (!demo_ || !demo_->has_combat() || panning_)
        return;
    const auto local = get_global_transform_with_canvas().affine_inverse().xform(pointer);
    if (!board_rect_.has_point(local))
        return;
    const auto canvas = get_node<Control>("BattlefieldScroll/Canvas")
                        ->get_global_transform_with_canvas()
                        .affine_inverse()
                        .xform(pointer);
    const Cell cell{static_cast<int>(std::floor(canvas.x / (combat_zoom_ * base_tile_))),
                    static_cast<int>(std::floor(canvas.y / (combat_zoom_ * base_tile_)))};
    const auto state = demo_->combat().snapshot();
    if (!state.battlefield.contains(cell))
        return;
    const auto npc = [&](EntityId id) -> std::optional<std::reference_wrapper<const PartyMember>>
    {
        if (!campaign_)
        return {};
    const auto &roster = campaign_->state().roster;
    const auto member =
        std::find_if(roster.begin(), roster.end(),
                         [&](const auto & candidate)
        {
            return candidate.id == id && !candidate.npc_source.empty();
        });
        return member == roster.end() ? std::nullopt : std::optional{std::cref(*member)};
    };
    const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto & actor)
    {
        return !actor.dead && actor.cell == cell &&
               (actor.side == 1 || npc(actor.id).has_value());
    });
    if (found == state.combatants.end())
        return;
    const auto npc_member = npc(found->id);
    const auto closest = std::min_element(state.combatants.begin(), state.combatants.end(),
                                          [&](const auto & a, const auto & b)
    {
        const auto distance = [&](const CombatantView & target)
        {
            if (target.side != 0 || !target.conscious)
                return std::numeric_limits<int>::max();
            return std::max(std::abs(target.cell.x - cell.x),
                            std::abs(target.cell.y - cell.y));
        };
        return distance(a) < distance(b);
    });
    const bool adjacent =
        closest != state.combatants.end() && closest->side == 0 && closest->conscious &&
        std::max(std::abs(closest->cell.x - cell.x), std::abs(closest->cell.y - cell.y)) <= 1;
    const auto &weapon =
        adjacent || !found->ranged_attack_available ? found->melee_weapon : found->ranged_weapon;
    String type = found->type_name.empty() ? gs(found->name) : i18n::text(found->type_name);
    String weapon_name = weapon.empty() ? i18n::text("Unspecified") : i18n::text(weapon);
    if (npc_member)
    {
        type = gs(found->name) + " (" + i18n::text("NPC") + ")";
        weapon_name = i18n::text("Unarmed");
        for (const auto id : npc_member->get().equipped)
            if (const auto item = npc_member->get().character.inventory().find(id))
            {
                const auto &key = item->get().definition_id;
                if (key == "longsword" || key == "shortsword" || key == "short_sword" ||
                        key == "dagger" || key == "mace" || key == "quarterstaff" ||
                        key == "scimitar" || key == "shortbow" || key == "longbow")
                {
                    weapon_name = gs(item->get().name);
                    break;
                }
            }
    }
    get_node<Label>("HoverInfo/Details")
    ->set_text(i18n::format("{type}\nAC {ac}  HP {hp}/{maximum}\nWeapon: {weapon}",
    {
        {"type", type},
        {"ac", found->armor_class},
        {"hp", found->hit_points},
        {"maximum", found->max_hit_points},
        {"weapon", weapon_name}
    }));
    const auto size = panel->get_size();
    panel->set_position(Vector2(
                            std::clamp(local.x + 18.0, 0.0, std::max(0.0, static_cast<double>(get_size().x - size.x))),
                            std::clamp(local.y + 18.0, 0.0,
                                       std::max(0.0, static_cast<double>(get_size().y - size.y)))));
    panel->show();
}

void CombatView::refresh()
{
    if (!ready_)
        return;
    const bool loaded = demo_ && demo_->has_combat();
    Snapshot s;
    if (loaded)
        s = demo_->combat().snapshot();
    if (loaded)
        for (const auto &actor : s.combatants)
        {
            const auto [entry, first_seen] = known_dead_.emplace(actor.id, actor.dead);
            if (!first_seen)
            {
                if (actor.dead && !entry->second)
                    skull_seconds_[actor.id] = 1.0;
                entry->second = actor.dead;
            }
        }
    bool player = false;
    String turn = i18n::text(demo_ ? demo_->status() : N_("Unable to load rules"));
    if (loaded && s.outcome == Outcome::ongoing)
        for (const auto &a : s.combatants)
            if (a.id == s.actor)
            {
                player = a.side == 0;
                turn = i18n::format(
                           s.reaction_pending
                           ? N_("Round {round} / {name} reaction\nMove {feet} ft | {action}")
                           : N_("Round {round} / {name} turn\nMove {feet} ft | {action}"),
                {
                    {"round", s.round},
                    {"name", gs(a.name)},
                    {"feet", a.movement_feet},
                    {"action", i18n::text(a.action ? N_("Action ready") : N_("Action spent"))}
                });
                turn += "\n" + (a.status_messages.empty()
                                ? i18n::text(a.status)
                                : i18n::render(a.status_messages).replace("\n", " | "));
            }
    if (loaded && s.outcome != Outcome::ongoing)
        turn = i18n::text(s.outcome == Outcome::victory ? N_("Victory")
                          : N_("Party incapacitated / defeat"));
    if (player && last_actor_ && last_actor_ != s.actor)
        selected_ = s.actor;
    if ((!selected_ || s.free_movement || s.effect_targeting) && player)
        selected_ = s.actor;
    if (loaded)
        last_actor_ = s.actor;
    get_node<Label>("Turn")->set_text(turn);
    layout_status();
    String roster;
    for (const auto &a : s.combatants)
    {
        roster += String(a.id == s.actor ? "> " : "  ") + String::num_int64(a.id) + " " +
                  presentation::bbcode_literal(gs(a.name)) + "  " +
                  presentation::hp_text(a.hit_points, a.max_hit_points, a.dead, a.temporary_hp,
                                        a.hp_messages) +
        "  " + i18n::format("AC {ac}", {{"ac", a.armor_class}}) + "\n";
        if (!a.conditions.empty())
            roster += "    " + presentation::bbcode_literal(i18n::render(a.conditions)) + "\n";
    }
    get_node<RichTextLabel>("Roster")->set_text(roster);
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *label = get_node<RichTextLabel>(gs("PartyHP" + std::to_string(slot)));
        const auto id = campaign_ ? campaign_->state().slots[slot] : 0;
        label->set_visible(bool(id));
        if (!id)
            continue;
        const auto found = std::find_if(s.combatants.begin(), s.combatants.end(),
                                        [&](const auto & a)
        {
            return a.id == id;
        });
        if (found == s.combatants.end())
        {
            label->hide();
            continue;
        }
        const auto &a = *found;
        label->set_position(
            Vector2(get_size().x - 300, 60 + slot * (get_size().y - 120) / 8.0 + 52));
        label->set_size(Vector2(270, 28));
        label->set_text(presentation::hp_text(a.hit_points, a.max_hit_points, a.dead,
                                              a.temporary_hp, a.hp_messages) +
        "  " + i18n::format("AC {ac}", {{"ac", a.armor_class}}));
    }
    const auto active = std::find_if(s.combatants.begin(), s.combatants.end(),
                                     [&](const auto & a)
    {
        return a.id == s.actor;
    });
    auto *cantrips = get_node<OptionButton>("Cantrip");
    std::vector<std::string> known;
    if (player && s.outcome == Outcome::ongoing && active != s.combatants.end())
        known = active->known_cantrips;
    if (std::find(known.begin(), known.end(), mode_) != known.end())
        cantrip_ = mode_;
    if (std::find(known.begin(), known.end(), cantrip_) == known.end())
        cantrip_ = known.empty() ? "" : known.front();
    // Preserve the live popup and keyboard selection during unrelated refreshes.
    bool changed = cantrips->get_item_count() != int(known.size());
    for (int i = 0; !changed && i < cantrips->get_item_count(); ++i)
        changed = String(cantrips->get_item_metadata(i)) != gs(known[i]);
    if (changed)
    {
        cantrips->clear();
        for (const auto &id : known)
        {
            const char *label = id == "fire_bolt"        ? N_("Fire Bolt")
                                : id == "poison_spray"   ? N_("Poison Spray")
                                : id == "sacred_flame"   ? N_("Sacred Flame")
                                : id == "chill_touch"    ? N_("Chill Touch")
                                : id == "shocking_grasp" ? N_("Shocking Grasp")
                                : id == "eldritch_blast" ? N_("Eldritch Blast")
                                : id == "ray_of_frost"   ? N_("Ray of Frost")
                                : nullptr;
            cantrips->add_item(label ? i18n::text(label) : gs(id));
            cantrips->set_item_metadata(cantrips->get_item_count() - 1, gs(id));
        }
    }
    if (!known.empty())
        cantrips->select(int(std::find(known.begin(), known.end(), cantrip_) - known.begin()));
    for (const char *name :
            {"CantripLabel", "Cantrip", "CastCantrip"
            })
        get_node<Control>(name)->set_visible(!known.empty());
    unsigned rushes = 0, capacity = 0;
    if (active != s.combatants.end())
        for (const auto &pool : active->resources)
            if (pool.id == "adrenaline_rush")
            {
                rushes = pool.remaining;
                capacity = pool.capacity;
            }
    const bool show_rush = player && capacity && s.outcome == Outcome::ongoing;
    get_node<Button>("AdrenalineRush")->set_visible(show_rush);
    get_node<Button>("Dash")->set_visible(show_rush);
    get_node<Button>("AdrenalineRush")
    ->set_text(i18n::format("Adrenaline Rush ({remaining}/{maximum})",
    {{"remaining", rushes}, {"maximum", capacity}}));
    unsigned surges = 0, surge_capacity = 0;
    if (active != s.combatants.end())
        for (const auto &pool : active->resources)
            if (pool.id == "action_surge")
            {
                surges = pool.remaining;
                surge_capacity = pool.capacity;
            }
    get_node<Button>("ActionSurge")
    ->set_visible(player && surge_capacity && s.outcome == Outcome::ongoing);
    get_node<Button>("ActionSurge")
    ->set_text(i18n::format("Action Surge ({remaining}/{maximum})",
    {{"remaining", surges}, {"maximum", surge_capacity}}));
    auto *modal = get_node<Window>("TemporaryHP");
    if (player && s.temporary_hp_offer)
    {
        const auto &offer = *s.temporary_hp_offer;
        get_node<Label>("TemporaryHP/Text")
        ->set_text(i18n::format(
                       "Choose Temporary HP\n\nCurrent: {current} — {current_source}\nNew: {offered} — {offered_source}\n\nThe amounts do not add. The Bonus Action and use are already spent.",
        {
            {"current", offer.current.amount},
            {"current_source", presentation::temporary_hp_source(offer.current)},
            {"offered", offer.offered.amount},
            {"offered_source", presentation::temporary_hp_source(offer.offered)}
        }));
        if (!modal->is_visible())
        {
            modal->popup_centered();
            get_node<Button>("TemporaryHP/Keep")->grab_focus();
        }
    }
    else if (modal->is_visible())
    {
        modal->hide();
        if (show_rush)
            get_node<Button>("AdrenalineRush")->grab_focus();
    }
    auto *mind = get_node<Window>("TacticalMind");
    if (player && s.ability_check_choice)
    {
        const auto &check = *s.ability_check_choice;
        const bool changed = !mind->is_visible();
        get_node<Label>("TacticalMind/Text")
        ->set_text(i18n::format(
                       "Failed Medicine check: d20 {roll} + {modifier} = {total} vs DC {dc}.\nSecond Wind uses: {uses}\n\nAdd 1d10. Spend one use only if the check succeeds.\nThe original Action is already spent.",
        {
            {"roll", check.natural},
            {"modifier", check.modifier},
            {"total", check.total},
            {"dc", check.difficulty},
            {"uses", check.resource_uses}
        }));
        if (changed)
        {
            mind->popup_centered();
            get_node<Button>("TacticalMind/Use")->grab_focus();
        }
    }
    else if (mind->is_visible())
    {
        mind->hide();
        get_node<Button>("End")->grab_focus();
    }
    get_node<Button>("Continue")
    ->set_visible(demo_ && (demo_->waiting() || (loaded && s.outcome != Outcome::ongoing)));
    get_node<Button>("End")->set_visible(!get_node<Button>("Continue")->is_visible());
    get_node<Button>("End")->set_text(i18n::text(s.effect_targeting ? N_("Skip effect")
            : s.spell_targeting || s.area_targeting ? N_("Cast spell")
            : s.free_movement  ? "Finish free move"
            : "End turn"));
    if (s.free_movement)
        mode_ = "move";
    if (s.effect_targeting)
        mode_ = s.effect_targeting->verb;
    if (s.spell_targeting)
        mode_ = s.spell_targeting->verb;
    if (s.area_targeting)
        mode_ = s.area_targeting->verb;
    const auto offered = loaded ? demo_->combat().legal_commands() : std::vector<Command> {};
    const auto enabled = [&](std::string_view verb)
    {
        return player && std::any_of(offered.begin(), offered.end(),
                                     [&](const auto & c)
        {
            return c.verb == verb ||
                   (verb == "end" && s.effect_targeting &&
                    c.verb == "effect_skip") ||
                   (verb == "end" && s.spell_targeting && c.verb == "spell_cast") ||
                   (verb == "end" && s.area_targeting && c.verb == "area_cast");
        });
    };
    auto *thrown = get_node<OptionButton>("ThrownWeapon");
    thrown->set_block_signals(true);
    thrown->set_fit_to_longest_item(false);
    thrown->clear();
    const auto throwing = std::find_if(s.combatants.begin(), s.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == s.actor && a.side == 0;
    });
    if (throwing != s.combatants.end())
        for (const auto &option : throwing->thrown_weapons)
        {
            const auto label = i18n::render(option.label);
            thrown->add_item(label, option.item);
            thrown->set_item_disabled(thrown->get_item_count() - 1, !option.available);
        }
    int throw_index = -1;
    for (int i = 0; i < thrown->get_item_count(); ++i)
        if (thrown->get_item_id(i) == int(thrown_item_))
            throw_index = i;
    if (throw_index < 0 && thrown->get_item_count())
        throw_index = 0;
    if (throw_index >= 0)
    {
        thrown->select(throw_index);
        thrown_item_ = thrown->get_item_id(throw_index);
    }
    else
        thrown_item_ = 0;
    thrown->set_block_signals(false);
    thrown->set_tooltip_text(throw_index >= 0 ? thrown->get_item_text(throw_index) : String());
    const bool show_thrown = s.outcome == Outcome::ongoing && thrown->get_item_count() > 0;
    const bool thrown_layout_changed = thrown->is_visible() != show_thrown;
    for (const char *name :
            {"ThrownWeaponLabel", "ThrownWeapon", "Throw"
            })
        get_node<Control>(name)->set_visible(show_thrown);
    const bool can_throw =
        player && std::any_of(offered.begin(), offered.end(),
                              [&](const auto & c)
    {
        return c.verb == "throw" && c.item == thrown_item_;
    });
    thrown->set_disabled(!enabled("throw"));
    get_node<Button>("Throw")->set_disabled(!can_throw);
    for (const auto &[node, verb] : action_buttons)
        get_node<Button>(node)->set_disabled(!enabled(spell_verb(verb, spell_slot_)));
    const auto cunning_actor = std::find_if(s.combatants.begin(), s.combatants.end(),
                                            [&](const auto & a)
    {
        return a.id == (player ? s.actor : selected_);
    });
    const bool ongoing = s.outcome == Outcome::ongoing;
    const bool show_stabilize =
        s.outcome == Outcome::ongoing && std::any_of(s.combatants.begin(), s.combatants.end(),
            [](const auto & a)
    {
        return !a.dead && a.hit_points == 0;
    });
    const bool aid_layout_changed = get_node<Button>("Stabilize")->is_visible() != show_stabilize;
    get_node<Button>("Stabilize")->set_visible(show_stabilize);
    get_node<Button>("Stabilize")->set_disabled(!enabled("stabilize"));
    const bool show_standing =
        ongoing && cunning_actor != s.combatants.end() && cunning_actor->prone;
    const bool posture_layout_changed = get_node<Button>("StandUp")->is_visible() != show_standing;
    get_node<Button>("StandUp")->set_visible(show_standing);
    get_node<Button>("StandUp")->set_disabled(!enabled("stand_up"));
    if (aid_layout_changed || posture_layout_changed || thrown_layout_changed)
        layout();
    const auto *choice_actor = cunning_actor != s.combatants.end() && s.outcome == Outcome::ongoing
                               ? &*cunning_actor
                               : nullptr;
    const bool weapon_layout = presentation::refresh_weapons(*this, choice_actor, player,
        [](const Message & message)
    {
        return i18n::render(message);
    });
    const bool bonus_layout =
        presentation::refresh_bonus_attacks(*this, choice_actor, offered, player, i18n::text,
                                            [](const Message & message)
    {
        return i18n::render(message);
    });
    presentation::refresh_nick(*this, choice_actor, player && !s.reaction_pending,
                               [](const Message & message)
    {
        return i18n::render(message);
    });
    presentation::refresh_initiative(*this, s, offered, i18n::text,
                                     [](const Message & m)
    {
        return i18n::render(m);
    });
    presentation::refresh_optional_effect(*this, s.optional_effect_choice, player,
                                          [](const Message & m)
    {
        return i18n::render(m);
    });
    if (s.reaction_pending)
        get_node<Button>("Nick")->hide();
    if (weapon_layout || bonus_layout)
        layout();
    get_node<Button>("CastCantrip")->set_disabled(cantrip_.empty() || !enabled(cantrip_));
    get_node<Button>("ActionSurge")->set_disabled(!enabled("action_surge"));
    get_node<Button>("AdrenalineRush")->set_disabled(!enabled("adrenaline_rush"));
    get_node<Button>("SpellSlot")
    ->set_text(i18n::format("Slot level {level}", {{"level", spell_slot_}}));
    get_node<Button>("SpellSlot")
    ->set_disabled(!enabled("magic_missile") && !enabled("magic_missile_2") &&
                   !enabled("cure_wounds") && !enabled("cure_wounds_2") &&
                   !enabled("healing_word") && !enabled("healing_word_2"));
    for (const auto &[node, verb] :
    std::array<std::pair<const char *, const char *>, 5> {{{"Move", "move"},
        {"End", "end"},
        {"SecondWind", "second_wind"},
        {"React", "opportunity"},
        {"Decline", "decline"}
    }
})
    get_node<Button>(node)->set_disabled(!enabled(verb));
    {
        const bool party_turn = loaded && s.outcome == Outcome::ongoing && player;
        const bool reaction =
            loaded && s.outcome == Outcome::ongoing && s.reaction_pending && player;
        layout_reaction_controls(party_turn);
        get_node<Button>("End")->set_visible(party_turn && !reaction);
        get_node<Button>("React")->set_visible(reaction);
        get_node<Button>("Decline")->set_visible(reaction);
    }
    get_node<Button>("Continue")->set_disabled(!demo_ || !demo_->waiting());
    get_node<Button>("Save")->set_disabled(!loaded || demo_->is_slums());
    get_node<Button>("Load")->set_disabled(!loaded || demo_->is_slums());
    get_node<Button>("Revisit")->set_disabled(!loaded || !demo_->script_complete() ||
            s.outcome != Outcome::victory);
    String action = i18n::text("Move");
    for (const auto &command : offered)
        if (command.verb == mode_ && matches_item(command))
        {
            action = i18n::text(command.label);
            break;
        }
    get_node<Label>("Prompt")->set_text(
        !error_.empty()             ? i18n::text(error_)
        : demo_ && demo_->waiting() ? i18n::text("Read the encounter text, then Continue.")
        : loaded && s.outcome != Outcome::ongoing ? i18n::text(demo_->status())
        : s.reaction_pending ? i18n::text("Use or decline the opportunity attack.")
        : player
    ? i18n::format("Selected: {action}. Click a highlighted square.", {{"action", action}})
    : i18n::text("Enemy turn"));
    if (error_.empty() && loaded && s.outcome == Outcome::ongoing && !s.reaction_pending &&
            selected_ && selected_ != s.actor)
    {
        const auto selected = std::find_if(s.combatants.begin(), s.combatants.end(),
                                           [&](const auto & a)
        {
            return a.id == selected_ && a.side == 0;
        });
        if (selected != s.combatants.end())
            get_node<Label>("Prompt")->set_text(
            i18n::format("It is not {name}'s turn.", {{"name", gs(selected->name)}}));
    }
    if (error_.empty() && player && !s.reaction_pending && selected_ == s.actor &&
            mode_ == "move" &&
            std::none_of(offered.begin(), offered.end(),
                         [](const auto & c)
{
    return c.verb == "move";
}))
    get_node<Label>("Prompt")->set_text(i18n::text(
                                            std::any_of(offered.begin(), offered.end(),
                                                    [](const auto & c)
    {
        return c.verb == "melee";
    })
    ? "No movement squares available. Attack an adjacent enemy or end the turn."
    : "No move or melee attack available. End the turn or use another action."));
    if (player && (mode_ == "stabilize" || mode_ == "throw" ||
                   (mode_.starts_with("light_") || mode_.starts_with("nick_"))))
    {
        std::vector<Command> targets;
        for (const auto &command : offered)
            if (command.verb == mode_ && matches_item(command))
                targets.push_back(command);
        if (!targets.empty())
        {
            if (std::none_of(targets.begin(), targets.end(),
                             [&](const auto & c)
        {
            return c.target == aid_target_;
        }))
            aid_target_ = targets.front().target;
            const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                             [&](const auto & a)
            {
                return a.id == aid_target_;
            });
            if (target != s.combatants.end())
                get_node<Label>("Prompt")->set_text(i18n::format(
                                                        mode_.starts_with("nick_")
                                                        ? N_("Nick attack: {name}\nLeft/Right: target | Space: use")
                                                        : mode_ == "throw" ? N_("Throw: {name}\nLeft/Right: target | Space: use")
                                                        : N_("Stabilize: {name}\nLeft/Right: target | Space: use"),
            {{"name", gs(target->name)}}));
        }
    }
    get_node<Label>("Footer")->set_text(
        player && (mode_ == "stabilize" || mode_ == "throw" ||
                   (mode_.starts_with("light_") || mode_.starts_with("nick_")))
        ? get_node<Label>("Prompt")->get_text().replace("\n", " | ") + " | " +
        i18n::text("Escape: cancel")
        : i18n::text(
            "Arrows/Numpad: move | Shift+arrow: diagonal | A: action | Space: use | Z: slot | Enter: end"));
    if (player && s.free_movement)
        get_node<Label>("Footer")->set_text(i18n::format(
                                                "Free move: {feet} ft | Arrows/click: move | Escape or Finish free move: finish",
    {{"feet", s.free_movement->remaining_feet}}));
    if (player && s.effect_targeting)
        get_node<Label>("Prompt")->set_text(
            i18n::render(s.effect_targeting->prompt) + "\n" +
            i18n::text("Arrows: choose | Space: use | Escape: skip"));
    if (player && s.area_targeting)
        get_node<Label>("Prompt")->set_text(
            i18n::text("Aim the spell.") + "\n" +
            i18n::text("Left click or arrows: move | Right click, Space or Enter: cast | Escape: cancel"));
    if (player && s.spell_targeting)
    {
        String chosen;
        for (const auto id : s.spell_targeting->chosen)
            for (const auto &a : s.combatants)
                if (a.id == id)
                    chosen += (chosen.is_empty() ? "" : ", ") + gs(a.name);
        get_node<Label>("Prompt")->set_text(
            i18n::format("Choose up to {maximum} creatures. Chosen: {chosen}",
        {
            {"maximum", static_cast<int64_t>(s.spell_targeting->maximum)},
            {"chosen", chosen}
        }) +
        "\n" + i18n::text("Click: choose or remove | Space or Cast spell: cast | Escape: cancel"));
    }
    String log = turn + "\n" + get_node<Label>("Prompt")->get_text() + "\n" +
                 i18n::text("A: next action | Space: use | Z: spell slot | Enter: end turn") +
                 "\n\n";
    if (demo_)
        log += i18n::campaign("por/combat/dialogue", demo_->dialogue()) + "\n\n";
    // Rebuild one startup notice per missing combination; refreshes never append duplicates.
    std::set<std::string> missing_combinations;
    for (const auto &[entity, combination] : missing_art_)
        missing_combinations.insert(combination);
    for (const auto &combination : missing_combinations)
        log += i18n::text("No combat artwork assigned") + ": " + gs(combination) + ". " +
               i18n::text("Showing unarmed with the saved body.") + "\n";
    if (s.log_messages.size() == s.log.size())
        for (const auto &entry : s.log_messages)
            log += i18n::render(entry) + "\n";
    else
        for (const auto &entry : s.log)
            log += i18n::text(entry) + "\n";
    if (!error_.empty())
        log += "\n" + i18n::text(error_);
    auto *log_view = get_node<RichTextLabel>("Log");
    auto *log_scroll = log_view->get_v_scroll_bar();
    const double previous_scroll = log_scroll->get_value();
    const bool follow_bottom =
        previous_scroll >= log_scroll->get_max() - log_scroll->get_page() - 2;
    log_view->set_text(log);
    if (follow_bottom)
        log_view->scroll_to_line(std::max(0, log_view->get_line_count() - 1));
    else
        log_scroll->set_value(previous_scroll);
    get_node<Button>("Continue")->hide();
    if (!campaign_ && !s.free_movement && !s.effect_targeting)
        get_node<Button>("End")->hide();
    get_node<Control>("BattlefieldScroll/Canvas")->queue_redraw();
    queue_redraw();
    update_hover(get_viewport()->get_mouse_position());
}

void CombatView::center_on(Cell cell)
{
    auto *scroll = get_node<ScrollContainer>("BattlefieldScroll");
    const double tile = combat_zoom_ * base_tile_;
    scroll->set_h_scroll(static_cast<int>((cell.x + .5) * tile - scroll->get_size().x * .5));
    scroll->set_v_scroll(static_cast<int>((cell.y + .5) * tile - scroll->get_size().y * .5));
}

void CombatView::_draw()
{
    draw_rect(Rect2(Vector2(), get_size()), Color("121a20"));
    draw_rect(board_rect_, Color("202d33"));
    if (!campaign_ || !demo_ || !demo_->has_combat())
        return;
    const auto snapshot = demo_->combat().snapshot();
    const auto font = get_theme_default_font();
    const double right = get_size().x - 382, row_height = (get_size().y - 120) / 8.0;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        const auto id = campaign_->state().slots[slot];
        const double top = 60 + slot * row_height;
        const Rect2 row(right, top, 358, row_height - 4);
        draw_rect(row, id && id == selected_ ? Color("344950") : Color("1b282e"));
        if (!id)
            continue;
        const auto &member = campaign_->member(id);
        const auto found = std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                                        [&](const auto & c)
        {
            return c.id == id;
        });
        const int hp =
            found == snapshot.combatants.end() ? member.vitals.hit_points : found->hit_points;
        const int maximum = found == snapshot.combatants.end() ? campaign_->hit_point_maximum(id)
                            : found->max_hit_points;
        const double size = std::min(64.0, row_height - 18), portrait_y = top + 4;
        const Rect2 image_rect(right + 5, portrait_y, size, size);
        draw_rect(image_rect, Color("10171c"));
        if (const auto portrait = portraits_.find(id); portrait != portraits_.end())
            draw_texture_rect(portrait->second, image_rect, false);
        else if (const auto sprite = art_.find(id); sprite != art_.end())
            draw_texture_rect(sprite->second.texture, image_rect, false);
        draw_rect(Rect2(right + 5, portrait_y + size + 2, size, 5), Color("37191d"));
        draw_rect(Rect2(right + 5, portrait_y + size + 2,
                        size * std::clamp(double(hp) / std::max(1, maximum), 0.0, 1.0), 5),
                  Color(presentation::hp_color(hp, maximum)));
        const double text_x = right + 82;
        const auto line = [&](String value, double y, int size, Color color)
        {
            auto cursor = Vector2(text_x, y);
            for (int i = 0; i < value.length(); ++i)
                cursor.x +=
                    font->draw_char(get_canvas_item(), cursor, value.unicode_at(i), size, color);
        };
        line(gs(member.character.sheet().name), top + 27, 17, Color("e2edf0"));
        const auto &sheet = member.character.sheet();
        line(gs(sheet.character_class).capitalize() + " / " + gs(sheet.race).capitalize() + " / " +
             gs(sheet.gender).capitalize(),
             top + 49, 13, Color("a8c1c7"));
    }
}

void CombatView::draw_battlefield()
{
    if (!demo_ || !demo_->has_combat())
        return;
    auto *canvas = get_node<Control>("BattlefieldScroll/Canvas");
    canvas->draw_set_transform(Vector2(), 0, Vector2(combat_zoom_, combat_zoom_));
    const auto s = demo_->combat().snapshot();
    const double tile = base_tile_;
    const auto font = get_theme_default_font();
    for (int y = 0; y < s.battlefield.height; ++y)
        for (int x = 0; x < s.battlefield.width; ++x)
        {
            const Rect2 cell(Vector2(x * tile, y * tile), Vector2(tile, tile));
            const auto terrain = s.battlefield.at({x, y});
            canvas->draw_rect(cell, terrain == 1 ? Color("64716d")
                              : terrain == 2
                              ? Color("665238")
                              : ((x + y) % 2 ? Color("29373c") : Color("253137")));
            const auto index = y * s.battlefield.width + x;
            if (index < demo_->battlefield_tiles().size() &&
                    demo_->battlefield_tiles()[index] < terrain_art_.size())
                canvas->draw_texture_rect(terrain_art_[demo_->battlefield_tiles()[index]], cell,
                                          false);
            else
                canvas->draw_rect(cell, Color("172228"), false);
        }
    const auto active = std::find_if(s.combatants.begin(), s.combatants.end(),
                                     [&](const auto & a)
    {
        return a.id == s.actor;
    });
    const auto selected = std::find_if(s.combatants.begin(), s.combatants.end(),
                                       [&](const auto & a)
    {
        return a.id == selected_ && a.side == 0;
    });
    if (selected != s.combatants.end() && !selected->dead)
    {
        if (selected_ == s.actor)
            for (const auto p : demo_->combat().movement_reach(selected_))
                canvas->draw_rect(
                    Rect2(Vector2(p.x * tile + 1, p.y * tile + 1), Vector2(tile - 2, tile - 2)),
                    Color(1, 1, 1, .18));
        canvas->draw_rect(Rect2(Vector2(selected->cell.x * tile + 1, selected->cell.y * tile + 1),
                                Vector2(tile - 2, tile - 2)),
                          Color("e7c484"), false, 2.0);
    }
    if (active != s.combatants.end() && active->side == 0 && mode_ != "move")
        for (const auto &c : demo_->combat().legal_commands())
            if (c.verb == mode_ && matches_item(c) && c.target && !s.effect_targeting)
            {
                const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                                 [&](const auto & a)
                {
                    return a.id == c.target;
                });
                if (target != s.combatants.end())
                    canvas->draw_rect(
                        Rect2(Vector2(target->cell.x * tile + 1, target->cell.y * tile + 1),
                              Vector2(tile - 2, tile - 2)),
                        Color(.4, .8, .75,
                              (mode_ == "stabilize" || mode_ == "throw" ||
                               (mode_.starts_with("light_") || mode_.starts_with("nick_"))) &&
                              c.target == aid_target_
                              ? .6
                              : .23));
            }
    // Silence's squares, then Heavily Obscured ones such as Fog Cloud's.
    for (const auto cell : s.silenced)
        canvas->draw_rect(Rect2(Vector2(cell.x * tile, cell.y * tile), Vector2(tile, tile)),
                          Color(.45, .5, .85, .25));
    for (const auto cell : s.obscured)
        canvas->draw_rect(Rect2(Vector2(cell.x * tile, cell.y * tile), Vector2(tile, tile)),
                          Color(.78, .8, .82, .35));
    if (s.area_targeting)
        for (const auto cell : s.area_targeting->cells)
            canvas->draw_rect(Rect2(Vector2(cell.x * tile + 1, cell.y * tile + 1),
                                    Vector2(tile - 2, tile - 2)),
                              Color(.55, .8, .35, .38));
    if (s.effect_targeting && active != s.combatants.end() && active->side == 0)
    {
        unsigned index = 0;
        for (const auto &c : demo_->combat().legal_commands())
            if (c.verb == s.effect_targeting->verb)
            {
                auto cell = c.destination;
                if (!s.effect_targeting->destination)
                {
                    const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                                     [&](const auto & a)
                    {
                        return a.id == c.target;
                    });
                    if (target == s.combatants.end())
                        continue;
                    cell = target->cell;
                }
                canvas->draw_rect(Rect2(Vector2(cell.x * tile + 1, cell.y * tile + 1),
                                        Vector2(tile - 2, tile - 2)),
                                  Color(.4, .8, .75, index++ == effect_target_index_ ? .65 : .23));
            }
    }
    for (const auto index : presentation::combat_sprite_draw_order(s.combatants))
    {
        const auto &a = s.combatants[index];
        const auto center = Vector2((a.cell.x + .5) * tile, (a.cell.y + .5) * tile);
        if (a.dead)
        {
            if (skull_art_.is_valid() && skull_seconds_.contains(a.id))
                canvas->draw_texture_rect(
                    skull_art_,
                    Rect2(Vector2(a.cell.x * tile, a.cell.y * tile), Vector2(tile, tile)), false);
            continue;
        }
        if (art_.contains(a.id))
        {
            const auto &art = art_.at(a.id);
            const bool left = a.facing_left;
            const bool acting = action_seconds_.contains(a.id) && art.action.is_valid();
            const bool unconscious = a.prone || !a.conscious;
            const auto texture = unconscious ? art.unconscious
                                 : left      ? (acting ? art.left_action : art.left_texture)
                                 : (acting ? art.action : art.texture);
            const auto rect = presentation::combat_sprite_rect(
                                  texture->get_size(),
                                  unconscious ? art.unconscious_visible
                                  : left      ? art.left_visible
                                  : art.visible,
                                  Rect2(Vector2(a.cell.x * tile, a.cell.y * tile), Vector2(tile, tile)),
                                  art.goliath && !unconscious);
            canvas->draw_texture_rect(texture, rect, false,
                                      !a.conscious ? Color(.65, .65, .65) : Color(1, 1, 1));
        }
        else
        {
            const auto number = std::to_string(a.id);
            auto cursor = center + Vector2(-5.5 * number.size(), 7);
            for (const char digit : number)
            {
                canvas->draw_char(font, cursor, gs(std::string(1, digit)), 20,
                                  a.side == 0 ? Color("79d6d4") : Color("dd9874"));
                cursor.x += 11;
            }
        }
    }
}

void CombatView::_process(double delta)
{
    if (Engine::get_singleton()->is_editor_hint())
        return;
    try
    {
        for (auto it = action_seconds_.begin(); it != action_seconds_.end();)
        {
            it->second -= delta;
            if (it->second <= 0)
                it = action_seconds_.erase(it);
            else
                ++it;
            get_node<Control>("BattlefieldScroll/Canvas")->queue_redraw();
        }
        for (auto it = skull_seconds_.begin(); it != skull_seconds_.end();)
        {
            it->second -= delta;
            if (it->second <= 0)
                it = skull_seconds_.erase(it);
            else
                ++it;
            get_node<Control>("BattlefieldScroll/Canvas")->queue_redraw();
        }
        if ((checking_ || expedition_check_) && !error_.empty())
            throw std::runtime_error(error_);
        if (!demo_)
            return;
        if (checking_ && demo_->waiting())
        {
            next();
            return;
        }
        if (!demo_->has_combat())
            return;
        const auto s = demo_->combat().snapshot();
        if (zoom_center_frames_)
            --zoom_center_frames_;
        else
            for (const auto &a : s.combatants)
                if (a.id == (selected_ ? selected_ : s.actor))
                {
                    const auto current = std::pair{a.id, a.cell};
                    if (followed_ != current)
                    {
                        center_on(a.cell);
                        followed_ = current;
                    }
                }
        if ((checking_ || expedition_check_) && capture_ && !captured_)
        {
            if (++completion_frames_ < 3)
                return;
            completion_frames_ = 0;
            const auto file = local_path(expedition_check_ ? "user://checks/slums-battlefield.png"
                                         : check_slums_    ? "user://checks/slums-combat.png"
                                         : "user://checks/training-combat.png");
            std::filesystem::create_directories(file.parent_path());
            const auto image = get_viewport()->get_texture()->get_image();
            if (image.is_null() || image->save_png(gs(file.generic_string())) != OK)
                throw std::runtime_error("Combat capture failed");
            captured_ = true;
        }
        if (s.outcome != Outcome::ongoing)
        {
            if (checking_ && ++completion_frames_ > 2)
            {
                if (check_slums_)
                {
                    if (!demo_->script_complete())
                        throw std::runtime_error("Original ECL did not finish");
                    if (s.outcome == Outcome::victory)
                        demo_->revisit();
                }
                UtilityFunctions::print(
                    "Godot C++ combat check passed: ", check_slums_ ? "Slums" : "training",
                    ", commands ", check_steps_, ", outcome ",
                    s.outcome == Outcome::victory ? "victory" : "defeat");
                checking_ = false;
                get_tree()->quit(0);
            }
            return;
        }
        const auto active = std::find_if(s.combatants.begin(), s.combatants.end(),
                                         [&](const auto & a)
        {
            return a.id == s.actor;
        });
        if (checking_ && active->side == 1)
        {
            Ref<InputEventKey> key;
            key.instantiate();
            key->set_keycode(Key::KEY_ENTER);
            key->set_pressed(true);
            _input(key);
            if (demo_->combat().snapshot().revision != s.revision)
                throw std::runtime_error("Keyboard skipped enemy turn");
        }
        if (checking_ && !checked_input_ && active->side == 0 && !s.reaction_pending)
        {
            const auto command = choose_demo_command(demo_->combat());
            if (command.target)
                for (const auto &[node, verb] : action_buttons)
                    if (command.verb == verb)
                    {
                        get_node<Button>(node)->emit_signal("pressed");
                        const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                                         [&](const auto & a)
                        {
                            return a.id == command.target;
                        });
                        Ref<InputEventMouseButton> mouse;
                        mouse.instantiate();
                        mouse->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
                        mouse->set_pressed(true);
                        if (!check_target_centered_)
                        {
                            center_on(target->cell);
                            check_target_centered_ = true;
                            return; // ScrollContainer applies the canvas offset during its layout
                            // pass.
                        }
                        mouse->set_position(
                            get_node<Control>("BattlefieldScroll/Canvas")
                            ->get_global_transform_with_canvas()
                            .xform(Vector2(target->cell.x + .5, target->cell.y + .5) *
                                   (combat_zoom_ * base_tile_)));
                        get_viewport()->push_input(mouse, true);
                        if (demo_->combat().snapshot().revision != s.revision + 1)
                            throw std::runtime_error(
                                "Action button/target click did not submit command");
                        checked_input_ = true;
                        ++check_steps_;
                        return;
                    }
        }
        if (party_check_ && settings::flag("--adrenaline-check") && active->side == 0)
        {
            if (s.temporary_hp_offer)
            {
                get_node<Button>("TemporaryHP/Keep")->emit_signal("pressed");
                return;
            }
            if (!get_node<Button>("AdrenalineRush")->is_disabled())
            {
                const auto before = active->hit_points;
                get_node<Button>("AdrenalineRush")->emit_signal("pressed");
                const auto after = demo_->combat().snapshot();
                const auto current = std::find_if(after.combatants.begin(), after.combatants.end(),
                                                  [&](const auto & a)
                {
                    return a.id == active->id;
                });
                if (current == after.combatants.end() || current->bonus_action ||
                        current->hit_points != before ||
                        (!after.temporary_hp_offer && current->temporary_hp.amount != 2))
                    throw std::runtime_error("Ordinary Orc Adrenaline Rush control failed");
                UtilityFunctions::print("Orc campaign Adrenaline Rush button passed");
                return;
            }
        }
        if (checking_ || party_check_ || defeat_check_ || active->side == 1)
        {
            ai_delay_ += delta;
            if (!checking_ && !party_check_ && !defeat_check_ && ai_delay_ < .65)
                return;
            ai_delay_ = 0;
            if (defeat_check_)
            {
                if (++check_steps_ > 2000)
                    throw std::runtime_error("Defeat check command limit exceeded");
                if (active->side == 0)
                {
                    const auto offered = demo_->combat().legal_commands();
                    const auto pass =
                        std::find_if(offered.begin(), offered.end(),
                                     [&](const auto & c)
                    {
                        return c.verb == (s.reaction_pending ? "decline" : "end");
                    });
                    if (pass == offered.end())
                        throw std::runtime_error("Defeat check cannot pass party turn");
                    act(*pass);
                    return;
                }
            }
            if (checking_ && ++check_steps_ > 1000)
            {
                std::string details = "Combat check command limit exceeded";
                for (const auto &line : s.log)
                    details += "\n" + line;
                throw std::runtime_error(details);
            }
            act(choose_demo_command(demo_->combat()));
        }
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
        if (checking_ || defeat_check_ || expedition_check_)
        {
            UtilityFunctions::push_error(gs(error_));
            checking_ = false;
            get_tree()->quit(1);
        }
    }
}
