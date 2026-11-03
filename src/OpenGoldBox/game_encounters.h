#ifndef OPENGOLDBOX_GAME_ENCOUNTERS_H
#define OPENGOLDBOX_GAME_ENCOUNTERS_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/encounter_data.h"

// What original creature records fight as (data/encounters, which the
// Creature Stats Editor writes), read on first use. The shipped files do not
// change while the game runs.
inline const opengold::por::EncounterData &game_encounters()
{
    static const auto encounters = opengold::por::load_encounter_data(
        presentation::path_from_godot(game_data_path("data/encounters")));
    return encounters;
}

#endif
