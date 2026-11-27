# Each balance tool plays its fights with the game's own setup. Original data only.
include("${CMAKE_CURRENT_LIST_DIR}/find_games.cmake")
if(NOT DEFINED game_folder_POOLRAD)
    message("Skipped: Pool of Radiance's files are not here")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${SIM}" "$ENV{OPENGOLD_GAME_DIR}" 1 "${WORK}" RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "opengold_campaign_sim failed (${result}): ${errors}")
endif()
# Six Sorcerers run out of low slots: a Shield once spent a level-two slot they lacked.
execute_process(COMMAND "${SIM}" "$ENV{OPENGOLD_GAME_DIR}" 3 "${WORK}" six-sorcerer
    RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "opengold_campaign_sim six-sorcerer failed (${result}): ${errors}")
endif()
# The troll arena stages its own areas: the no-fire party plays one loadout at every width.
execute_process(COMMAND "${SIM}" --troll-arena "$ENV{OPENGOLD_GAME_DIR}" 1 "${WORK}" no-fire
    RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "opengold_campaign_sim --troll-arena failed (${result}): ${errors}")
endif()
execute_process(COMMAND "${BALANCE}" "$ENV{OPENGOLD_GAME_DIR}" 1 RESULT_VARIABLE result ERROR_VARIABLE errors OUTPUT_QUIET)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "opengold_encounter_balance failed (${result}): ${errors}")
endif()
