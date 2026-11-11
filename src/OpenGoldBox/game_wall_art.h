#ifndef OPENGOLDBOX_GAME_WALL_ART_H
#define OPENGOLDBOX_GAME_WALL_ART_H

#include "game_resources.h"
#include "unloadable_art.h"
#include "wall_art_files.h"

// The game's new wall art (data/art/walls), decoded on first use. The shipped
// files do not change while the game runs, and every new or loaded game builds
// its session from them, so they are decoded only once. A set that cannot be
// loaded is logged once and shows the original.
inline const opengold::por::WallArtReplacements &game_wall_art()
{
    static const auto art = []
    {
        auto loaded = load_wall_art_files(game_data_path("data/art/walls"));
        log_unloadable_art(loaded.problems);
        return loaded;
    }();
    return art;
}

#endif
