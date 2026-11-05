# Every event of the Slums through the game's town screen, from the saves
# opengold_expedition_tests writes one step from each event's squares
# (FIXTURES/slums-events, a sample). Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
if(NOT EXISTS "${FIXTURES}/slums-events/slums-3-13-10-3-potion.ogs")
    message(FATAL_ERROR "opengold_expedition_fixtures did not write the Slums event fixtures")
endif()
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/area_events_tests.gd"
    "-DEXPECTED=Events passed" -DTEST_TIMEOUT=500
    "-DARGS=--fixtures=${FIXTURES}/slums-events;--finish-fights;--words=OHLO,XYZZY"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "An event of the Slums misbehaves")
endif()
file(REMOVE_RECURSE "${WORK}")
