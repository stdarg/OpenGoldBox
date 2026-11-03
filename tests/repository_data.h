#ifndef OPENGOLD_REPOSITORY_DATA_H
#define OPENGOLD_REPOSITORY_DATA_H

#include "opengold/companions.h"
#include "opengold/encounter_data.h"
#include <filesystem>
#include <vector>

// The repository's data/encounters, which the game ships.
inline const opengold::por::EncounterData &repository_encounters()
{
    static const auto encounters = opengold::por::load_encounter_data(
        std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/encounters");
    return encounters;
}

// The repository's data/companions, which the game ships.
inline const std::vector<opengold::CompanionDefinition> &repository_companions()
{
    static const auto companions =
        opengold::load_companions(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/companions");
    return companions;
}

#endif
