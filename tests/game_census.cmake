# The reading census (docs/GAMES.md, phase 1): every file of each game whose
# census is finished must decode. Each game's folder is the one the game finds
# by its files (find_games.cmake, with CENSUS as the finder). Skipped when
# none is found.
# A game joins this list when its census decodes every file.
set(complete_games POOLRAD CURSE SECRET GATEWAY DARKNESS TREASURE UA
    buck-rogers-countdown-to-doomsday buck-rogers-matrix-cubed)
set(GAME_FINDER "${CENSUS}")
include("${CMAKE_CURRENT_LIST_DIR}/find_games.cmake")
set(folders)
foreach(game IN LISTS complete_games)
    if(DEFINED game_folder_${game})
        list(APPEND folders "${game_folder_${game}}")
    endif()
endforeach()
if(NOT folders)
    message("Skipped: no game's files are here")
    return()
endif()
execute_process(COMMAND "${CENSUS}" --require-complete ${folders} RESULT_VARIABLE result
                OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("${output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The census found files that do not decode: ${errors}")
endif()
