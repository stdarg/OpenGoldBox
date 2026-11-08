# Every event of every map but New Phlan's and the Slums' through the game's
# town screen, from the saves opengold_expedition_tests writes one step from
# each event's squares (FIXTURES/area-events, a sample). Godot's user data
# goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
file(GLOB saves "${FIXTURES}/area-events/area*.ogs")
if(NOT saves)
    message(FATAL_ERROR "opengold_expedition_fixtures did not write the other maps' event fixtures")
endif()
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_SETTINGS_FILE} "${WORK}/home/settings.cfg")
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/area_events_tests.gd"
    "-DEXPECTED=Events passed" -DTEST_TIMEOUT=500
    "-DARGS=--fixtures=${FIXTURES}/area-events;--finish-fights;--full-branches=1"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "An event of another map misbehaves")
endif()
file(REMOVE_RECURSE "${WORK}")
