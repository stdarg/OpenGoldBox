# The Slums' four orcs with original files: opengold_expedition_tests writes the
# party beside them, and a fresh Godot game loads it through Load game and
# fights them until one shoots. Godot's user data goes to WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "")
    message("Skipped: OPENGOLD_GAME_DIR is not set")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
set(fixtures "${WORK}/fixtures")

set(ENV{OPENGOLD_SLUMS_FIXTURES} "${fixtures}")
execute_process(COMMAND "${EXPEDITION}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_SLUMS_FIXTURES})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixtures}/orcs.ogs")
    message(FATAL_ERROR "The expedition route did not write the Slums fixtures (${result})")
endif()

foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/weapon_set_tests.gd"
    "-DEXPECTED=Weapon set checks passed" -DTEST_TIMEOUT=180
    "-DARGS=--slums-fixture=${fixtures}/orcs.ogs"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The weapon set checks failed")
endif()
file(REMOVE_RECURSE "${WORK}")
