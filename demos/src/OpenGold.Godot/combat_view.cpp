#include "../../../src/OpenGoldBox/initiative_controls.h"
#include "../../../src/OpenGoldBox/optional_effect_controls.h"
#include "../../../src/OpenGoldBox/nick_controls.h"
#include "../../../src/OpenGoldBox/combat_weapon_controls.h"
#include "combat_view.h"
#include "opengold/srd5.h"
#include "opengold/combat_body_catalog.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
using namespace godot;
using namespace opengold;
using namespace opengold::rules;

namespace
{
String gs(std::string_view text)
{
    return String::utf8(text.data(), text.size());
}

String render_weapon_message(const Message &message)
{
    auto text = gs(message.source);
    for (const auto &a : message.arguments)
        text = text.replace(gs("{" + a.name + "}"), gs(a.value));
    return text;
}

const std::array<std::pair<const char *, const char *>, 10> action_buttons
{
    {   {"Melee", "melee"},
        {"Ranged", "ranged"},
        {"FireBolt", "fire_bolt"},
        {"MagicMissile", "magic_missile"},
        {"CureWounds", "cure_wounds"},
        {"HealingWord", "healing_word"},
        {"ScorchingRay", "scorching_ray"},
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
} // namespace

void CombatView::_bind_methods()
{
}

void CombatView::_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && ready_)
    {
        layout();
        queue_redraw();
    }
}

std::filesystem::path CombatView::local_path(const char *path) const
{
    return std::filesystem::u8path(
               ProjectSettings::get_singleton()->globalize_path(path).utf8().get_data());
}

void CombatView::_ready()
{
    presentation::setup_nick(
        *this, gs, callable_mp(this, &CombatView::begin_nick),
        callable_mp(this, &CombatView::nick_selected), callable_mp(this, &CombatView::confirm_nick),
        callable_mp(this, &CombatView::cancel_nick), callable_mp(this, &CombatView::nick_input));
    presentation::setup_initiative(
        *this, gs, callable_mp(this, &CombatView::immediate).bind("initiative_swap"),
        callable_mp(this, &CombatView::immediate).bind("initiative_keep"),
        callable_mp(this, &CombatView::initiative_input),
        callable_mp(this, &CombatView::refresh).unbind(1));
    presentation::setup_optional_effect(
        *this, gs, callable_mp(this, &CombatView::immediate).bind("effect_use"),
        callable_mp(this, &CombatView::immediate).bind("effect_skip"),
        callable_mp(this, &CombatView::optional_effect_input),
        callable_mp(this, &CombatView::refresh).unbind(1));
    presentation::setup_weapon_controls(*this, gs, callable_mp(this, &CombatView::weapon_selected));
    ready_ = true;
    get_window()->set_min_size(Vector2i(1120, 800));
    set_texture_filter(TEXTURE_FILTER_NEAREST);
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    for (const auto &[node, verb] : action_buttons)
        get_node<Button>(node)->connect(
            "pressed", callable_mp(this, &CombatView::select_mode).bind(String(verb)));
    get_node<Button>("Move")->connect(
        "pressed", callable_mp(this, &CombatView::select_mode).bind(String("move")));
    get_node<Button>("SpellSlot")->connect("pressed", callable_mp(this, &CombatView::spell_slot));
    get_node<OptionButton>("CunningAction")
    ->connect("item_selected", callable_mp(this, &CombatView::bonus_selected));
    get_node<Button>("UseCunningAction")
    ->connect("pressed", callable_mp(this, &CombatView::use_bonus_action));
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
    get_node<Button>("Replay")->connect("pressed", callable_mp(this, &CombatView::replay));
    get_node<Button>("Continue")->connect("pressed", callable_mp(this, &CombatView::next));
    get_node<Button>("Revisit")->connect("pressed", callable_mp(this, &CombatView::revisit));
    get_node<Button>("Save")->connect("pressed", callable_mp(this, &CombatView::save_game));
    get_node<Button>("Load")->connect("pressed", callable_mp(this, &CombatView::load_game));
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
        demo_ = std::make_unique<CombatDemo>(
                    srd5::load(local_path("res://../../data/rules/srd-5.2.1/combat.rules")));
        if (campaign_)
            demo_->campaign_party(campaign_);
        if (encounter_)
            demo_->encounter(*encounter_, 42);
        else if (check_slums_)
            slums();
        else
            training();
        sync_art();
        layout();
        refresh();
        get_node<Button>("Save")->hide();
        get_node<Button>("Load")->hide();
        if (campaign_)
            for (const char *name :
                    {"Training", "Slums", "Replay", "Save", "Load", "Revisit"
                    })
                get_node<Control>(name)->hide();
        if (encounter_)
        {
            get_node<Label>("Title")->set_text("SLUMS / Combat");
            get_node<Label>("Subtitle")
            ->set_text("Choose an action, then click its target. Enter ends your turn.");
            get_node<Label>("Footer")->set_text(
                "Each square is 5 feet. Victory returns your party to exploration.");
            get_node<Label>("Help")->set_text(
                "Teal: party | Orange: enemies\nYour HP and spent resources carry forward.");
        }
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        refresh();
    }
}

void CombatView::layout()
{
    const double width = get_size().x, height = get_size().y, sidebar = 358,
                 left_width = width - sidebar - 72;
    const auto board = demo_ && demo_->has_combat() ? demo_->combat().snapshot().battlefield
                       : Battlefield{12, 9, {}};
    const bool recovery =
        get_node<Button>("Stabilize")->is_visible() || get_node<Button>("StandUp")->is_visible();
    const double weapon_height = get_node<OptionButton>("Weapons")->is_visible() ? 44 : 0;
    const double bonus_height = get_node<OptionButton>("CunningAction")->is_visible() ? 44 : 0;
    const double tile = std::min(left_width / board.width,
                                 (height - weapon_height - bonus_height -
                                  (get_node<OptionButton>("ThrownWeapon")->is_visible() ? 368
                                   : recovery                                           ? 324
                                   : 280)) /
                                 board.height);
    board_rect_ = Rect2(24, 116, tile * board.width, tile * board.height);
    const double right = width - sidebar - 24;
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = get_node<Control>(name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("Title", Rect2(24, 18, left_width, 34));
    place("Subtitle", Rect2(24, 62, left_width, 45));
    place("Training", Rect2(right, 20, 112, 34));
    place("Slums", Rect2(right + 120, 20, 112, 34));
    place("Replay", Rect2(right + 240, 20, 118, 34));
    place("Turn", Rect2(right, 70, sidebar, 70));
    place("Roster", Rect2(right, 148, sidebar, 160));
    place("Prompt", Rect2(right, 318, sidebar, 46));
    unsigned index = 0;
    for (const char *name :
            {"Move", "Melee", "Ranged", "FireBolt", "MagicMissile", "CureWounds",
             "HealingWord", "ScorchingRay", "SpellSlot", "SecondWind", "Dash",
             "Dodge", "Disengage", "End", "Continue"
            })
    {
        const unsigned row = index / 3, column = index % 3;
        place(name, Rect2(right + column * 122, 370 + row * 43, 114, 36));
        ++index;
    }
    place("React", Rect2(right, 590, 174, 36));
    place("Decline", Rect2(right + 184, 590, 174, 36));
    place("Nick", Rect2(right + 184, 590, 174, 36));
    place("Save", Rect2(right, 639, 112, 34));
    place("Load", Rect2(right + 122, 639, 112, 34));
    place("Revisit", Rect2(right + 244, 639, 114, 34));
    place("Help", Rect2(right, 686, sidebar, height - 732));
    const double weapon_top = board_rect_.get_end().y + 16;
    place("WeaponLabel", Rect2(24, weapon_top, 180, 36));
    place("Weapons", Rect2(214, weapon_top, 450, 36));
    const double bonus_top = weapon_top + weapon_height;
    place("CunningActionLabel", Rect2(24, bonus_top, 180, 36));
    place("CunningAction", Rect2(214, bonus_top, 200, 36));
    place("UseCunningAction", Rect2(424, bonus_top, 240, 36));
    const double top = bonus_top + bonus_height;
    place("Stabilize", Rect2(24 + left_width - 170, top, 170, 36));
    place("StandUp", Rect2(24, top + 44, 180, 36));
    place("ThrownWeaponLabel", Rect2(24, top + 88, 190, 36));
    place("ThrownWeapon", Rect2(224, top + 88, 276, 36));
    place("Throw", Rect2(510, top + 88, 204, 36));
    place("Log",
          Rect2(24,
                top + (get_node<OptionButton>("ThrownWeapon")->is_visible() ? 132
                       : recovery                                           ? 88
                       : 0),
                left_width,
                std::max(0.0, height - board_rect_.get_end().y - 64 - weapon_height - bonus_height -
                         (get_node<OptionButton>("ThrownWeapon")->is_visible() ? 132
                          : recovery                                           ? 88
                          : 0))));
    place("Footer", Rect2(24, height - 34, width - 48, 24));
}

#include "../../../src/OpenGoldBox/nick_dialog_impl.h"

void CombatView::training()
{
    try
    {
        error_.clear();
        demo_->training();
        art_.clear();
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
        auto directory = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
        if (directory.is_empty())
            directory =
                ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
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

void CombatView::sync_art()
{
    art_.clear();
    terrain_art_.clear();
    if (!demo_)
        return;
    for (const auto &source : demo_->terrain_art())
    {
        PackedByteArray pixels;
        pixels.resize(source.rgba.size());
        std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
        terrain_art_.push_back(ImageTexture::create_from_image(godot::Image::create_from_data(
                    source.width, source.height, false, godot::Image::FORMAT_RGBA8, pixels)));
    }
    for (const auto &source : demo_->art())
    {
        PackedByteArray pixels;
        pixels.resize(source.image.rgba.size());
        std::copy(source.image.rgba.begin(), source.image.rgba.end(), pixels.ptrw());
        const auto image = godot::Image::create_from_data(
                               source.image.width, source.image.height, false, godot::Image::FORMAT_RGBA8, pixels);
        art_[source.entity] = ImageTexture::create_from_image(image);
    }
    if (campaign_)
    {
        auto directory = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
        if (directory.is_empty())
            directory =
                ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
        const auto originals =
            por::CharacterArt::load(std::filesystem::u8path(directory.utf8().get_data()));
        const auto catalog = por::CombatBodyCatalog::load(
                                 local_path("res://../../data/art/combat-body-looks.tsv"),
                                 local_path("res://../../data/art/combat-weapon-options.tsv"));
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
        PackedByteArray pixels;
        pixels.resize(source.image.rgba.size());
        std::copy(source.image.rgba.begin(), source.image.rgba.end(), pixels.ptrw());
        art_[source.entity] = ImageTexture::create_from_image(godot::Image::create_from_data(
                source.image.width, source.image.height, false, godot::Image::FORMAT_RGBA8, pixels));
    }
}

void CombatView::save_game()
{
    try
    {
        const auto bytes = demo_->save_combat();
        const auto path = local_path("res://../../user-data/combat.save");
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += ".tmp";
        auto backup = path;
        backup += ".bak";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output << bytes;
            output.close();
            if (!output)
                throw std::runtime_error("Combat save failed");
        }
        const bool previous = std::filesystem::exists(path);
        if (previous)
        {
            std::filesystem::remove(backup);
            std::filesystem::rename(path, backup);
        }
        try
        {
            std::filesystem::rename(temporary, path);
        }
        catch (...)
        {
            if (previous)
                std::filesystem::rename(backup, path);
            throw;
        }
        error_.clear();
        get_node<Label>("Prompt")->set_text("Training combat saved.");
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
        const auto path = local_path("res://../../user-data/combat.save");
        if (std::filesystem::file_size(path) > 65536)
            throw std::runtime_error("Combat save exceeds limit");
        std::ifstream input(path, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(input), {}};
        if (input.bad())
            throw std::runtime_error("Combat save read failed");
        demo_->restore_combat(bytes);
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
    mode_ = spell_verb(verb.utf8().get_data(), spell_slot_);
    if (mode_ == "dash" || mode_ == "dodge" || mode_ == "disengage")
    {
        immediate(verb);
        return;
    }
    refresh();
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

void CombatView::bonus_selected(std::int64_t)
{
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

void CombatView::use_bonus_action()
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
    else
        immediate(verb);
}

void CombatView::spell_slot()
{
    spell_slot_ = spell_slot_ == 1 ? 2 : 1;
    mode_ = "move";
    refresh();
}

void CombatView::immediate(String verb)
{
    if (!demo_ || !demo_->has_combat())
        return;
    auto wanted = std::string(verb.utf8().get_data());
    const auto state = demo_->combat().snapshot();
    if (state.effect_targeting && wanted == "end")
        wanted = "effect_skip";
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
}

void CombatView::act(const Command &command)
{
    try
    {
        if (demo_->submit(command))
        {
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
    if (get_node<Window>("TacticalMind")->is_visible())
        return;
    if (!demo_ || defeated() || Engine::get_singleton()->is_editor_hint())
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
        Cell direction{};
        switch (key->get_keycode())
        {
        case Key::KEY_LEFT:
            direction.x = -1;
            break;
        case Key::KEY_RIGHT:
            direction.x = 1;
            break;
        case Key::KEY_UP:
            direction.y = -1;
            break;
        case Key::KEY_DOWN:
            direction.y = 1;
            break;
        default:
            break;
        }
        if (direction.x || direction.y)
        {
            const auto state = demo_->combat().snapshot();
            const auto actor = std::find_if(state.combatants.begin(), state.combatants.end(),
                                            [&](const auto & a)
            {
                return a.id == state.actor && a.side == 0;
            });
            if (actor != state.combatants.end())
                for (const auto &command : demo_->combat().legal_commands())
                    if (command.verb == "move" &&
                            command.destination ==
                            Cell{actor->cell.x + direction.x, actor->cell.y + direction.y})
                    {
                        act(command);
                        break;
                    }
            get_viewport()->set_input_as_handled();
            return;
        }
    }
    if (key.is_valid() && (get_node<OptionButton>("Weapons")->has_focus() ||
                           get_node<OptionButton>("Weapons")->get_popup()->is_visible()))
        return;
    if (key.is_valid() && (get_node<OptionButton>("CunningAction")->has_focus() ||
                           get_node<OptionButton>("CunningAction")->get_popup()->is_visible()))
        return;
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
            (get_node<Button>("Nick")->has_focus() ||
             get_node<Button>("UseCunningAction")->has_focus() ||
             get_node<Button>("Throw")->has_focus() ||
             get_node<Button>("Stabilize")->has_focus() ||
             get_node<Button>("StandUp")->has_focus()) &&
            (key->get_keycode() == Key::KEY_ENTER || key->get_keycode() == Key::KEY_SPACE))
        return;
    if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::KEY_ESCAPE &&
            (mode_ == "stabilize" || mode_ == "throw" ||
             (mode_.starts_with("light_") || mode_.starts_with("nick_"))))
    {
        mode_ = "move";
        refresh();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (key.is_valid() && key->is_pressed() && !key->is_echo() &&
            key->get_keycode() == Key::KEY_ENTER)
    {
        if (demo_->waiting())
            next();
        else
            immediate("end");
        get_viewport()->set_input_as_handled();
        return;
    }
    const Ref<InputEventMouseButton> mouse = event;
    if (mouse.is_null() || !mouse->is_pressed() ||
            mouse->get_button_index() != MouseButton::MOUSE_BUTTON_LEFT || !demo_->has_combat())
        return;
    const auto local =
        get_global_transform_with_canvas().affine_inverse().xform(mouse->get_position());
    if (!board_rect_.has_point(local))
        return;
    const auto s = demo_->combat().snapshot();
    const auto current = std::find_if(s.combatants.begin(), s.combatants.end(),
                                      [&](const auto & a)
    {
        return a.id == s.actor;
    });
    if (current == s.combatants.end() || current->side != 0)
        return;
    const auto relative = (local - board_rect_.position) /
                          (board_rect_.size.x / demo_->combat().snapshot().battlefield.width);
    const Cell cell{static_cast<int>(relative.x), static_cast<int>(relative.y)};
    for (const auto &c : demo_->combat().legal_commands())
        if (c.verb == mode_ && matches_item(c))
        {
            if ((c.verb == "move" || c.verb == "effect_push") && c.destination == cell)
            {
                act(c);
                break;
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
                    break;
                }
            }
        }
    get_viewport()->set_input_as_handled();
}

void CombatView::refresh()
{
    if (!ready_)
        return;
    const bool loaded = demo_ && demo_->has_combat();
    Snapshot s;
    if (loaded)
        s = demo_->combat().snapshot();
    bool player = false;
    std::string turn = demo_ ? demo_->status() : "Unable to load rules";
    if (loaded && s.outcome == Outcome::ongoing)
        for (const auto &a : s.combatants)
            if (a.id == s.actor)
            {
                player = a.side == 0;
                turn = "Round " + std::to_string(s.round) + " / " + a.name +
                       (s.reaction_pending ? " reaction" : " turn") + "\nMove " +
                       std::to_string(a.movement_feet) + " ft | " +
                       (a.action ? "Action ready" : "Action spent") + "\n" + a.status;
            }
    if (loaded && s.outcome != Outcome::ongoing)
        turn = s.outcome == Outcome::victory ? "Victory" : "Party incapacitated / defeat";
    get_node<Label>("Turn")->set_text(gs(turn));
    std::string roster;
    for (const auto &a : s.combatants)
        roster += (a.id == s.actor ? "> " : "  ") + std::to_string(a.id) + " " + a.name + "  " +
                  std::to_string(a.hit_points) + "/" + std::to_string(a.max_hit_points) +
                  " HP  AC " + std::to_string(a.armor_class) + "\n";
    get_node<RichTextLabel>("Roster")->set_text(gs(roster));
    auto *mind = get_node<Window>("TacticalMind");
    if (player && s.ability_check_choice)
    {
        const auto &check = *s.ability_check_choice;
        const bool changed = !mind->is_visible();
        get_node<Label>("TacticalMind/Text")
        ->set_text(gs(
                       "Failed Medicine check: d20 " + std::to_string(check.natural) + " + " +
                       std::to_string(check.modifier) + " = " + std::to_string(check.total) + " vs DC " +
                       std::to_string(check.difficulty) +
                       ".\nSecond Wind uses: " + std::to_string(check.resource_uses) +
                       "\n\nAdd 1d10. Spend one use only if the check succeeds.\nThe original Action is already spent."));
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
    get_node<Button>("End")->set_text(gs(s.effect_targeting ? "Skip effect"
                                         : s.free_movement  ? "Finish free move"
                                         : "End turn"));
    get_node<Button>("End")->set_size(Vector2(s.free_movement
            ? get_node<Button>("Continue")->get_position().x +
            get_node<Button>("Continue")->get_size().x -
            get_node<Button>("End")->get_position().x
            : get_node<Button>("Continue")->get_size().x,
            36));
    get_node<Button>("Continue")->set_visible(!s.free_movement);
    if (s.free_movement)
        mode_ = "move";
    if (s.effect_targeting)
        mode_ = s.effect_targeting->verb;
    const auto offered = loaded ? demo_->combat().legal_commands() : std::vector<Command> {};
    const auto enabled = [&](std::string_view verb)
    {
        return player && std::any_of(offered.begin(), offered.end(),
                                     [&](const auto & c)
        {
            return c.verb == verb ||
                   (verb == "end" && s.effect_targeting &&
                    c.verb == "effect_skip");
        });
    };
    const auto active = std::find_if(s.combatants.begin(), s.combatants.end(),
                                     [&](const auto & a)
    {
        return a.id == s.actor && a.side == 0;
    });
    const auto *choice_actor =
        active != s.combatants.end() && s.outcome == Outcome::ongoing ? &*active : nullptr;
    const bool weapon_layout =
        presentation::refresh_weapons(*this, choice_actor, player,
                                      [](const Message & message)
    {
        return render_weapon_message(message);
    });
    const bool bonus_layout =
        presentation::refresh_bonus_attacks(*this, choice_actor, offered, player, gs,
                                            [](const Message & message)
    {
        return render_weapon_message(message);
    });
    presentation::refresh_nick(*this, choice_actor, player && !s.reaction_pending,
                               [](const Message & message)
    {
        return render_weapon_message(message);
    });
    presentation::refresh_initiative(*this, s, offered, gs,
                                     [](const Message & m)
    {
        return render_weapon_message(m);
    });
    presentation::refresh_optional_effect(*this, s.optional_effect_choice, player,
                                          [](const Message & m)
    {
        return render_weapon_message(m);
    });
    if (s.reaction_pending)
        get_node<Button>("Nick")->hide();
    get_node<Button>("Decline")->set_visible(!get_node<Button>("Nick")->is_visible());
    if (weapon_layout || bonus_layout)
        layout();
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
            auto label = gs(option.label.source);
            for (const auto &arg : option.label.arguments)
                label = label.replace(gs("{" + arg.name + "}"), gs(arg.value));
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
    get_node<Button>("Stabilize")
    ->set_visible(s.outcome == Outcome::ongoing &&
                  std::any_of(s.combatants.begin(), s.combatants.end(),
                              [](const auto & a)
    {
        return !a.dead && a.hit_points == 0;
    }));
    get_node<Button>("Stabilize")->set_disabled(!enabled("stabilize"));
    get_node<Button>("StandUp")->set_visible(s.outcome == Outcome::ongoing &&
            std::any_of(s.combatants.begin(), s.combatants.end(),
                        [&](const auto & a)
    {
        return a.id == s.actor &&
               a.side == 0 && a.prone;
    }));
    get_node<Button>("StandUp")->set_disabled(!enabled("stand_up"));
    layout();
    for (const auto &[node, verb] : action_buttons)
        get_node<Button>(node)->set_disabled(!enabled(spell_verb(verb, spell_slot_)));
    get_node<Button>("SpellSlot")->set_text("Slot level " + String::num_uint64(spell_slot_));
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
    get_node<Button>("Continue")->set_disabled(!demo_ || !demo_->waiting());
    get_node<Button>("Save")->set_disabled(!loaded || demo_->is_slums());
    get_node<Button>("Load")->set_disabled(!loaded || demo_->is_slums());
    get_node<Button>("Revisit")->set_disabled(!loaded || !demo_->script_complete() ||
            s.outcome != Outcome::victory);
    get_node<Label>("Prompt")->set_text(
        gs(!error_.empty()                           ? error_
           : demo_ && demo_->waiting()               ? "Read the encounter text, then Continue."
           : loaded && s.outcome != Outcome::ongoing ? demo_->status()
           : s.reaction_pending                      ? "Use or decline the opportunity attack."
           : player ? "Selected: " + mode_ + ". Click a highlighted square."
           : "Enemy turn"));
    std::string log = demo_ ? demo_->dialogue() + "\n\n" : "";
    for (const auto &entry : s.log)
        log += entry + "\n";
    if (!error_.empty())
        log += "\n" + error_;
    get_node<RichTextLabel>("Log")->set_text(gs(log));
    get_node<RichTextLabel>("Log")->scroll_to_line(
        std::max(0, get_node<RichTextLabel>("Log")->get_line_count() - 1));
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
                get_node<Label>("Prompt")->set_text(
                    gs(std::string(mode_.starts_with("nick_") ? "Nick attack: "
                                   : mode_ == "throw"         ? "Throw: "
                                   : "Stabilize: ") +
                       target->name + "\nLeft/Right: target | Space: use"));
        }
    }
    if (player && s.free_movement)
        get_node<Label>("Prompt")->set_text(
            gs("Free move: " + std::to_string(s.free_movement->remaining_feet) +
               " ft\nArrows/click: move | Escape: finish"));
    if (player && s.effect_targeting)
        get_node<Label>("Prompt")->set_text(render_weapon_message(s.effect_targeting->prompt) +
                                            "\n" +
                                            gs("Arrows: choose | Space: use | Escape: skip"));
    queue_redraw();
}

void CombatView::_draw()
{
    draw_rect(Rect2(Vector2(), get_size()), Color("121a20"));
    draw_rect(board_rect_, Color("202d33"));
    if (!demo_ || !demo_->has_combat())
        return;
    const auto s = demo_->combat().snapshot();
    const double tile = board_rect_.size.x / s.battlefield.width;
    const auto font = get_theme_default_font();
    for (int y = 0; y < s.battlefield.height; ++y)
        for (int x = 0; x < s.battlefield.width; ++x)
        {
            const Rect2 cell(board_rect_.position + Vector2(x * tile, y * tile),
                             Vector2(tile, tile));
            const auto terrain = s.battlefield.at({x, y});
            draw_rect(cell, terrain == 1   ? Color("64716d")
                      : terrain == 2 ? Color("665238")
                      : ((x + y) % 2 ? Color("29373c") : Color("253137")));
            const auto index = y * s.battlefield.width + x;
            if (index < demo_->battlefield_tiles().size() &&
                    demo_->battlefield_tiles()[index] < terrain_art_.size())
                draw_texture_rect(terrain_art_[demo_->battlefield_tiles()[index]], cell, false);
            else
                draw_rect(cell, Color("172228"), false);
        }
    const auto active = std::find_if(s.combatants.begin(), s.combatants.end(),
                                     [&](const auto & a)
    {
        return a.id == s.actor;
    });
    unsigned effect_index = 0;
    if (active != s.combatants.end() && active->side == 0)
        for (const auto &c : demo_->combat().legal_commands())
            if (c.verb == mode_ && matches_item(c))
            {
                auto p = c.destination;
                if (c.target && c.verb != "effect_push")
                {
                    const auto target = std::find_if(s.combatants.begin(), s.combatants.end(),
                                                     [&](const auto & a)
                    {
                        return a.id == c.target;
                    });
                    if (target == s.combatants.end())
                        continue;
                    p = target->cell;
                }
                draw_rect(Rect2(board_rect_.position + Vector2(p.x * tile + 2, p.y * tile + 2),
                                Vector2(tile - 4, tile - 4)),
                          Color(.4, .8, .75,
                                s.effect_targeting
                                ? (effect_index++ == effect_target_index_ ? .65 : .17)
                                : (mode_ == "stabilize" || mode_ == "throw" ||
                                   (mode_.starts_with("light_") || mode_.starts_with("nick_"))) &&
                                c.target == aid_target_
                                ? .6
                                : .17));
            }
    for (const auto &a : s.combatants)
    {
        const auto center =
            board_rect_.position + Vector2((a.cell.x + .5) * tile, (a.cell.y + .5) * tile);
        const auto color = a.side == 0 ? Color("79d6d4") : Color("dd9874");
        draw_circle(center, tile * .34, a.conscious ? color : Color("51595b"));
        if (a.id == s.actor && s.outcome == Outcome::ongoing)
            draw_arc(center, tile * .42, 0, 6.283185, 32, Color("e6c28a"), 2);
        if (art_.contains(a.id))
        {
            const auto texture = art_.at(a.id);
            const double scale = tile * .9 / std::max(texture->get_width(), texture->get_height());
            const Vector2 size(texture->get_width() * scale, texture->get_height() * scale);
            draw_texture_rect(texture, Rect2(center - size * .5, size), false,
                              a.conscious ? Color(1, 1, 1) : Color(.5, .5, .5));
        }
        else
        {
            const auto number = std::to_string(a.id);
            auto cursor = center + Vector2(-5.5 * number.size(), 7);
            for (const char digit : number)
            {
                draw_char(font, cursor, gs(std::string(1, digit)), 20, Color("142027"));
                cursor.x += 11;
            }
        }
        draw_rect(Rect2(center + Vector2(-tile * .35, tile * .38), Vector2(tile * .7, 4)),
                  Color("101719"));
        draw_rect(Rect2(center + Vector2(-tile * .35, tile * .38),
                        Vector2(tile * .7 * a.hit_points / a.max_hit_points, 4)),
                  color);
    }
}

void CombatView::_process(double delta)
{
    if (Engine::get_singleton()->is_editor_hint())
        return;
    try
    {
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
        if ((checking_ || expedition_check_) && capture_ && !captured_)
        {
            if (++completion_frames_ < 3)
                return;
            completion_frames_ = 0;
            const auto file =
                local_path(expedition_check_ ? "res://../../user-data/slums-battlefield.png"
                           : check_slums_    ? "res://../../user-data/slums-combat.png"
                           : "res://../../user-data/training-combat.png");
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
                        mouse->set_position(board_rect_.position +
                                            Vector2(target->cell.x + .5, target->cell.y + .5) *
                                            (board_rect_.size.x /
                                             demo_->combat().snapshot().battlefield.width));
                        get_viewport()->push_input(mouse, true);
                        if (demo_->combat().snapshot().revision != s.revision + 1)
                            throw std::runtime_error(
                                "Action button/target click did not submit command");
                        checked_input_ = true;
                        ++check_steps_;
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
