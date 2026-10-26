// The creation scene's in-game menu: Escape that nothing else used opens it
// over the party screen, the exploration screen or a fight.
#include "character_creation_view.h"
#include "combat_view.h"
#include "game_menu_view.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "rolf_tour_view.h"
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
    menu->connect("main_menu_requested",
                  presentation::guarded(this, &CharacterCreationView::party_button_pressed)
                      .bind(static_cast<int>(PartyAction::return_to_party)));
    menu->connect("closed", presentation::guarded(this, &CharacterCreationView::game_menu_closed));
    presentation::attach_child(*this, std::move(owned));
}

void CharacterCreationView::_unhandled_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  const Ref<InputEventKey> key = event;
                                  if (key.is_null() || !key->is_pressed() || key->is_echo() ||
                                      key->get_keycode() != Key::KEY_ESCAPE)
                                  {
                                      return;
                                  }
                                  // A dialog window open over the screen keeps Escape for itself.
                                  for (int i = 0; i < get_child_count(); ++i)
                                  {
                                      if (const auto *window =
                                              Object::cast_to<Window>(get_child(i));
                                          window && window->is_visible())
                                      {
                                          return;
                                      }
                                  }
                                  get_viewport()->set_input_as_handled();
                                  open_game_menu();
                              });
}

void CharacterCreationView::open_game_menu()
{
    auto &menu = required_node<GameMenuView>(*this, "GameMenu");
    if (menu.is_open())
    {
        return;
    }
    const auto *combat = Object::cast_to<Control>(get_node_or_null("CampaignCombat"));
    const auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
    const bool fighting = (combat && combat->is_visible()) || (campaign_ && campaign_->in_combat());
    const bool exploring = town && town->is_visible();
    const bool idle = !exploring || town->can_leave();
    const bool party = campaign_ && campaign_->selected();
    menu.open({.save = party && !fighting && idle && !campaign_defeated_,
               .load = !fighting && (idle || campaign_defeated_),
               .main_menu = exploring && !fighting && idle});
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
