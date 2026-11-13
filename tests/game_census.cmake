# The reading census (docs/GAMES.md, phase 1): every file of each game whose
# census is finished must decode. OPENGOLD_GOLDBOX_DIR names the folder that
# holds the games' folders (POOLRAD, CURSE, ...); OPENGOLD_GAME_DIR's Pool of
# Radiance is checked too. Skipped when neither is set.
# A game joins this list when its census decodes every file.
set(complete_games POOLRAD CURSE SECRET GATEWAY DARKNESS TREASURE UA)
set(folders)
if(NOT "$ENV{OPENGOLD_GOLDBOX_DIR}" STREQUAL "")
    foreach(game IN LISTS complete_games)
        if(EXISTS "$ENV{OPENGOLD_GOLDBOX_DIR}/${game}")
            list(APPEND folders "$ENV{OPENGOLD_GOLDBOX_DIR}/${game}")
        endif()
    endforeach()
endif()
if(NOT "$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    list(APPEND folders "$ENV{OPENGOLD_GAME_DIR}")
endif()
if(NOT folders)
    message("Skipped: neither OPENGOLD_GOLDBOX_DIR nor OPENGOLD_GAME_DIR is set")
    return()
endif()
execute_process(COMMAND "${CENSUS}" --require-complete ${folders} RESULT_VARIABLE result
                OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("${output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The census found files that do not decode: ${errors}")
endif()
