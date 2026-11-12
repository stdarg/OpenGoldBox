#ifndef OPENGOLDBOX_GAME_WALL_ART_H
#define OPENGOLDBOX_GAME_WALL_ART_H

#include "game_resources.h"
#include "unloadable_art.h"
#include "wall_art_files.h"
#include <optional>

// The new art's walls (walls/ in its folder), decoded on first use and again
// only when the player chooses another folder: every new or loaded game builds
// its session from them. None without the new art. A set that cannot be loaded
// is logged and shows the original.
inline const opengold::por::WallArtReplacements &game_wall_art()
{
    static std::optional<godot::String> loaded_from;
    static opengold::por::WallArtReplacements art;
    const auto folder = new_art_path("walls");
    if (loaded_from != folder)
    {
        art =
            folder.is_empty() ? opengold::por::WallArtReplacements{} : load_wall_art_files(folder);
        log_unloadable_art(art.problems);
        loaded_from = folder;
    }
    return art;
}

#endif
