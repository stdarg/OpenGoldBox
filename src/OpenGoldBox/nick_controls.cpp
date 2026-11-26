#include "nick_controls.h"
#include "godot_nodes.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/window.hpp>
#include <string>

namespace presentation
{
void setup_nick(godot::Node &root, const godot::Callable &open, const godot::Callable &chosen,
                const godot::Callable &target, const godot::Callable &cancel,
                const godot::Callable &input)
{
    using namespace godot;
    auto *button = &required_node<Button>(root, "Nick");
    button->set_text(i18n::text(N_("Nick attack")));
    button->hide();
    button->connect("pressed", open);
    auto *dialog = attach_modal_dialog(root, "NickAttack");
    dialog->set_title(i18n::text(N_("Nick attack")));
    dialog->connect("close_requested", cancel);
    dialog->connect("window_input", input);
    auto *explanation = dialog_control<Label>(*dialog, "Text");
    explanation->set_text(i18n::text(N_(
        "Choose a Nick weapon and attack. This uses the Light extra attack for this turn and does not spend your Bonus Action.")));
    auto *label = dialog_control<Label>(*dialog, "Label");
    label->set_text(i18n::text(N_("Weapon and attack")));
    auto *choices = dialog_control<OptionButton>(*dialog, "Choices");
    choices->connect("item_selected", chosen);
    auto *no = dialog_control<Button>(*dialog, "Cancel");
    no->set_text(i18n::text(N_("Cancel")));
    no->connect("pressed", cancel);
    auto *yes = dialog_control<Button>(*dialog, "Target");
    yes->set_text(i18n::text(N_("Target")));
    yes->connect("pressed", target);
}

void refresh_nick(godot::Node &root, const opengold::rules::CombatantView *actor,
                  bool available_turn)
{
    using namespace godot;
    auto *button = &required_node<Button>(root, "Nick");
    button->set_visible(actor && actor->nick_mastery);
    bool available = false;
    if (actor)
    {
        for (const auto &attack : actor->nick_attacks)
        {
            available |= attack.available;
        }
    }
    button->set_disabled(!available_turn || !available);
    auto *choices = &required_node<OptionButton>(root, "NickAttack/Choices");
    const String old = choices->get_selected() >= 0
                           ? String(choices->get_item_metadata(choices->get_selected()))
                           : String();
    choices->clear();
    int first = -1, selected = -1;
    if (actor)
    {
        for (const auto &attack : actor->nick_attacks)
        {
            const int index = choices->get_item_count();
            const auto label = i18n::render(attack.label);
            choices->add_item(label);
            choices->set_item_tooltip(index, label);
            choices->set_item_metadata(
                index, String::utf8((attack.verb + "#" + std::to_string(attack.item)).c_str()));
            choices->set_item_disabled(index, !attack.available);
            if (attack.available)
            {
                if (first < 0)
                {
                    first = index;
                }
                if (String(choices->get_item_metadata(index)) == old)
                {
                    selected = index;
                }
            }
        }
    }
    choices->select(selected >= 0 ? selected : first);
    choices->set_disabled(!available_turn || !available);
    required_node<Button>(root, "NickAttack/Target")
        .set_disabled(!available_turn || !available || choices->get_selected() < 0 ||
                      choices->is_item_disabled(choices->get_selected()));
    if (required_node<Window>(root, "NickAttack").is_visible() && (!available_turn || !available))
    {
        required_node<Window>(root, "NickAttack").hide();
    }
}
} // namespace presentation
