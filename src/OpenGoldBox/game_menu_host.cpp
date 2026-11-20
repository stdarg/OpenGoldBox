// The creation scene's in-game menu: Escape that nothing else used opens it
// over the party screen, the exploration screen or a fight.
#include "character_creation_view.h"
#include "game_rules.h"
#include "application_settings.h"
#include "combat_view.h"
#include "game_menu_view.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "rolf_tour_view.h"
#include "town_sheet_dialog.h"
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>

using namespace godot;
using namespace opengold;
using presentation::required_node;

void CharacterCreationView::attach_game_menu()
{
    auto owned = presentation::instantiate_scene("res://scenes/game_menu.tscn");
    auto *menu = Object::cast_to<GameMenuView>(owned.get());
    if (!menu)
    {
        throw std::runtime_error("Invalid game menu scene");
    }
    menu->set_name("GameMenu");
    menu->connect("save_requested",
                  presentation::guarded(this, &CharacterCreationView::open_saves));
    menu->connect("quick_save_requested",
                  presentation::guarded(this, &CharacterCreationView::quick_save));
    menu->connect("quick_load_requested",
                  presentation::guarded(this, &CharacterCreationView::quick_load));
    menu->connect("main_menu_requested",
                  presentation::guarded(this, &CharacterCreationView::party_button_pressed)
                      .bind(static_cast<int>(PartyAction::return_to_party)));
    menu->connect("closed", presentation::guarded(this, &CharacterCreationView::game_menu_closed));
    menu->connect("art_changed", presentation::guarded(this, &CharacterCreationView::art_changed));
    menu->connect("new_art_changed",
                  presentation::guarded(this, &CharacterCreationView::new_art_changed));
    menu->connect("volume_changed",
                  presentation::guarded(this, &CharacterCreationView::volume_changed));
    presentation::attach_child(*this, std::move(owned));
}

void CharacterCreationView::_unhandled_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_unhandled_key(event);
                              });
}

// Escape opens the in-game menu, F5 quick saves and F9 quick loads, unless a
// dialog window open over the screen keeps the key for itself.
void CharacterCreationView::respond_to_unhandled_key(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
    {
        return;
    }
    const auto keycode = key->get_keycode();
    if (keycode != Key::KEY_ESCAPE && keycode != Key::KEY_F5 && keycode != Key::KEY_F9)
    {
        return;
    }
    for (int i = 0; i < get_child_count(); ++i)
    {
        if (const auto *window = Object::cast_to<Window>(get_child(i));
            window && window->is_visible())
        {
            return;
        }
    }
    // The menu, open, has Quick save and Quick load of its own.
    if (keycode != Key::KEY_ESCAPE && required_node<GameMenuView>(*this, "GameMenu").is_open())
    {
        return;
    }
    get_viewport()->set_input_as_handled();
    switch (keycode)
    {
    case Key::KEY_F5:
        quick_save();
        break;
    case Key::KEY_F9:
        quick_load();
        break;
    default:
        open_game_menu();
        break;
    }
}

CharacterCreationView::PlayState CharacterCreationView::play_state() const
{
    const auto *combat = Object::cast_to<Control>(get_node_or_null("CampaignCombat"));
    const auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    const bool exploring = town && town->is_visible();
    return {.fighting = (combat && combat->is_visible()) || (campaign_ && campaign_->in_combat()),
            .exploring = exploring,
            .idle = !exploring || town->can_leave()};
}

bool CharacterCreationView::may_save() const
{
    const auto play = play_state();
    const bool party = campaign_ && campaign_->selected();
    return party && !play.fighting && play.idle && !campaign_defeated_;
}

bool CharacterCreationView::may_load() const
{
    const auto play = play_state();
    return !play.fighting && (play.idle || campaign_defeated_);
}

void CharacterCreationView::open_game_menu()
{
    auto &menu = required_node<GameMenuView>(*this, "GameMenu");
    if (menu.is_open())
    {
        return;
    }
    const auto play = play_state();
    menu.open({.save = may_save(),
               .load = may_load(),
               .main_menu = play.exploring && !play.fighting && play.idle,
               .srd_rules = !campaign_ || game_rules_of(*campaign_).set == GameRules::Set::srd});
    // Nothing beneath moves or hears keys while the menu is open.
    paused_.clear();
    for (const char *name : {"CampaignTown", "CampaignCombat"})
    {
        if (auto *node = get_node_or_null(name))
        {
            paused_.push_back({name, node->is_processing(), node->is_processing_input()});
            node->set_process(false);
            node->set_process_input(false);
        }
    }
    move_child(&menu, get_child_count() - 1);
}

void CharacterCreationView::open_game_options()
{
    open_game_menu();
    required_node<GameMenuView>(*this, "GameMenu").show_options();
}

void CharacterCreationView::art_changed()
{
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->show_original_walls(settings::original_wall_art());
    }
    // The party's portraits, new or original.
    show_leader_change();
    if (auto *combat = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
    {
        combat->refresh_art();
    }
}

// Another new art folder: every screen reads its portraits again, and the game
// in progress is built again from itself with the new walls, close-ups and
// monsters, so what the player sees changes at once.
void CharacterCreationView::new_art_changed()
{
    reload_portraits();
    required_node<TownSheetDialog>(*this, "TownSheet").forget_portraits();
    auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    if (town)
    {
        town->forget_portraits();
    }
    // A design, which cannot be saved yet, keeps its session and its walls.
    if (campaign_ && !campaign_defeated_ && !campaign_->in_combat() && town && town->can_leave() &&
        town->saved_session())
    {
        const bool exploring = town->is_visible();
        reload_in_memory();
        error_ = String();
        if (exploring)
        {
            party_action(PartyAction::explore);
        }
    }
    art_changed();
}

void CharacterCreationView::volume_changed()
{
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->apply_sound_volume();
    }
    if (auto *combat = Object::cast_to<CombatView>(get_node_or_null("CampaignCombat")))
    {
        combat->apply_sound_volume();
    }
}

void CharacterCreationView::game_menu_closed()
{
    for (const auto &paused : paused_)
    {
        if (auto *node = get_node_or_null(paused.node))
        {
            node->set_process(paused.process);
            node->set_process_input(paused.input);
        }
    }
    paused_.clear();
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
    {
        town->apply_settings();
    }
}
