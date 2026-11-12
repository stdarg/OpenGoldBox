#ifndef OPENGOLDBOX_GAME_MONSTERS_H
#define OPENGOLDBOX_GAME_MONSTERS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/monster_bundle.h"
#include "unloadable_art.h"
#include "wall_art_files.h"
#include <memory>
#include <optional>

// The Combat Sprite Catalog's export (#274): the creatures' names, gear and
// sizes from data/monsters, with their weapon sets' art from the new art's
// monsters/ folder. Decoded on first use and again only when the player
// chooses another folder; every new or loaded game shares them. Art that
// cannot be loaded is logged and shows the original.
inline std::shared_ptr<const opengold::por::MonsterBundle> game_monsters()
{
    static std::optional<godot::String> loaded_from;
    static std::shared_ptr<const opengold::por::MonsterBundle> monsters;
    const auto art = new_art_path("monsters");
    if (loaded_from != art)
    {
        auto loaded = opengold::por::load_monster_bundle(
            presentation::path_from_godot(game_data_path("data/monsters")),
            art.is_empty() ? std::filesystem::path{} : presentation::path_from_godot(art),
            decode_wall_png);
        log_unloadable_art(loaded.problems);
        monsters = std::make_shared<const opengold::por::MonsterBundle>(std::move(loaded));
        loaded_from = art;
    }
    return monsters;
}

#endif
