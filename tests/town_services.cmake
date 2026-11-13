# New Phlan's shops, temples and inns through the game's town screen, from the
# saves opengold_expedition_tests writes one step from each door
# (FIXTURES/town). Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
if(NOT EXISTS "${FIXTURES}/town/shop-9-13.ogs")
    message(FATAL_ERROR "opengold_expedition_fixtures did not write the town fixtures")
endif()
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_SETTINGS_FILE} "${WORK}/home/settings.cfg")
# The shops as their data/shops records describe them: Original shops off
# (rogue_shop_tests checks the original's stock).
file(WRITE "${WORK}/home/settings.cfg" "[original_shops]\nPOOLRAD=\"off\"\n")
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/town_services_tests.gd"
    "-DEXPECTED=Town services passed" -DTEST_TIMEOUT=300
    "-DARGS=--fixtures=${FIXTURES}/town"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "New Phlan's shops, temples or inns misbehave")
endif()
file(REMOVE_RECURSE "${WORK}")
