#ifndef OPENGOLDBOX_INVENTORY_LABEL_H
#define OPENGOLDBOX_INVENTORY_LABEL_H

#include "localization.h"
#include "opengold/inventory.h"
#include "opengold/srd5.h"

// An inventory line's name and count: "Dagger x2", or for one item holding
// several charges or missiles, "Wand of Magic Missiles (23 charges)" (MAGIC-3).
inline godot::String inventory_label(const opengold::InventoryItem &item)
{
    const auto noun = opengold::srd5::charge_noun(item.definition_id);
    const auto count = static_cast<int>(item.quantity);
    const auto name = i18n::text(item.name);
    if (noun == "charges")
    {
        return i18n::plural("{item} ({count} charge)", "{item} ({count} charges)", count,
                            {{"item", name}});
    }
    if (noun == "missiles")
    {
        return i18n::plural("{item} ({count} missile)", "{item} ({count} missiles)", count,
                            {{"item", name}});
    }
    return i18n::format(N_("{item} x{quantity}"), {{"item", name}, {"quantity", count}});
}
#endif
