#ifndef OPENGOLDBOX_INVENTORY_LABEL_H
#define OPENGOLDBOX_INVENTORY_LABEL_H

#include "localization.h"
#include "opengold/campaign_party.h"
#include "opengold/inventory.h"
#include "opengold/scroll_key.h"
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

// The same for a member's item; an identified scroll also lists the spells
// still on it (MAGIC-6): "Clerical Scroll With 2 Spells x1: Greater Restoration".
inline godot::String inventory_label(const opengold::PartyMember &member,
                                     const opengold::InventoryItem &item)
{
    // A Wand of Secrets counts its charges down to 0 (SECRET-1).
    if (item.definition_id == "wand_of_secrets")
    {
        return i18n::plural("{item} ({count} charge)", "{item} ({count} charges)",
                            static_cast<int>(opengold::wand_charges(member, item.id)),
                            {{"item", i18n::text(item.name)}});
    }
    // The parsed key's spell names point into `key`.
    const auto key = opengold::rules_definition(member, item);
    const auto scroll = opengold::rules::parse_scroll_key(key);
    if (!scroll || scroll->unidentified)
    {
        return inventory_label(item);
    }
    godot::String spells;
    for (const auto &entry : scroll->spells)
    {
        spells += (spells.is_empty() ? "" : ", ") +
                  i18n::text(opengold::srd5::spell_label(entry.id));
    }
    return i18n::format(N_("{item}: {spells}"), {{"item", inventory_label(item)}, {"spells", spells}});
}
#endif
