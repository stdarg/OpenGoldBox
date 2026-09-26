#ifndef OPENGOLDBOX_GRIP_CONTROL_H
#define OPENGOLDBOX_GRIP_CONTROL_H
#include "localization.h"
#include "opengold/rules.h"
#include <godot_cpp/classes/option_button.hpp>

namespace presentation
{
inline void refresh_grip(godot::OptionButton &control,
                         const opengold::rules::EquipmentState &equipment,
                         const std::vector<opengold::rules::GripOption> &choices,
                         bool enabled = true)
{
    control.clear();
    control.set_disabled(!enabled || choices.empty());
    control.set_focus_mode(godot::Control::FOCUS_ALL);
    for (const auto &choice : choices)
    {
        const auto index = control.get_item_count();
        control.add_item(i18n::render(choice.label), choice.hands);
        control.set_item_disabled(index, !choice.available);
        if (choice.hands == equipment.weapon_hands)
            control.select(index);
    }
    if (choices.empty())
    {
        control.add_item(i18n::text(N_("No Versatile weapon")));
        control.select(0);
    }
    control.set_tooltip_text(i18n::text(N_(
        "Grip of the equipped weapon. Two hands require a free hand; thrown attacks use the one-handed damage die.")));
}
} // namespace presentation
#endif
