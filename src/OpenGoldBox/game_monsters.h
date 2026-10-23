#ifndef OPENGOLDBOX_GAME_MONSTERS_H
#define OPENGOLDBOX_GAME_MONSTERS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/monster_bundle.h"
#include "wall_art_files.h"
#include <memory>

// The Combat Sprite Catalog's export (data/monsters, #274), decoded on first
// use. The shipped files do not change while the game runs, and every new or
// loaded game shares them.
inline std::shared_ptr<const opengold::por::MonsterBundle> game_monsters()
{
    static const auto monsters = std::make_shared<const opengold::por::MonsterBundle>(
        opengold::por::load_monster_bundle(
            presentation::path_from_godot(game_data_path("data/monsters")), decode_wall_png));
    return monsters;
}

#endif
