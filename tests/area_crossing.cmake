# A crossing between areas through the game's own controls, with original files:
# opengold_expedition_tests writes the campaign saved at New Phlan's west gate, a
# fresh Godot game loads it, walks into the Slums and back and saves, and the
# native suite checks the save the game wrote.
# Godot's user data goes to WORK so a player's saves and settings are untouched.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home" "${WORK}/out")
set(fixture "${WORK}/west-gate.ogs")

set(ENV{OPENGOLD_GATE_FIXTURE} "${fixture}")
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_GATE_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The expedition route did not write the gate fixture (${result})")
endif()

foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/area_crossing_tests.gd"
    "-DEXPECTED=Area crossing route passed" -DTEST_TIMEOUT=120
    "-DARGS=--gate-fixture=${fixture};--gate-output=${WORK}/out"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The game did not walk into the Slums and back from the fixture")
endif()

execute_process(COMMAND "${EXPEDITION}" --verify-gate-crossing "${fixture}" "${WORK}/out/crossed.ogs"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The game's save after the crossing does not hold the town as it was")
endif()
file(REMOVE_RECURSE "${WORK}")
