#ifndef OPENGOLDBOX_GAME_COMPANIONS_H
#define OPENGOLDBOX_GAME_COMPANIONS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/companions.h"
#include <vector>

// The original's NPCs who join the party (data/companions, which the Creature
// Stats Editor writes), read on first use. The shipped file does not change
// while the game runs.
inline const std::vector<opengold::CompanionDefinition> &game_companions()
{
    static const auto companions =
        opengold::load_companions(presentation::path_from_godot(game_data_path("data/companions")));
    return companions;
}

#endif
