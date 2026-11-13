# Curse's Add Character from Pool of Radiance on the party screen
# (curse_from_pool_tests.gd). Skipped unless OPENGOLD_GAME_DIR holds Pool of
# Radiance and OPENGOLD_GOLDBOX_DIR holds the CURSE folder.
#
# FIXTURE_WRITER (opengold_original_creation_tests) writes the Pool of
# Radiance save the check brings characters from; the check runs with ARGS and
# its own settings.cfg and user data under WORK.
if("$ENV{OPENGOLD_GAME_DIR}" STREQUAL "" OR NOT EXISTS "$ENV{OPENGOLD_GOLDBOX_DIR}/CURSE")
    message("Skipped: OPENGOLD_GAME_DIR or OPENGOLD_GOLDBOX_DIR's CURSE folder is not set")
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
file(WRITE "${settings}" "[game]\npath=\"${game_folder}\"\n\n[interface]\nlanguage=\"en\"\n")

set(fixture "${WORK}/pool-veterans.ogs")
set(ENV{OPENGOLD_POOL_SAVE_FIXTURE} "${fixture}")
execute_process(COMMAND "${FIXTURE_WRITER}" RESULT_VARIABLE result)
unset(ENV{OPENGOLD_POOL_SAVE_FIXTURE})
if(NOT result STREQUAL "0" OR NOT EXISTS "${fixture}")
    message(FATAL_ERROR "The Pool of Radiance save was not written (${result})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGODOT=${GODOT}" "-DPROJECT=${PROJECT}"
    "-DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/curse_from_pool_tests.gd" "-DEXPECTED=${EXPECTED}"
    "-DARGS=--pool-save=${fixture};${ARGS}" -DTEST_TIMEOUT=150
    -P "${CMAKE_CURRENT_LIST_DIR}/run_godot_test.cmake"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Godot check failed: curse_from_pool_tests")
endif()
