#ifndef OPENGOLDBOX_ALLY_ATTACK_CONTROLS_H
#define OPENGOLDBOX_ALLY_ATTACK_CONTROLS_H
#include "godot_nodes.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>

namespace presentation
{
// The original's "Attack Ally" question, asked before a member under the
// player's command attacks its own side: yes and the party's companions and
// the creatures beside it turn against it.
template <class Text>
void setup_ally_attack(godot::Node &root, Text text, const godot::Callable &attack,
                       const godot::Callable &spare, const godot::Callable &input)
{
    using namespace godot;
    auto owned = make_node<Window>();
    owned->set_name("AttackAlly");
    attach_dialog_layout(*owned);
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = attach_child(root, std::move(owned));
    w->set_title(text(N_("Attack Ally")));
    w->connect("close_requested", spare);
    w->connect("window_input", input);
    dialog_control<Label>(*w, "Text");
    auto *no = dialog_control<Button>(*w, "Spare");
    no->set_text(text(N_("Cancel")));
    no->connect("pressed", spare);
    auto *yes = dialog_control<Button>(*w, "Attack");
    yes->set_text(text(N_("Attack")));
    yes->connect("pressed", attack);
}
} // namespace presentation
#endif
