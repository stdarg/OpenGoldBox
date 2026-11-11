#ifndef OPENGOLDBOX_GAME_COMBAT_LOOKS_H
#define OPENGOLDBOX_GAME_COMBAT_LOOKS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/combat_body_catalog.h"
#include "unloadable_art.h"
#include <optional>

// The party's combat looks (data/art/combat-body-looks.tsv and
// combat-weapon-options.tsv), read afresh each time. None when they cannot be
// loaded: the log names the files, and each member shows the original icon
// it was saved with.
inline std::optional<opengold::por::CombatBodyCatalog> game_combat_looks()
{
    auto loaded =
        opengold::por::load_combat_looks(presentation::path_from_godot(game_combat_body_file()),
                                         presentation::path_from_godot(game_combat_weapon_file()));
    if (!loaded.looks)
    {
        log_unloadable_art(std::string_view{loaded.problem});
    }
    return std::move(loaded.looks);
}

// The looks, or null for none.
inline const opengold::por::CombatBodyCatalog *
looks_or_none(const std::optional<opengold::por::CombatBodyCatalog> &looks)
{
    return looks ? &*looks : nullptr;
}

#endif
