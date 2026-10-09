# The UI audit with original files: opengold_expedition_tests writes fresh
# Slums saves, so the town, a story choice and a campaign fight are audited
# with the current rules, along with character creation and the party.
# Godot's user data goes to WORK.
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
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixtures}/hobgoblins.ogs")
    message(FATAL_ERROR "The expedition route did not write the Slums fixtures (${result})")
endif()

# The audit reads the saves from where the expedition test wrote them.
set(ENV{OPENGOLD_SLUMS_FIXTURES} "${fixtures}")
foreach(variable HOME APPDATA XDG_DATA_HOME)
    set(ENV{${variable}} "${WORK}/home")
endforeach()
set(ENV{OPENGOLD_LANG} en)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/ui_audit.gd"
    "-DEXPECTED=UI audit passed:.*creation-4.*party.*town.*story.*campaign-combat" -DTEST_TIMEOUT=240
    "-DARGS=--playtest-fixtures=${PLAYTEST_FIXTURES}"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "The UI audit failed")
endif()
file(REMOVE_RECURSE "${WORK}")
