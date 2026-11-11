#ifndef OPENGOLDBOX_GAME_MONSTERS_H
#define OPENGOLDBOX_GAME_MONSTERS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/monster_bundle.h"
#include "unloadable_art.h"
#include "wall_art_files.h"
#include <memory>

// The Combat Sprite Catalog's export (data/monsters, #274), decoded on first
// use. The shipped files do not change while the game runs, and every new or
// loaded game shares them. A creature that cannot be loaded is logged once and
// shows the original.
inline std::shared_ptr<const opengold::por::MonsterBundle> game_monsters()
{
    static const auto monsters = []
    {
        auto loaded = opengold::por::load_monster_bundle(
            presentation::path_from_godot(game_data_path("data/monsters")), decode_wall_png);
        log_unloadable_art(loaded.problems);
        return std::make_shared<const opengold::por::MonsterBundle>(std::move(loaded));
    }();
    return monsters;
}

#endif
