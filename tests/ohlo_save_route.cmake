# Ohlo's quest across processes and the game's save controls, with original files:
# opengold_expedition_tests writes the campaign saved at Ohlo's door, a fresh Godot
# game loads it through Load game, hands in the potion and saves, then loads that
# save and revisits Ohlo; the native suite checks both saves the game wrote.
# Godot's user data goes to WORK so a player's saves and settings are untouched.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home" "${WORK}/out")
set(fixture "${WORK}/ohlo-door.ogs")

set(ENV{OPENGOLD_OHLO_FIXTURE} "${fixture}")
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_OHLO_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The expedition route did not write Ohlo's fixture (${result})")
endif()

foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/ohlo_save_route_tests.gd"
    "-DEXPECTED=Ohlo save route passed" -DTEST_TIMEOUT=120
    "-DARGS=--ohlo-fixture=${fixture};--ohlo-output=${WORK}/out"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The game did not complete Ohlo's hand-in from the fixture")
endif()

foreach(save handed-in revisited)
    execute_process(COMMAND "${EXPEDITION}" --verify-hand-in "${fixture}" "${WORK}/out/${save}.ogs"
        RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "The game's ${save} save does not hold one completed reward")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
