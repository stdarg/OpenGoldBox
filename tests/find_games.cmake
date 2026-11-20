# The games' folders, found by their files as the game finds them (docs/TECH.md,
# "Finding the games' folders"): included by a check before it gives its game
# a home folder of its own, where the game's own search would find nothing.
#
#   GAME_FINDER  opengold_census, whose --find prints each game found
#
# Sets game_folder_<NAME> for each game found, NAME as game_folder names it
# (POOLRAD, CURSE, ...), and found_games_settings: settings.cfg lines naming
# the games other than Pool of Radiance, as if the player had chosen them.
# OPENGOLD_GAME_DIR is set to Pool of Radiance's folder, as the checks have
# always passed it on to the game. found_games_home is the home folder the
# games were found from: a native fixture writer run after HOME has moved gets
# it back (cmake -E env HOME=...), so it finds the same games.
get_filename_component(found_games_checkout "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(found_games_home "$ENV{HOME}")
execute_process(COMMAND "${GAME_FINDER}" --find "${found_games_checkout}"
    RESULT_VARIABLE found_games_result OUTPUT_VARIABLE found_games ERROR_VARIABLE found_games_errors)
if(NOT found_games_result STREQUAL "0")
    message(FATAL_ERROR "The games' folders could not be looked for (${found_games_result}): ${found_games_errors}")
endif()
set(found_games_settings "[games]\n")
string(REPLACE "\n" ";" found_games "${found_games}")
foreach(found_game IN LISTS found_games)
    if(found_game MATCHES "^([^\t]+)\t(.+)$")
        set(found_game_name "${CMAKE_MATCH_1}")
        file(TO_CMAKE_PATH "${CMAKE_MATCH_2}" "game_folder_${found_game_name}")
        if(NOT found_game_name STREQUAL "POOLRAD")
            string(APPEND found_games_settings
                "${found_game_name}=\"${game_folder_${found_game_name}}\"\n")
        endif()
    endif()
endforeach()
if(DEFINED game_folder_POOLRAD)
    set(ENV{OPENGOLD_GAME_DIR} "${game_folder_POOLRAD}")
endif()
