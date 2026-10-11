#ifndef OPENGOLDBOX_NICK_CONTROLS_H
#define OPENGOLDBOX_NICK_CONTROLS_H
#include "godot_nodes.h"
#include "localization.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>

namespace presentation
{
template <class Text>
void setup_nick(godot::Node &root, Text text, const godot::Callable &open,
                const godot::Callable &chosen, const godot::Callable &target,
                const godot::Callable &cancel, const godot::Callable &input)
{
    using namespace godot;
    auto *button = &required_node<Button>(root, "Nick");
    button->set_text(text(N_("Nick attack")));
    button->hide();
    button->connect("pressed", open);
    auto owned = make_node<Window>();
    owned->set_name("NickAttack");
    owned->set_title(text(N_("Nick attack")));
    attach_dialog_layout(*owned);
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *dialog = attach_child(root, std::move(owned));
    dialog->connect("close_requested", cancel);
    dialog->connect("window_input", input);
    auto *explanation = dialog_control<Label>(*dialog, "Text");
    explanation->set_text(text(N_(
                                   "Choose a Nick weapon and attack. This uses the Light extra attack for this turn and does not spend your Bonus Action.")));
    explanation->set("autowrap_mode", 3);
    auto *label = dialog_control<Label>(*dialog, "Label");
    label->set_text(text(N_("Weapon and attack")));
    auto *choices = dialog_control<OptionButton>(*dialog, "Choices");
    choices->set_fit_to_longest_item(false);
    choices->set_clip_text(true);
    choices->connect("item_selected", chosen);
    auto *no = dialog_control<Button>(*dialog, "Cancel");
    no->set_text(text(N_("Cancel")));
    no->connect("pressed", cancel);
    auto *yes = dialog_control<Button>(*dialog, "Target");
    yes->set_text(text(N_("Target")));
    yes->connect("pressed", target);
}

template <class Render>
void refresh_nick(godot::Node &root, const opengold::rules::CombatantView *actor,
                  bool available_turn, Render render)
{
    using namespace godot;
    auto *button = &required_node<Button>(root, "Nick");
    button->set_visible(actor && actor->nick_mastery);
    bool available = false;
    if (actor)
        for (const auto &attack : actor->nick_attacks)
            available |= attack.available;
    button->set_disabled(!available_turn || !available);
    auto *choices = &required_node<OptionButton>(root, "NickAttack/Choices");
    const String old = choices->get_selected() >= 0
                       ? String(choices->get_item_metadata(choices->get_selected()))
                       : String();
    choices->clear();
    int first = -1, selected = -1;
    if (actor)
        for (const auto &attack : actor->nick_attacks)
        {
            const int index = choices->get_item_count();
            const auto label = render(attack.label);
            choices->add_item(label);
            choices->set_item_tooltip(index, label);
            choices->set_item_metadata(
                index, String::utf8((attack.verb + "#" + std::to_string(attack.item)).c_str()));
            choices->set_item_disabled(index, !attack.available);
            if (attack.available)
            {
                if (first < 0)
                    first = index;
                if (String(choices->get_item_metadata(index)) == old)
                    selected = index;
            }
        }
    choices->select(selected >= 0 ? selected : first);
    choices->set_disabled(!available_turn || !available);
    required_node<Button>(root, "NickAttack/Target")
    .set_disabled(!available_turn || !available || choices->get_selected() < 0 ||
                  choices->is_item_disabled(choices->get_selected()));
    if (required_node<Window>(root, "NickAttack").is_visible() && (!available_turn || !available))
        required_node<Window>(root, "NickAttack").hide();
}
} // namespace presentation
#endif
