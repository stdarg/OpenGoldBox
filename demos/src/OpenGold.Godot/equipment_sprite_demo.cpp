#include "equipment_sprite_demo.h"
#include "../../../src/OpenGoldBox/godot_nodes.h"
#include "../../../src/OpenGoldBox/godot_images.h"
#include "opengold/srd5.h"
#include "../../../src/OpenGoldBox/godot_path.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <set>

using namespace godot;
using namespace opengold;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}

std::filesystem::path path(const String &s)
{
    return presentation::path_from_godot(s);
}
} // namespace

void EquipmentSpriteDemo::_ready()
{
    set_texture_filter(TEXTURE_FILTER_NEAREST);
    get_window()->set_min_size({1120, 800});
    for (const char *name :
            {"Title", "Help", "ReadyLabel", "ActionLabel", "Equipment", "Status"
            })
        presentation::add_control<Label>(*this, name, {});
    get_node<Label>("Title")->set_text("Equipment sprite demo");
    get_node<Label>("Title")->add_theme_font_size_override("font_size", 28);
    get_node<Label>("Help")->set_text(
        "Double-click a weapon (or press Enter) to equip. Toggle the shield separately.");
    get_node<Label>("ReadyLabel")->set_text("Ready");
    get_node<Label>("ActionLabel")->set_text("Action");
    for (const char *name :
            {"Equipment", "Status"
            })
        get_node<Label>(name)->set("autowrap_mode", 3);
    auto *list = presentation::add_control<ItemList>(*this, "Items", {});
    list->connect("item_selected", callable_mp(this, &EquipmentSpriteDemo::select));
    list->connect("item_activated", callable_mp(this, &EquipmentSpriteDemo::activate));
    auto *shield = presentation::add_control<Button>(*this, "Shield", {});
    shield->set_toggle_mode(true);
    shield->set_text("Shield: Off");
    shield->set_disabled(true);
    shield->connect("pressed", callable_mp(this, &EquipmentSpriteDemo::toggle_shield));
    for (bool equip :
            {
                true, false
            })
    {
        auto *button = presentation::add_control<Button>(*this, equip ? "Equip" : "Unequip", {});
        button->set_text(equip ? "Equip" : "Unequip");
        button->connect("pressed",
                        callable_mp(this, &EquipmentSpriteDemo::change_equipment).bind(equip));
        button->set_disabled(true);
    }
    for (const char *name :
            {"Ready", "Action"
            })
    {
        auto *preview = presentation::add_control<TextureRect>(*this, name, {});
        preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    }
    ready_ = true;
    layout();
    if (Engine::get_singleton()->is_editor_hint())
        return;
    try
    {
        auto directory = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
        if (directory.is_empty())
            directory =
                ProjectSettings::get_singleton()->get_setting("opengold/game_directory", "");
        art_ = por::CharacterArt::load(path(directory));
        const auto root = ProjectSettings::get_singleton()->globalize_path("res://../../data/art/");
        catalog_ = por::CombatBodyCatalog::load(path(root.path_join("combat-body-looks.tsv")),
                                                path(root.path_join("combat-weapon-options.tsv")));
        rules::CharacterDraft draft;
        draft.race = "human";
        draft.gender = "male";
        draft.character_class = "fighter";
        draft.alignment = "lawful_good";
        draft.background = "soldier";
        draft.name = "Equipment preview";
        draft.rolled = true;
        for (auto &roll : draft.rolls)
            roll = {{6, 5, 4, 1}, 3};
        por::CharacterAppearance appearance;
        appearance.tall = true;
        appearance.combat_body = 24;
        Character character(*srd5::character_rules(), draft, appearance);
        campaign_ = std::make_unique<CampaignParty>(
                        srd5::load(path(ProjectSettings::get_singleton()->globalize_path(
                                            "res://../../data/rules/srd-5.2.1/combat.rules"))));
        std::set<int> seen;
        const auto add = [&](int type, const std::string & name)
        {
            if (type < 1 || !seen.insert(type).second)
                return;
            por::Equipment item;
            item.stored.type = type;
            item.stored.stack_size = 1;
            character.inventory().add(equipment_conversion(item), name, 1, type);
        };
        for (const auto &option : catalog_.options)
            if (!option.id.ends_with("_shield"))
                add(option.original_type, option.label);
        add(59, "Shield");
        member_ = campaign_->add_pc(std::move(character));
        for (const auto &item : campaign_->member(member_).character.inventory().items())
            hands_.push_back(campaign_->equipment_info(member_, item.id).hands);
        refresh();
        list->grab_focus();
        get_node<Label>("Status")->set_text(
            "One of every weapon and one shield. This temporary preview does not save changes.");
    }
    catch (const std::exception &e)
    {
        member_ = 0;
        campaign_.reset();
        get_node<Label>("Status")->set_text(gs(e.what()));
    }
}

void EquipmentSpriteDemo::layout()
{
    if (!ready_)
        return;
    const auto w = get_size().x, h = get_size().y;
    const auto place = [&](const char *name, Rect2 rect)
    {
        auto *node = get_node<Control>(name);
        node->set_position(rect.position);
        node->set_size(rect.size);
    };
    place("Title", {24, 18, w - 48, 42});
    place("Help", {24, 66, w - 48, 32});
    place("Items", {24, 116, 380, h - 300});
    place("Shield", {24, h - 168, 380, 44});
    place("Equip", {24, h - 112, 182, 44});
    place("Unequip", {222, h - 112, 182, 44});
    const float half = (w - 464) / 2;
    place("ReadyLabel", {440, 116, half, 32});
    place("ActionLabel", {456 + half, 116, half, 32});
    place("Ready", {440, 166, half, 384});
    place("Action", {456 + half, 166, half, 384});
    place("Equipment", {440, 570, w - 464, 112});
    place("Status", {24, h - 60, w - 48, 52});
}

void EquipmentSpriteDemo::_notification(int what)
{
    if (what == NOTIFICATION_RESIZED)
        layout();
}

void EquipmentSpriteDemo::_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->is_ctrl_pressed() &&
            key->get_keycode() == KEY_X)
        get_tree()->quit();
}

void EquipmentSpriteDemo::select(std::int64_t index)
{
    selected_ = static_cast<int>(index);
    if (!member_)
        return;
    const auto &member = campaign_->member(member_);
    const auto item = member.character.inventory().items()[selected_];
    const bool equipped = std::ranges::find(member.equipped, item.id) != member.equipped.end();
    get_node<Button>("Equip")->set_disabled(equipped);
    get_node<Button>("Unequip")->set_disabled(!equipped);
}

void EquipmentSpriteDemo::activate(std::int64_t index)
{
    select(index);
    change_equipment(true);
}

void EquipmentSpriteDemo::toggle_shield()
{
    if (!member_)
        return;
    try
    {
        const auto &member = campaign_->member(member_);
        const auto shield = member.character.inventory().items().back();
        const bool equipped =
            std::ranges::find(member.equipped, shield.id) != member.equipped.end();
        if (equipped)
            campaign_->unequip(member_, shield.id);
        else
            campaign_->equip(member_, shield.id);
        get_node<Label>("Status")->set_text(equipped ? "Shield unequipped." : "Shield equipped.");
    }
    catch (const std::exception &error)
    {
        get_node<Label>("Status")->set_text(gs(error.what()));
    }
    refresh();
}

void EquipmentSpriteDemo::refresh()
{
    if (!member_)
        return;
    const auto &member = campaign_->member(member_);
    auto *list = get_node<ItemList>("Items");
    list->clear();
    const auto items = member.character.inventory().items();
    for (unsigned i = 0; i < items.size(); ++i)
    {
        const auto &item = items[i];
        const bool equipped = std::ranges::find(member.equipped, item.id) != member.equipped.end();
        if (item.original_type == 59)
        {
            auto *shield = get_node<Button>("Shield");
            shield->set_disabled(false);
            shield->set_pressed_no_signal(equipped);
            shield->set_text(equipped ? "Shield: On" : "Shield: Off");
            continue;
        }
        list->add_item(gs((equipped ? "[Equipped] " : "") + item.name + "  (" +
                          std::to_string(hands_[i]) + " hand" + (hands_[i] == 1 ? ")" : "s)")));
        Dictionary info;
        info["type"] = item.original_type;
        info["hands"] = hands_[i];
        info["equipped"] = equipped;
        list->set_item_metadata(i, info);
    }
    list->select(selected_);
    list->ensure_current_is_visible();
    const bool equipped =
        std::ranges::find(member.equipped, items[selected_].id) != member.equipped.end();
    get_node<Button>("Equip")->set_disabled(equipped);
    get_node<Button>("Unequip")->set_disabled(!equipped);
    const auto resolved = por::resolve_combat_appearance(member, catalog_);
    for (bool action :
            {
                false, true
            })
        get_node<TextureRect>(action ? "Action" : "Ready")
        ->set_texture(presentation::image_texture(resolved.icon(*art_, action)));
    set_meta("body", resolved.appearance.combat_body);
    set_meta("equipment_body", resolved.selection.body);
    get_node<Label>("Equipment")
    ->set_text(
        gs(resolved.selection.label + "\nCharacter body " +
           std::to_string(resolved.appearance.combat_body) +
           (resolved.selection.matched ? "" : " - No artwork assignment; showing unarmed.") +
           "\nOriginal pixels, enlarged. Both poses use the same scale."));
}

void EquipmentSpriteDemo::change_equipment(bool equip)
{
    if (!member_)
        return;
    try
    {
        const auto item = campaign_->member(member_).character.inventory().items()[selected_];
        if (equip)
            campaign_->equip(member_, item.id);
        else
            campaign_->unequip(member_, item.id);
        get_node<Label>("Status")->set_text(
            gs(item.name + (equip ? " equipped." : " unequipped.")));
        refresh();
    }
    catch (const std::exception &error)
    {
        get_node<Label>("Status")->set_text(gs(error.what()));
    }
}
