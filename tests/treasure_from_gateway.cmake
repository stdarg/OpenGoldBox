# Treasures' characters from Gateway on the party screen
# (treasure_from_gateway_tests.gd). Skipped unless Pool of Radiance's,
# Treasures' and Gateway's files are found (find_games.cmake).
#
# FIXTURE_WRITER (opengold_treasure_tests) writes the Gateway save the check
# brings a character from; the check runs with the Gateway's-money option on
# and its own settings.cfg and user data under WORK, passing ARGS on.
include("${CMAKE_CURRENT_LIST_DIR}/find_games.cmake")
if(NOT DEFINED game_folder_POOLRAD OR NOT DEFINED game_folder_TREASURE
   OR NOT DEFINED game_folder_GATEWAY)
    message("Skipped: Pool of Radiance's, Treasures' or Gateway's files are not here")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(settings "${WORK}/home/settings.cfg")
set(ENV{OPENGOLD_SETTINGS_FILE} "${settings}")
set(ENV{OPENGOLD_LANG} en)
file(TO_CMAKE_PATH "$ENV{OPENGOLD_GAME_DIR}" game_folder)
file(WRITE "${settings}"
     "[game]\npath=\"${game_folder}\"\n\n${found_games_settings}\n[interface]\nlanguage=\"en\"\n\n[gateway_money]\nTREASURE=\"on\"\n")

set(fixture "${WORK}/gateway-veterans.ogs")
set(ENV{OPENGOLD_GATEWAY_SAVE_FIXTURE} "${fixture}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "HOME=${found_games_home}" "${FIXTURE_WRITER}"
    RESULT_VARIABLE result)
unset(ENV{OPENGOLD_GATEWAY_SAVE_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The Gateway save was not written (${result})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/treasure_from_gateway_tests.gd" "-DEXPECTED=${EXPECTED}"
    "-DARGS=--gateway-save=${fixture};${ARGS}" -DTEST_TIMEOUT=200
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: treasure_from_gateway_tests")
endif()
