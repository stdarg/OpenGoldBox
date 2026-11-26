#include "ally_attack_controls.h"
#include "godot_nodes.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>

namespace presentation
{
void setup_ally_attack(godot::Node &root, const godot::Callable &attack,
                       const godot::Callable &spare, const godot::Callable &input)
{
    using namespace godot;
    auto *w = attach_modal_dialog(root, "AttackAlly");
    w->set_title(i18n::text(N_("Attack Ally")));
    w->connect("close_requested", spare);
    w->connect("window_input", input);
    dialog_control<Label>(*w, "Text");
    auto *no = dialog_control<Button>(*w, "Spare");
    no->set_text(i18n::text(N_("Cancel")));
    no->connect("pressed", spare);
    auto *yes = dialog_control<Button>(*w, "Attack");
    yes->set_text(i18n::text(N_("Attack")));
    yes->connect("pressed", attack);
}
} // namespace presentation
